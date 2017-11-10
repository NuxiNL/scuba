// Copyright (c) 2017 Nuxi, https://nuxi.nl/
//
// SPDX-License-Identifier: BSD-2-Clause

#ifndef SCUBA_RUNTIME_SERVICE_RUNTIME_SERVICE_H
#define SCUBA_RUNTIME_SERVICE_RUNTIME_SERVICE_H

#include <map>
#include <memory>
#include <shared_mutex>
#include <string>
#include <utility>

#include <arpc++/arpc++.h>
#include <flower/protocol/switchboard.ad.h>
#include <grpc++/grpc++.h>
#include <k8s.io/kubernetes/pkg/kubelet/apis/cri/v1alpha1/runtime/api.grpc.pb.h>

#include <scuba/runtime_service/pod_sandbox.h>

namespace scuba {
namespace runtime_service {

class IPAddressAllocator;

class RuntimeService final : public runtime::RuntimeService::Service {
 public:
  explicit RuntimeService(
      const arpc::FileDescriptor* root_directory,
      const arpc::FileDescriptor* image_directory,
      flower::protocol::switchboard::Switchboard::Stub* switchboard_servers,
      IPAddressAllocator* ip_address_allocator)
      : root_directory_(root_directory),
        image_directory_(image_directory),
        switchboard_servers_(switchboard_servers),
        ip_address_allocator_(ip_address_allocator) {
  }

  // Global state.
  grpc::Status Version(grpc::ServerContext* context,
                       const runtime::VersionRequest* request,
                       runtime::VersionResponse* response) override;
  grpc::Status Status(grpc::ServerContext* context,
                      const runtime::StatusRequest* request,
                      runtime::StatusResponse* response) override;

  // Pod management.
  grpc::Status RunPodSandbox(grpc::ServerContext* context,
                             const runtime::RunPodSandboxRequest* request,
                             runtime::RunPodSandboxResponse* response) override;
  grpc::Status StopPodSandbox(
      grpc::ServerContext* context,
      const runtime::StopPodSandboxRequest* request,
      runtime::StopPodSandboxResponse* response) override;
  grpc::Status RemovePodSandbox(
      grpc::ServerContext* context,
      const runtime::RemovePodSandboxRequest* request,
      runtime::RemovePodSandboxResponse* response) override;
  grpc::Status PodSandboxStatus(
      grpc::ServerContext* context,
      const runtime::PodSandboxStatusRequest* request,
      runtime::PodSandboxStatusResponse* response) override;
  grpc::Status ListPodSandbox(
      grpc::ServerContext* context,
      const runtime::ListPodSandboxRequest* request,
      runtime::ListPodSandboxResponse* response) override;

  // Container management.
  grpc::Status CreateContainer(
      grpc::ServerContext* context,
      const runtime::CreateContainerRequest* request,
      runtime::CreateContainerResponse* response) override;
  grpc::Status StartContainer(
      grpc::ServerContext* context,
      const runtime::StartContainerRequest* request,
      runtime::StartContainerResponse* response) override;
  grpc::Status StopContainer(grpc::ServerContext* context,
                             const runtime::StopContainerRequest* request,
                             runtime::StopContainerResponse* response) override;
  grpc::Status RemoveContainer(
      grpc::ServerContext* context,
      const runtime::RemoveContainerRequest* request,
      runtime::RemoveContainerResponse* response) override;
  grpc::Status ListContainers(
      grpc::ServerContext* context,
      const runtime::ListContainersRequest* request,
      runtime::ListContainersResponse* response) override;
  grpc::Status ContainerStatus(
      grpc::ServerContext* context,
      const runtime::ContainerStatusRequest* request,
      runtime::ContainerStatusResponse* response) override;

  // Misc.
  grpc::Status Attach(grpc::ServerContext* context,
                      const runtime::AttachRequest* request,
                      runtime::AttachResponse* response) override;
  grpc::Status PortForward(grpc::ServerContext* context,
                           const runtime::PortForwardRequest* request,
                           runtime::PortForwardResponse* response) override;
  grpc::Status UpdateRuntimeConfig(
      grpc::ServerContext* context,
      const runtime::UpdateRuntimeConfigRequest* request,
      runtime::UpdateRuntimeConfigResponse* response) override;

 private:
  const arpc::FileDescriptor* const root_directory_;
  const arpc::FileDescriptor* const image_directory_;
  flower::protocol::switchboard::Switchboard::Stub* const switchboard_servers_;
  IPAddressAllocator* const ip_address_allocator_;

  std::shared_mutex pod_sandboxes_lock_;
  std::map<std::string, std::unique_ptr<PodSandbox>, std::less<>>
      pod_sandboxes_;

  RuntimeService(RuntimeService&) = delete;
  void operator=(RuntimeService) = delete;
};

}  // namespace runtime_service
}  // namespace scuba

#endif
