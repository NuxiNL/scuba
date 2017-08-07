// Copyright (c) 2017 Nuxi, https://nuxi.nl/
//
// This file is distributed under a 2-clause BSD license.
// See the LICENSE file for details.

#include <fcntl.h>

#include <cerrno>
#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <arpc++/arpc++.h>
#include <flower/protocol/switchboard.ad.h>
#include <google/protobuf/map.h>

#include <scuba/runtime_service/ip_address_allocator.h>
#include <scuba/runtime_service/pod_sandbox.h>

using arpc::FileDescriptor;
using flower::protocol::switchboard::Switchboard;
using google::protobuf::Map;
using runtime::ContainerConfig;
using runtime::ContainerState;
using runtime::ContainerStatus;
using runtime::PodSandboxConfig;
using runtime::PodSandboxState;
using runtime::PodSandboxStatus;
using runtime::SANDBOX_READY;
using scuba::runtime_service::IPAddressLease;
using scuba::runtime_service::PodSandbox;

PodSandbox::PodSandbox(const PodSandboxConfig& config,
                       IPAddressLease ip_address_lease)
    : metadata_(config.metadata()),
      log_directory_(config.log_directory()),
      creation_time_(std::chrono::system_clock::now()),
      labels_(config.labels()),
      annotations_(config.annotations()),
      ip_address_lease_(std::move(ip_address_lease)) {
}

void PodSandbox::GetInfo(runtime::PodSandbox* info) {
  *info->mutable_metadata() = metadata_;
  info->set_state(SANDBOX_READY);
  info->set_created_at(
      std::chrono::nanoseconds(creation_time_.time_since_epoch()).count());
  *info->mutable_labels() = labels_;
  *info->mutable_annotations() = annotations_;
}

void PodSandbox::GetStatus(PodSandboxStatus* status) {
  *status->mutable_metadata() = metadata_;
  status->set_state(SANDBOX_READY);
  status->set_created_at(creation_time_.time_since_epoch().count());
  status->mutable_network()->set_ip(ip_address_lease_.GetString());
  *status->mutable_labels() = labels_;
  *status->mutable_annotations() = annotations_;
}

void PodSandbox::Stop() {
  // Do a forced stop of all containers in the pod sandbox.
  for (const auto& container : containers_)
    container.second->Stop(0);
}

bool PodSandbox::MatchesFilter(
    std::optional<PodSandboxState> state,
    const Map<std::string, std::string>& labels) const {
  return (!state || *state == SANDBOX_READY) &&
         std::includes(labels_.begin(), labels_.end(), labels.begin(),
                       labels.end(),
                       std::less<std::pair<std::string, std::string>>());
}

void PodSandbox::CreateContainer(std::string_view container_id,
                                 const ContainerConfig& config) {
  // Idempotence: only create the container if it doesn't exist yet.
  auto container = containers_.find(container_id);
  if (container == containers_.end())
    containers_.insert(
        container,
        std::make_pair(container_id, std::make_unique<Container>(config)));
}

void PodSandbox::RemoveContainer(std::string_view container_id) {
  // TODO(ed): Why can't we call erase() without casting?
  containers_.erase(std::string(container_id));
}

void PodSandbox::StartContainer(std::string_view container_id,
                                const FileDescriptor& root_directory,
                                const FileDescriptor& image_directory,
                                Switchboard::Stub* switchboard_servers) {
  auto container = containers_.find(container_id);
  if (container == containers_.end())
    throw std::invalid_argument(std::string(container_id) + " does not exist");

  // Turn provided log directory into a path relative to the root.
  const char* log_directory = log_directory_.c_str();
  while (*log_directory == '/')
    ++log_directory;
  int fd = openat(root_directory.get(), log_directory, O_DIRECTORY | O_SEARCH);
  if (fd < 0)
    throw std::system_error(errno, std::system_category(), log_directory_);
  container->second->Start(metadata_, root_directory, image_directory,
                           FileDescriptor(fd), switchboard_servers);
}

bool PodSandbox::StopContainer(std::string_view container_id,
                               std::int64_t timeout) {
  auto container = containers_.find(container_id);
  if (container == containers_.end())
    return false;
  container->second->Stop(timeout);
  return true;
}

std::vector<std::pair<std::string, runtime::Container>>
PodSandbox::GetContainerInfo(std::string_view container_id,
                             std::optional<ContainerState> state,
                             const Map<std::string, std::string>& labels) {
  std::vector<std::pair<std::string, runtime::Container>> infos;
  for (const auto& container : containers_) {
    // Apply filters.
    if (!container_id.empty() && container_id != container.first)
      continue;
    if (!container.second->MatchesFilter(state, labels))
      continue;

    infos.emplace_back(container.first, runtime::Container{});
    container.second->GetInfo(&infos.back().second);
  }
  return infos;
}

bool PodSandbox::GetContainerStatus(std::string_view container_id,
                                    ContainerStatus* status) {
  auto container = containers_.find(container_id);
  if (container == containers_.end())
    return false;
  container->second->GetStatus(status);
  return true;
}
