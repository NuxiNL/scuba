// Copyright (c) 2017 Nuxi, https://nuxi.nl/
//
// This file is distributed under a 2-clause BSD license.
// See the LICENSE file for details.

#ifndef SCUBA_RUNTIME_SERVICE_NAMING_SCHEME_H
#define SCUBA_RUNTIME_SERVICE_NAMING_SCHEME_H

#include <string>
#include <string_view>
#include <utility>

#include <k8s.io/kubernetes/pkg/kubelet/apis/cri/v1alpha1/runtime/api.pb.h>

namespace scuba {
namespace runtime_service {

class NamingScheme {
 public:
  static std::string CreatePodSandboxName(
      const runtime::PodSandboxMetadata& metadata);
  static std::string CreateContainerName(
      const runtime::ContainerMetadata& metadata);

  static std::string ComposePodSandboxContainerName(
      std::string_view pod_sandbox_id, std::string_view container_id);
  static std::pair<std::string_view, std::string_view>
  DecomposePodSandboxContainerName(const std::string_view id);
};

}  // namespace runtime_service
}  // namespace scuba

#endif
