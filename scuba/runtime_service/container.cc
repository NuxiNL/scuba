// Copyright (c) 2017 Nuxi, https://nuxi.nl/
//
// This file is distributed under a 2-clause BSD license.
// See the LICENSE file for details.

#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <uv.h>

#include <algorithm>
#include <cassert>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <fstream>
#include <functional>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>

#include <google/protobuf/map.h>
#include <k8s.io/kubernetes/pkg/kubelet/apis/cri/v1alpha1/runtime/api.pb.h>
#include <yaml2argdata/yaml_argdata_factory.h>
#include <yaml2argdata/yaml_builder.h>
#include <yaml2argdata/yaml_canonicalizing_factory.h>
#include <yaml2argdata/yaml_error_factory.h>
#include <argdata.hpp>

#include <scuba/runtime_service/iso8601_timestamp.h>
#include <scuba/runtime_service/pod_sandbox.h>
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
using yaml2argdata::YAMLArgdataFactory;
using yaml2argdata::YAMLBuilder;
using yaml2argdata::YAMLCanonicalizingFactory;
using yaml2argdata::YAMLErrorFactory;

std::mutex Container::child_loop_lock_;
uv_loop_t Container::child_loop_;

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
  // If this is the first container to be created, create an event loop
  // with which we can track termination of child processes.
  static std::once_flag child_loop_initialized;
  std::call_once(child_loop_initialized, []() {
    if (uv_loop_init(&child_loop_) != 0)
      std::terminate();
  });
}

Container::~Container() {
  std::unique_lock lock(child_loop_lock_);
  if (container_state_ != ContainerState::CONTAINER_CREATED) {
    // Child process spawned. Unregister process handle from the event loop.
    uv_close(reinterpret_cast<uv_handle_t*>(&child_process_),
             [](uv_handle_t* handle) {
               Container* container =
                   reinterpret_cast<Container*>(handle->data);
               container->container_state_ = ContainerState::CONTAINER_EXITED;
             });
    uv_run(&child_loop_, UV_RUN_NOWAIT);
    assert(container_state_ == ContainerState::CONTAINER_EXITED &&
           "Destroying container while the child process is running");
  }
}

void Container::GetInfo(runtime::Container* info) {
  *info->mutable_metadata() = metadata_;
  *info->mutable_image() = image_;
  info->set_image_ref(image_.image());
  *info->mutable_labels() = labels_;
  *info->mutable_annotations() = annotations_;

  std::unique_lock lock(child_loop_lock_);
  uv_run(&child_loop_, UV_RUN_NOWAIT);
  info->set_state(container_state_);
  info->set_created_at(
      std::chrono::nanoseconds(creation_time_.time_since_epoch()).count());
}

void Container::GetStatus(ContainerStatus* status) {
  *status->mutable_metadata() = metadata_;
  *status->mutable_image() = image_;
  status->set_image_ref(image_.image());
  *status->mutable_labels() = labels_;
  *status->mutable_annotations() = annotations_;
  *status->mutable_mounts() = mounts_;
  status->set_log_path(log_path_);
  // TODO(ed): reason, message.

  std::unique_lock lock(child_loop_lock_);
  uv_run(&child_loop_, UV_RUN_NOWAIT);
  status->set_state(container_state_);
  switch (container_state_) {
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
}

bool Container::MatchesFilter(std::optional<ContainerState> state,
                              const Map<std::string, std::string>& labels) {
  // Perform subset match on labels. We can't use std::includes() here,
  // as these maps use hashing.
  for (const auto& label : labels) {
    const auto& match = labels_.find(label.first);
    if (match == labels_.end() || label.second != match->second)
      return false;
  }
  if (!state)
    return true;
  std::unique_lock lock(child_loop_lock_);
  uv_run(&child_loop_, UV_RUN_NOWAIT);
  return *state == container_state_;
}

void Container::Start(const PodSandboxMetadata& pod_metadata,
                      const FileDescriptor& root_directory,
                      const FileDescriptor& image_directory,
                      const FileDescriptor& log_directory,
                      Switchboard::Stub* containers_switchboard_handle) {
  // Idempotence: container may already have been started.
  std::unique_lock lock(child_loop_lock_);
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
      containers_switchboard_handle, &error_factory);
  YAMLArgdataFactory argdata_factory(&file_descriptor_factory);
  YAMLCanonicalizingFactory<const argdata_t*> canonicalizing_factory(
      &argdata_factory);
  YAMLBuilder<const argdata_t*> builder(&canonicalizing_factory);
  std::istringstream argdata_stream(argdata_);
  const argdata_t* argdata = builder.Build(&argdata_stream);

  // Create a process handle through the event loop.
  uv_process_options_t options = {};
  options.exit_cb = [](uv_process_t* process, int64_t exit_status,
                       int term_signal) {
    Container* container = reinterpret_cast<Container*>(process->data);
    container->container_state_ = ContainerState::CONTAINER_EXITED;
    container->finish_time_ = std::chrono::system_clock::now();
    container->exit_code_ = term_signal == 0 ? exit_status : term_signal;
  };
  options.executable = executable.get();
  options.argdata = argdata;
  child_process_.data = this;
  if (int error = uv_spawn(&child_loop_, &child_process_, &options); error != 0)
    throw std::runtime_error(std::string("Failed to spawn process: ") +
                             uv_strerror(error));
  container_state_ = ContainerState::CONTAINER_RUNNING;
  start_time_ = std::chrono::system_clock::now();
}

void Container::Stop(std::int64_t timeout) {
  std::unique_lock lock(child_loop_lock_);
  if (container_state_ != ContainerState::CONTAINER_CREATED)
    uv_process_kill(&child_process_, SIGKILL);
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
  })
      .detach();
  return writefd;
}
