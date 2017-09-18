// Copyright (c) 2017 Nuxi, https://nuxi.nl/
//
// This file is distributed under a 2-clause BSD license.
// See the LICENSE file for details.

#include <iostream>
#include <optional>
#include <shared_mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

#include <grpc++/grpc++.h>
#include <k8s.io/kubernetes/pkg/kubelet/apis/cri/v1alpha1/runtime/api.grpc.pb.h>

#include <scuba/runtime_service/naming_scheme.h>
#include <scuba/runtime_service/runtime_service.h>

using grpc::ServerContext;
using grpc::Status;
using grpc::StatusCode;
using runtime::AttachRequest;
using runtime::AttachResponse;
using runtime::ContainerConfig;
using runtime::ContainerFilter;
using runtime::ContainerState;
using runtime::ContainerStatus;
using runtime::ContainerStatusRequest;
using runtime::ContainerStatusResponse;
using runtime::CreateContainerRequest;
using runtime::CreateContainerResponse;
using runtime::ListContainersRequest;
using runtime::ListContainersResponse;
using runtime::ListPodSandboxRequest;
using runtime::ListPodSandboxResponse;
using runtime::PodSandboxConfig;
using runtime::PodSandboxFilter;
using runtime::PodSandboxState;
using runtime::PodSandboxStatus;
using runtime::PodSandboxStatusRequest;
using runtime::PodSandboxStatusResponse;
using runtime::PortForwardRequest;
using runtime::PortForwardResponse;
using runtime::RemoveContainerRequest;
using runtime::RemoveContainerResponse;
using runtime::RemovePodSandboxRequest;
using runtime::RemovePodSandboxResponse;
using runtime::RunPodSandboxRequest;
using runtime::RunPodSandboxResponse;
using runtime::RuntimeCondition;
using runtime::RuntimeStatus;
using runtime::StartContainerRequest;
using runtime::StartContainerResponse;
using runtime::StatusRequest;
using runtime::StatusResponse;
using runtime::StopContainerRequest;
using runtime::StopContainerResponse;
using runtime::StopPodSandboxRequest;
using runtime::StopPodSandboxResponse;
using runtime::UpdateRuntimeConfigRequest;
using runtime::UpdateRuntimeConfigRequest;
using runtime::UpdateRuntimeConfigResponse;
using runtime::VersionRequest;
using runtime::VersionRequest;
using runtime::VersionResponse;
using runtime::VersionResponse;
using scuba::runtime_service::RuntimeService;

Status RuntimeService::Version(ServerContext* context,
                               const VersionRequest* request,
                               VersionResponse* response) {
  response->set_version("0.1.0");
  response->set_runtime_name("scuba");
  response->set_runtime_version("0.1");
  response->set_runtime_api_version("v1alpha1");
  return Status::OK;
}

Status RuntimeService::Status(ServerContext* context,
                              const StatusRequest* request,
                              StatusResponse* response) {
  // This environment is always runtime ready and network ready.
  RuntimeStatus* status = response->mutable_status();
  RuntimeCondition* condition = status->add_conditions();
  condition->set_type("RuntimeReady");
  condition->set_status(true);

  condition = status->add_conditions();
  condition->set_type("NetworkReady");
  condition->set_status(true);
  return Status::OK;
}

Status RuntimeService::RunPodSandbox(ServerContext* context,
                                     const RunPodSandboxRequest* request,
                                     RunPodSandboxResponse* response) {
  const PodSandboxConfig& config = request->config();
  std::string pod_sandbox_id =
      NamingScheme::CreatePodSandboxName(config.metadata());

  // Idempotence: only create the pod sandbox if it doesn't exist yet.
  std::unique_lock lock(pod_sandboxes_lock_);
  auto pod_sandbox = pod_sandboxes_.find(pod_sandbox_id);
  if (pod_sandbox == pod_sandboxes_.end()) {
    IPAddressLease ip_address_lease;
    try {
      ip_address_lease = ip_address_allocator_->Allocate();
    } catch (const std::exception& e) {
      return {StatusCode::INTERNAL, e.what()};
    }
    pod_sandboxes_.insert(
        pod_sandbox, std::make_pair(pod_sandbox_id,
                                    std::make_unique<PodSandbox>(
                                        config, std::move(ip_address_lease))));
  }

  response->set_pod_sandbox_id(pod_sandbox_id);
  return Status::OK;
}

