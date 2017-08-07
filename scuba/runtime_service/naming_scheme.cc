// Copyright (c) 2017 Nuxi, https://nuxi.nl/
//
// This file is distributed under a 2-clause BSD license.
// See the LICENSE file for details.

#include <algorithm>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

#include <k8s.io/kubernetes/pkg/kubelet/apis/cri/v1alpha1/runtime/api.grpc.pb.h>

#include <scuba/runtime_service/naming_scheme.h>

using runtime::ContainerMetadata;
using runtime::PodSandboxMetadata;
using scuba::runtime_service::NamingScheme;

std::string NamingScheme::CreatePodSandboxName(
    const PodSandboxMetadata& metadata) {
  std::ostringstream ss;
  ss << "name=" << metadata.name() << ",uid=" << metadata.uid()
     << ",namespace=" << metadata.namespace_()
     << ",attempt=" << metadata.attempt();
  return ss.str();
}

std::string NamingScheme::CreateContainerName(
    const ContainerMetadata& metadata) {
  std::ostringstream ss;
  ss << "name=" << metadata.name() << ",attempt=" << metadata.attempt();
  return ss.str();
}

std::string NamingScheme::ComposePodSandboxContainerName(
    std::string_view pod_sandbox_id, std::string_view container_id) {
  std::ostringstream ss;
  ss << pod_sandbox_id << "|" << container_id;
  return ss.str();
}

std::pair<std::string_view, std::string_view>
NamingScheme::DecomposePodSandboxContainerName(std::string_view id) {
  auto split = std::find(id.begin(), id.end(), '|');
  if (split == id.end())
    return {};
  return {std::string_view(id.begin(), split - id.begin()),
          std::string_view(split + 1, id.end() - (split + 1))};
}
