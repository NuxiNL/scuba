// Copyright (c) 2017 Nuxi, https://nuxi.nl/
//
// SPDX-License-Identifier: BSD-2-Clause

#ifndef SCUBA_RUNTIME_SERVICE_YAML_FILE_DESCRIPTOR_FACTORY_H
#define SCUBA_RUNTIME_SERVICE_YAML_FILE_DESCRIPTOR_FACTORY_H

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "argdata.hpp"
#include "flower/protocol/switchboard.ad.h"
#include "k8s.io/kubernetes/pkg/kubelet/apis/cri/v1alpha1/runtime/api.pb.h"
#include "yaml-cpp/mark.h"
#include "yaml2argdata/yaml_factory.h"

namespace scuba {
namespace runtime_service {

class YAMLFileDescriptorFactory
    : public yaml2argdata::YAMLFactory<const argdata_t*> {
 public:
  YAMLFileDescriptorFactory(
      const runtime::PodSandboxMetadata* pod_metadata,
      const runtime::ContainerMetadata* container_metadata,
      const arpc::FileDescriptor* container_log,
      const std::map<std::string, arpc::FileDescriptor, std::less<>>* mounts,
      flower::protocol::switchboard::Switchboard::Stub* switchboard_servers,
      YAMLFactory<const argdata_t*>* fallback)
      : pod_metadata_(pod_metadata),
        container_metadata_(container_metadata),
        container_log_(container_log),
        mounts_(mounts),
        switchboard_servers_(switchboard_servers),
        fallback_(fallback) {
  }

  const argdata_t* GetNull(const YAML::Mark& mark) override;
  const argdata_t* GetScalar(const YAML::Mark& mark, std::string_view tag,
                             std::string_view value) override;
  const argdata_t* GetSequence(const YAML::Mark& mark, std::string_view tag,
                               std::vector<const argdata_t*> elements) override;
  const argdata_t* GetMap(const YAML::Mark& mark, std::string_view tag,
                          std::vector<const argdata_t*> keys,
                          std::vector<const argdata_t*> values) override;

 private:
  const runtime::PodSandboxMetadata* const pod_metadata_;
  const runtime::ContainerMetadata* const container_metadata_;
  const arpc::FileDescriptor* const container_log_;
  const std::map<std::string, arpc::FileDescriptor, std::less<>>* const mounts_;
  flower::protocol::switchboard::Switchboard::Stub* const switchboard_servers_;
  YAMLFactory<const argdata_t*>* const fallback_;

  std::vector<std::unique_ptr<argdata_t>> argdatas_;
  std::vector<std::shared_ptr<arpc::FileDescriptor>> fds_;
};

}  // namespace runtime_service
}  // namespace scuba

#endif
