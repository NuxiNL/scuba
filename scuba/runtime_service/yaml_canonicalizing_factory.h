// Copyright (c) 2017 Nuxi, https://nuxi.nl/
//
// This file is distributed under a 2-clause BSD license.
// See the LICENSE file for details.

#ifndef SCUBA_RUNTIME_SERVICE_YAML_CANONICALIZING_FACTORY_H
#define SCUBA_RUNTIME_SERVICE_YAML_CANONICALIZING_FACTORY_H

#include <string_view>
#include <vector>

#include <yaml-cpp/exceptions.h>

#include <scuba/runtime_service/yaml_factory.h>

namespace scuba {
namespace runtime_service {

template <typename T>
class YAMLCanonicalizingFactory : public YAMLFactory<T> {
 public:
  explicit YAMLCanonicalizingFactory(YAMLFactory<const argdata_t*>* fallback)
      : fallback_(fallback) {
  }

  T GetNull(const YAML::Mark& mark) override {
    return fallback_->GetNull(mark);
  }

  T GetScalar(const YAML::Mark& mark, std::string_view tag,
              std::string_view value) override {
    if (tag == "!") {
      tag = "tag:yaml.org,2002:str";
    } else if (tag == "?") {
      // TODO(ed): Use heuristics to switch to other types.
      tag = "tag:yaml.org,2002:str";
    }
    return fallback_->GetScalar(mark, tag, value);
  }

  T GetSequence(const YAML::Mark& mark, std::string_view tag,
                std::vector<T> elements) override {
    if (tag == "!" || tag == "?") {
      tag = "tag:yaml.org,2002:seq";
    }
    return fallback_->GetSequence(mark, tag, std::move(elements));
  }

  T GetMap(const YAML::Mark& mark, std::string_view tag, std::vector<T> keys,
           std::vector<T> values) override {
    if (tag == "!" || tag == "?") {
      tag = "tag:yaml.org,2002:map";
    }
    return fallback_->GetMap(mark, tag, std::move(keys), std::move(values));
  }

 private:
  YAMLFactory<const argdata_t*>* const fallback_;
};

}  // namespace runtime_service
}  // namespace scuba

#endif