Status RuntimeService::StopPodSandbox(ServerContext* context,
                                      const StopPodSandboxRequest* request,
                                      StopPodSandboxResponse* response) {
  std::shared_lock lock(pod_sandboxes_lock_);
  auto pod_sandbox = pod_sandboxes_.find(request->pod_sandbox_id());
  if (pod_sandbox == pod_sandboxes_.end())
    return {StatusCode::NOT_FOUND, "Pod sandbox does not exist"};
  pod_sandbox->second->Stop();
  return Status::OK;
}

Status RuntimeService::RemovePodSandbox(ServerContext* context,
                                        const RemovePodSandboxRequest* request,
                                        RemovePodSandboxResponse* response) {
  std::unique_lock lock(pod_sandboxes_lock_);
  pod_sandboxes_.erase(request->pod_sandbox_id());
  return Status::OK;
}

Status RuntimeService::PodSandboxStatus(ServerContext* context,
                                        const PodSandboxStatusRequest* request,
                                        PodSandboxStatusResponse* response) {
  const std::string& pod_sandbox_id = request->pod_sandbox_id();
  std::shared_lock lock(pod_sandboxes_lock_);
  auto pod_sandbox = pod_sandboxes_.find(pod_sandbox_id);
  if (pod_sandbox == pod_sandboxes_.end())
    return {StatusCode::NOT_FOUND, "Pod sandbox does not exist"};

  auto status = response->mutable_status();
  pod_sandbox->second->GetStatus(status);
  status->set_id(pod_sandbox_id);
  return Status::OK;
}

Status RuntimeService::ListPodSandbox(ServerContext* context,
                                      const ListPodSandboxRequest* request,
                                      ListPodSandboxResponse* response) {
  const PodSandboxFilter& filter = request->filter();
  const std::string& pod_sandbox_id = filter.id();
  std::optional<PodSandboxState> state;
  if (filter.has_state())
    state = filter.state().state();

  std::shared_lock lock(pod_sandboxes_lock_);
  for (const auto& pod_sandbox : pod_sandboxes_) {
    // Apply filters.
    if (!pod_sandbox_id.empty() && pod_sandbox_id != pod_sandbox.first)
      continue;
    if (!pod_sandbox.second->MatchesFilter(state, filter.label_selector()))
      continue;

    runtime::PodSandbox* info = response->add_items();
    pod_sandbox.second->GetInfo(info);
    info->set_id(pod_sandbox.first);
  }
  return Status::OK;
}

Status RuntimeService::CreateContainer(ServerContext* context,
                                       const CreateContainerRequest* request,
                                       CreateContainerResponse* response) {
  std::shared_lock lock(pod_sandboxes_lock_);
  auto pod_sandbox = pod_sandboxes_.find(request->pod_sandbox_id());
  if (pod_sandbox == pod_sandboxes_.end())
    return {StatusCode::NOT_FOUND, "Pod sandbox does not exist"};

  const ContainerConfig& config = request->config();
  std::string container_id =
      NamingScheme::CreateContainerName(config.metadata());
  pod_sandbox->second->CreateContainer(container_id, config);
  response->set_container_id(NamingScheme::ComposePodSandboxContainerName(
      pod_sandbox->first, container_id));
  return Status::OK;
}

Status RuntimeService::StartContainer(ServerContext* context,
                                      const StartContainerRequest* request,
                                      StartContainerResponse* response) {
  auto ids =
      NamingScheme::DecomposePodSandboxContainerName(request->container_id());
  std::shared_lock lock(pod_sandboxes_lock_);
  auto pod_sandbox = pod_sandboxes_.find(ids.first);
  if (pod_sandbox == pod_sandboxes_.end())
    return {StatusCode::NOT_FOUND, "Pod sandbox does not exist"};
  try {
    pod_sandbox->second->StartContainer(
        ids.second, *root_directory_, *image_directory_, switchboard_servers_);
  } catch (const std::invalid_argument& e) {
    return {StatusCode::INVALID_ARGUMENT, e.what()};
  } catch (const std::exception& e) {
    return {StatusCode::INTERNAL, e.what()};
  }
  return Status::OK;
}

