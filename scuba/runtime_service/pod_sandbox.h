// Copyright (c) 2017 Nuxi, https://nuxi.nl/
//
// SPDX-License-Identifier: BSD-2-Clause

#ifndef SCUBA_RUNTIME_SERVICE_POD_SANDBOX_H
#define SCUBA_RUNTIME_SERVICE_POD_SANDBOX_H

#include <chrono>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <arpc++/arpc++.h>
#include <flower/protocol/switchboard.ad.h>
#include <google/protobuf/map.h>
#include <k8s.io/kubernetes/pkg/kubelet/apis/cri/v1alpha1/runtime/api.pb.h>

#include <scuba/runtime_service/container.h>
#include <scuba/runtime_service/ip_address_allocator.h>

namespace scuba {
namespace runtime_service {

class PodSandbox {
 public:
  explicit PodSandbox(const runtime::PodSandboxConfig& config,
                      IPAddressLease ip);

  void GetInfo(runtime::PodSandbox* info);
  void GetStatus(runtime::PodSandboxStatus* status);
  void Stop();

  bool MatchesFilter(
      std::optional<runtime::PodSandboxState> state,
      const google::protobuf::Map<std::string, std::string>& labels);

  void CreateContainer(std::string_view container_id,
                       const runtime::ContainerConfig& config);
  void RemoveContainer(std::string_view container_id);
  void StartContainer(std::string_view container_id,
                      const arpc::FileDescriptor& root_directory,
                      const arpc::FileDescriptor& image_directory,
                      flower::protocol::switchboard::Switchboard::Stub*
                          containers_switchboard_handle);
  bool StopContainer(std::string_view container_id, std::int64_t timeout);
  std::vector<std::pair<std::string, runtime::Container>> GetContainerInfo(
      std::string_view container_id,
      std::optional<runtime::ContainerState> state,
      const google::protobuf::Map<std::string, std::string>& labels);
  bool GetContainerStatus(std::string_view container_id,
                          runtime::ContainerStatus* status);

 private:
  // Data that should be returned through PodSandboxStatus.
  const runtime::PodSandboxMetadata metadata_;
  const std::string log_directory_;
  const std::chrono::system_clock::time_point creation_time_;
  const google::protobuf::Map<std::string, std::string> labels_;
  const google::protobuf::Map<std::string, std::string> annotations_;
  const IPAddressLease ip_address_lease_;

  std::shared_mutex lock_;
  runtime::PodSandboxState state_;
  std::map<std::string, std::unique_ptr<Container>, std::less<>> containers_;

  PodSandbox(PodSandbox&) = delete;
  void operator=(PodSandbox) = delete;
};

}  // namespace runtime_service
}  // namespace scuba

#endif
