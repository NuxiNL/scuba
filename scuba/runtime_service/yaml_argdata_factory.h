// Copyright (c) 2017 Nuxi, https://nuxi.nl/
//
// This file is distributed under a 2-clause BSD license.
// See the LICENSE file for details.

#ifndef SCUBA_RUNTIME_SERVICE_YAML_ARGDATA_FACTORY_H
#define SCUBA_RUNTIME_SERVICE_YAML_ARGDATA_FACTORY_H

#include <forward_list>
#include <memory>
#include <string_view>
#include <vector>

#include <argdata.hpp>

#include <scuba/runtime_service/yaml_factory.h>

namespace scuba {
namespace runtime_service {

class YAMLArgdataFactory : public YAMLFactory<const argdata_t*> {
 public:
  explicit YAMLArgdataFactory(YAMLFactory<const argdata_t*>* fallback)
      : fallback_(fallback) {
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
  YAMLFactory<const argdata_t*>* const fallback_;

  std::vector<std::unique_ptr<argdata_t>> argdatas_;
  std::forward_list<std::vector<const argdata_t*>> lists_;
  std::forward_list<std::string> strings_;
};

}  // namespace runtime_service
}  // namespace scuba

#endif
