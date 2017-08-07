// Copyright (c) 2017 Nuxi, https://nuxi.nl/
//
// This file is distributed under a 2-clause BSD license.
// See the LICENSE file for details.

#include <string>
#include <string_view>
#include <vector>

#include <yaml-cpp/mark.h>
#include <argdata.hpp>

#include <scuba/runtime_service/yaml_argdata_factory.h>

using scuba::runtime_service::YAMLArgdataFactory;

const argdata_t* YAMLArgdataFactory::GetNull(const YAML::Mark& mark) {
  return argdata_t::null();
}

const argdata_t* YAMLArgdataFactory::GetScalar(const YAML::Mark& mark,
                                               std::string_view tag,
                                               std::string_view value) {
  // TODO(ed): Add support for booleans, integers, etc.
  if (tag == "tag:yaml.org,2002:str") {
    return argdatas_
        .emplace_back(argdata_t::create_str(strings_.emplace_front(value)))
        .get();
  } else {
    return fallback_->GetScalar(mark, tag, value);
  }
}

const argdata_t* YAMLArgdataFactory::GetSequence(
    const YAML::Mark& mark, std::string_view tag,
    std::vector<const argdata_t*> elements) {
  if (tag == "tag:yaml.org,2002:seq") {
    return argdatas_
        .emplace_back(
            argdata_t::create_seq(lists_.emplace_front(std::move(elements))))
        .get();
  } else {
    return fallback_->GetSequence(mark, tag, std::move(elements));
  }
}

const argdata_t* YAMLArgdataFactory::GetMap(
    const YAML::Mark& mark, std::string_view tag,
    std::vector<const argdata_t*> keys, std::vector<const argdata_t*> values) {
  if (tag == "tag:yaml.org,2002:map") {
    return argdatas_
        .emplace_back(
            argdata_t::create_map(lists_.emplace_front(std::move(keys)),
                                  lists_.emplace_front(std::move(values))))
        .get();
  } else {
    return fallback_->GetMap(mark, tag, std::move(keys), std::move(values));
  }
}
