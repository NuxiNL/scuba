// Copyright (c) 2017 Nuxi, https://nuxi.nl/
//
// This file is distributed under a 2-clause BSD license.
// See the LICENSE file for details.

#include <sys/procdesc.h>

#include <fcntl.h>
#include <program.h>
#include <signal.h>
#include <unistd.h>

#include <algorithm>
#include <cassert>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <fstream>
#include <functional>
#include <iomanip>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>

#include <google/protobuf/map.h>
#include <k8s.io/kubernetes/pkg/kubelet/apis/cri/v1alpha1/runtime/api.pb.h>
#include <yaml-cpp/exceptions.h>
#include <argdata.hpp>

#include <scuba/runtime_service/iso8601_timestamp.h>
#include <scuba/runtime_service/pod_sandbox.h>
#include <scuba/runtime_service/yaml_argdata_factory.h>
#include <scuba/runtime_service/yaml_builder.h>
#include <scuba/runtime_service/yaml_canonicalizing_factory.h>
#include <scuba/runtime_service/yaml_error_factory.h>
#include <scuba/runtime_service/yaml_file_descriptor_factory.h>
#include <scuba/util/fd_streambuf.h>

using arpc::FileDescriptor;
using flower::protocol::switchboard::Switchboard;
using google::protobuf::Map;
using runtime::ContainerConfig;
using runtime::ContainerState;
using runtime::ContainerStatus;
using runtime::PodSandboxMetadata;
using scuba::runtime_service::Container;
using scuba::util::fd_streambuf;

Container::Container(const ContainerConfig& config)
    : metadata_(config.metadata()),
      image_(config.image()),
      creation_time_(std::chrono::system_clock::now()),
      labels_(config.labels()),
      annotations_(config.annotations()),
      mounts_(config.mounts()),
      log_path_(config.log_path()),
      argdata_(config.argdata()),
      container_state_(ContainerState::CONTAINER_CREATED) {
}

void Container::GetInfo(runtime::Container* info) {
  *info->mutable_metadata() = metadata_;
  *info->mutable_image() = image_;
  info->set_image_ref(image_.image());
  info->set_state(GetContainerState_());
  info->set_created_at(
      std::chrono::nanoseconds(creation_time_.time_since_epoch()).count());
  *info->mutable_labels() = labels_;
  *info->mutable_annotations() = annotations_;
}

void Container::GetStatus(ContainerStatus* status) {
  *status->mutable_metadata() = metadata_;
  ContainerState state = GetContainerState_();
  status->set_state(state);
  switch (state) {
    case ContainerState::CONTAINER_EXITED:
      status->set_finished_at(
          std::chrono::nanoseconds(finish_time_.time_since_epoch()).count());
      status->set_exit_code(exit_code_);
      [[fallthrough]];
    case ContainerState::CONTAINER_RUNNING:
      status->set_started_at(
          std::chrono::nanoseconds(start_time_.time_since_epoch()).count());
      [[fallthrough]];
    case ContainerState::CONTAINER_CREATED:
      status->set_created_at(
          std::chrono::nanoseconds(creation_time_.time_since_epoch()).count());
      break;
    default:
      assert(0 && "Container cannot be in an unknown state");
  }
  *status->mutable_image() = image_;
  status->set_image_ref(image_.image());
  // TODO(ed): reason, message.
  *status->mutable_labels() = labels_;
  *status->mutable_annotations() = annotations_;
  *status->mutable_mounts() = mounts_;
  status->set_log_path(log_path_);
}

bool Container::MatchesFilter(std::optional<ContainerState> state,
                              const Map<std::string, std::string>& labels) {
  return (!state || *state == GetContainerState_()) &&
         std::includes(labels_.begin(), labels_.end(), labels.begin(),
                       labels.end(),
                       std::less<std::pair<std::string, std::string>>());
}

