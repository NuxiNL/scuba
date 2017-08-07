// Copyright (c) 2017 Nuxi, https://nuxi.nl/
//
// This file is distributed under a 2-clause BSD license.
// See the LICENSE file for details.

#ifndef SCUBA_RUNTIME_SERVICE_YAML_ERROR_FACTORY_H
#define SCUBA_RUNTIME_SERVICE_YAML_ERROR_FACTORY_H

#include <sstream>
#include <string_view>
#include <vector>

#include <yaml-cpp/exceptions.h>

#include <scuba/runtime_service/yaml_factory.h>

namespace scuba {
namespace runtime_service {

template <typename T>
class YAMLErrorFactory : public YAMLFactory<T> {
 public:
  T GetNull(const YAML::Mark& mark) override {
    throw YAML::ParserException(mark, "Unsupported null");
  }

  T GetScalar(const YAML::Mark& mark, std::string_view tag,
              std::string_view value) override {
    std::ostringstream ss;
    ss << "Unsupported scalar with tag \"" << tag << "\"";
    throw YAML::ParserException(mark, ss.str());
  }

  T GetSequence(const YAML::Mark& mark, std::string_view tag,
                std::vector<T> elements) override {
    std::ostringstream ss;
    ss << "Unsupported sequence with tag \"" << tag << "\"";
    throw YAML::ParserException(mark, ss.str());
  }

  T GetMap(const YAML::Mark& mark, std::string_view tag, std::vector<T> keys,
           std::vector<T> values) override {
    std::ostringstream ss;
    ss << "Unsupported map with tag \"" << tag << "\"";
    throw YAML::ParserException(mark, ss.str());
  }
};

}  // namespace runtime_service
}  // namespace scuba

#endif