Status RuntimeService::StopContainer(ServerContext* context,
                                     const StopContainerRequest* request,
                                     StopContainerResponse* response) {
  auto ids =
      NamingScheme::DecomposePodSandboxContainerName(request->container_id());
  std::shared_lock lock(pod_sandboxes_lock_);
  auto pod_sandbox = pod_sandboxes_.find(ids.first);
  if (pod_sandbox == pod_sandboxes_.end())
    return {StatusCode::NOT_FOUND, "Pod sandbox does not exist"};
  if (!pod_sandbox->second->StopContainer(ids.second, request->timeout()))
    return {StatusCode::NOT_FOUND, "Container does not exist"};
  return Status::OK;
}

Status RuntimeService::RemoveContainer(ServerContext* context,
                                       const RemoveContainerRequest* request,
                                       RemoveContainerResponse* response) {
  auto ids =
      NamingScheme::DecomposePodSandboxContainerName(request->container_id());
  std::shared_lock lock(pod_sandboxes_lock_);
  auto pod_sandbox = pod_sandboxes_.find(ids.first);
  if (pod_sandbox != pod_sandboxes_.end())
    pod_sandbox->second->RemoveContainer(ids.second);
  return Status::OK;
}

Status RuntimeService::ListContainers(ServerContext* context,
                                      const ListContainersRequest* request,
                                      ListContainersResponse* response) {
  const ContainerFilter& filter = request->filter();
  auto ids = NamingScheme::DecomposePodSandboxContainerName(filter.id());
  const std::string& pod_sandbox_id = filter.pod_sandbox_id();
  std::optional<ContainerState> state;
  if (filter.has_state())
    state = filter.state().state();

  std::shared_lock lock(pod_sandboxes_lock_);
  for (const auto& pod_sandbox : pod_sandboxes_) {
    // Apply filters.
    if (!pod_sandbox_id.empty() && pod_sandbox_id != pod_sandbox.first)
      continue;
    if (!ids.first.empty() && ids.first != pod_sandbox.first)
      continue;

    for (const auto& info_in : pod_sandbox.second->GetContainerInfo(
             ids.second, state, filter.label_selector())) {
      runtime::Container* info_out = response->add_containers();
      *info_out = info_in.second;
      info_out->set_id(NamingScheme::ComposePodSandboxContainerName(
          pod_sandbox.first, info_in.first));
      info_out->set_pod_sandbox_id(pod_sandbox.first);
    }
  }
  return Status::OK;
}

Status RuntimeService::ContainerStatus(ServerContext* context,
                                       const ContainerStatusRequest* request,
                                       ContainerStatusResponse* response) {
  const std::string& id = request->container_id();
  auto ids = NamingScheme::DecomposePodSandboxContainerName(id);
  std::shared_lock lock(pod_sandboxes_lock_);
  auto pod_sandbox = pod_sandboxes_.find(ids.first);
  if (pod_sandbox == pod_sandboxes_.end())
    return {StatusCode::NOT_FOUND, "Pod sandbox does not exist"};
  auto status = response->mutable_status();
  if (!pod_sandbox->second->GetContainerStatus(ids.second, status))
    return {StatusCode::NOT_FOUND, "Container does not exist"};
  status->set_id(id);
  return Status::OK;
}

Status RuntimeService::Attach(ServerContext* context,
                              const AttachRequest* request,
                              AttachResponse* response) {
  return {StatusCode::UNIMPLEMENTED, "Attach still needs to be implemented!"};
}

Status RuntimeService::PortForward(ServerContext* context,
                                   const PortForwardRequest* request,
                                   PortForwardResponse* response) {
  return {StatusCode::UNIMPLEMENTED,
          "PortForward still needs to be implemented!"};
}

Status RuntimeService::UpdateRuntimeConfig(
    ServerContext* context, const UpdateRuntimeConfigRequest* request,
    UpdateRuntimeConfigResponse* response) {
  if (!ip_address_allocator_->SetRange(
          request->runtime_config().network_config().pod_cidr()))
    return {StatusCode::INVALID_ARGUMENT, "Failed to parse IP range"};
  return Status::OK;
}