void Container::Start(const PodSandboxMetadata& pod_metadata,
                      const FileDescriptor& root_directory,
                      const FileDescriptor& image_directory,
                      const FileDescriptor& log_directory,
                      Switchboard::Stub* switchboard_servers) {
  // Idempotence: container may already have been started.
  if (container_state_ != ContainerState::CONTAINER_CREATED)
    return;

  // Open the executable.
  // TODO(ed): This should validate the path.
  // TODO(ed): Compute executable checksum.
  int executable_fd =
      openat(image_directory.get(), image_.image().c_str(), O_EXEC);
  if (executable_fd < 0)
    throw std::system_error(errno, std::system_category(), image_.image());
  FileDescriptor executable(executable_fd);

  std::unique_ptr<FileDescriptor> container_log =
      OpenContainerLog_(log_directory);

  // Obtain file descriptors for every mount.
  std::map<std::string, FileDescriptor, std::less<>> mounts;
  for (const auto& mount : mounts_) {
    // TODO(ed): Pick proper O_ACCMODE.
    const char* host_path = mount.host_path().c_str();
    while (*host_path == '/')
      ++host_path;
    int mount_fd = openat(root_directory.get(), host_path, O_SEARCH);
    if (mount_fd < 0)
      throw std::system_error(errno, std::system_category(), mount.host_path());
    mounts.emplace(mount.container_path(), mount_fd);
  }

  // Convert Argdata in YAML form to serialized data.
  YAMLErrorFactory<const argdata_t*> error_factory;
  YAMLFileDescriptorFactory file_descriptor_factory(
      &pod_metadata, &metadata_, container_log.get(), &mounts,
      switchboard_servers, &error_factory);
  YAMLArgdataFactory argdata_factory(&file_descriptor_factory);
  YAMLCanonicalizingFactory<const argdata_t*> canonicalizing_factory(
      &argdata_factory);
  YAMLBuilder<const argdata_t*> builder(&canonicalizing_factory);
  const argdata_t* argdata = builder.Build(argdata_);

  // Fork and execute child process.
  int child = program_spawn(executable.get(), argdata);
  if (child < 0)
    throw std::system_error(errno, std::system_category(),
                            "Failed to fork process");
  container_state_ = ContainerState::CONTAINER_RUNNING;
  child_process_.emplace(child);
  start_time_ = std::chrono::system_clock::now();
}

void Container::Stop(std::int64_t timeout) {
  if (container_state_ == ContainerState::CONTAINER_RUNNING) {
    child_process_.reset();
    container_state_ = ContainerState::CONTAINER_EXITED;
    finish_time_ = std::chrono::system_clock::now();
    exit_code_ = SIGKILL;
  }
}

ContainerState Container::GetContainerState_() {
  if (container_state_ == ContainerState::CONTAINER_RUNNING) {
    siginfo_t si;
    pdwait(child_process_->get(), &si, WNOHANG);
    if (si.si_signo == SIGCHLD) {
      child_process_.reset();
      container_state_ = ContainerState::CONTAINER_EXITED;
      finish_time_ = std::chrono::system_clock::now();
      exit_code_ = si.si_status;
    }
  }
  return container_state_;
}

std::unique_ptr<FileDescriptor> Container::OpenContainerLog_(
    const FileDescriptor& log_directory) {
  // Open logging output file.
  int logfile = openat(log_directory.get(), log_path_.c_str(),
                       O_CREAT | O_WRONLY | O_TRUNC, 0666);
  if (logfile < 0)
    throw std::runtime_error(std::string("Failed to open logfile ") +
                             std::string(log_path_));
  auto logfd = std::make_unique<FileDescriptor>(logfile);

  // Create pipe to which the container may write.
  int pipefds[2];
  if (pipe(pipefds) != 0)
    throw std::system_error(errno, std::system_category(),
                            "Failed to create pipe");
  auto readfd = std::make_unique<FileDescriptor>(pipefds[0]);
  auto writefd = std::make_unique<FileDescriptor>(pipefds[1]);

  // Read messages from the pipe and write them into the log file in the
  // format that Kubernetes expects.
  std::thread([ logfd{std::move(logfd)}, readfd{std::move(readfd)} ]() mutable {
    // Startup message.
    fd_streambuf logstreambuf(std::move(logfd));
    std::ostream logfile(&logstreambuf);
    logfile << ISO8601Timestamp() << " stderr --- Logging started" << std::endl;

    // Processing of logs written by the container.
    ssize_t input_length;
    bool line_start = true;
    for (;;) {
      char input_buffer[4096];
      input_length = read(readfd->get(), input_buffer, sizeof(input_buffer));
      if (input_length <= 0)
        break;

      std::optional<ISO8601Timestamp> now;
      for (char c : std::string_view(input_buffer, input_length)) {
        if (line_start) {
          if (!now)
            now = ISO8601Timestamp();
          logfile << *now << " stdout ";
          line_start = false;
        }
        logfile << c;
        if (c == '\n')
          line_start = true;
      }
      logfile << std::flush;
    }
    if (!line_start)
      logfile << std::endl;

    // Termination message.
    logfile << ISO8601Timestamp() << " stderr --- Logging stopped: "
            << (input_length == 0 ? "Pipe closed by container"
                                  : std::strerror(errno))
            << std::endl;
  }).detach();
  return writefd;
}
