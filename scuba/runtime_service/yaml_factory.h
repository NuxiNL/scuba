// Copyright (c) 2017 Nuxi, https://nuxi.nl/
//
// This file is distributed under a 2-clause BSD license.
// See the LICENSE file for details.

#ifndef SCUBA_RUNTIME_SERVICE_YAML_FACTORY_H
#define SCUBA_RUNTIME_SERVICE_YAML_FACTORY_H

#include <string_view>
#include <vector>

#include <yaml-cpp/mark.h>

namespace scuba {
namespace runtime_service {

template <typename T>
class YAMLFactory {
 public:
  virtual ~YAMLFactory() {
  }

  virtual T GetNull(const YAML::Mark& mark) = 0;
  virtual T GetScalar(const YAML::Mark& mark, std::string_view tag,
                      std::string_view value) = 0;
  virtual T GetSequence(const YAML::Mark& mark, std::string_view tag,
                        std::vector<T> elements) = 0;
  virtual T GetMap(const YAML::Mark& mark, std::string_view tag,
                   std::vector<T> keys, std::vector<T> elements) = 0;
};

}  // namespace runtime_service
}  // namespace scuba

#endif
