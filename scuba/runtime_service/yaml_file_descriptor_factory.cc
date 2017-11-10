// Copyright (c) 2017 Nuxi, https://nuxi.nl/
//
// SPDX-License-Identifier: BSD-2-Clause

#include <algorithm>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <arpc++/arpc++.h>
#include <flower/protocol/switchboard.ad.h>
#include <yaml-cpp/exceptions.h>
#include <yaml-cpp/mark.h>
#include <argdata.hpp>

#include <scuba/runtime_service/yaml_file_descriptor_factory.h>

using arpc::ClientContext;
using arpc::Status;
using flower::protocol::switchboard::ConstrainRequest;
using flower::protocol::switchboard::ConstrainResponse;
using flower::protocol::switchboard::Right;
using scuba::runtime_service::YAMLFileDescriptorFactory;

const argdata_t* YAMLFileDescriptorFactory::GetNull(const YAML::Mark& mark) {
  return fallback_->GetNull(mark);
}

const argdata_t* YAMLFileDescriptorFactory::GetScalar(const YAML::Mark& mark,
                                                      std::string_view tag,
                                                      std::string_view value) {
  if (tag == "tag:nuxi.nl,2015:cloudabi/kubernetes/container_log") {
    return argdatas_.emplace_back(argdata_t::create_fd(container_log_->get()))
        .get();
  } else if (tag == "tag:nuxi.nl,2015:cloudabi/kubernetes/mount") {
    auto lookup = mounts_->find(value);
    if (lookup == mounts_->end())
      throw YAML::ParserException(mark, "Unknown volume mount");
    return argdatas_.emplace_back(argdata_t::create_fd(lookup->second.get()))
        .get();
  } else {
    return fallback_->GetScalar(mark, tag, value);
  }
}

const argdata_t* YAMLFileDescriptorFactory::GetSequence(
    const YAML::Mark& mark, std::string_view tag,
    std::vector<const argdata_t*> elements) {
  return fallback_->GetSequence(mark, tag, std::move(elements));
}

const argdata_t* YAMLFileDescriptorFactory::GetMap(
    const YAML::Mark& mark, std::string_view tag,
    std::vector<const argdata_t*> keys, std::vector<const argdata_t*> values) {
  if (tag == "tag:nuxi.nl,2015:cloudabi/kubernetes/server") {
    // Constraints to be placed on the switchboard connection that is
    // provided to the running process.
    ConstrainRequest request;
    request.add_rights(Right::SERVER_START);
    auto labels = request.mutable_in_labels();
    (*labels)["server_kubernetes_namespace"] = pod_metadata_->namespace_();
    (*labels)["server_kubernetes_pod_name"] = pod_metadata_->name();
    (*labels)["server_kubernetes_pod_attempt"] =
        std::to_string(pod_metadata_->attempt());
    (*labels)["server_kubernetes_container_name"] = container_metadata_->name();
    (*labels)["server_kubernetes_container_attempt"] =
        std::to_string(container_metadata_->attempt());
    for (size_t i = 0; i < keys.size(); ++i) {
      std::optional<std::string_view> key = keys[i]->get_str();
      std::optional<std::string_view> value = values[i]->get_str();
      if (!key || !value)
        throw YAML::ParserException(
            mark, "Switchboard label keys and values must be strings");
      if (!labels->emplace(*key, *value).second) {
        std::ostringstream ss;
        ss << "Attempted to override predefined label \"" << *key << '"';
        throw YAML::ParserException(mark, ss.str());
      }
    }

    // Request a new switchboard connection.
    ClientContext context;
    ConstrainResponse response;
    if (Status status =
            switchboard_servers_->Constrain(&context, request, &response);
        !status.ok())
      throw YAML::ParserException(
          mark, std::string("Failed to constrain switchboard channel: ") +
                    status.error_message());
    if (!response.switchboard())
      throw YAML::ParserException(
          mark, "Switchboard did not return a file descriptor");

    return argdatas_
        .emplace_back(argdata_t::create_fd(
            fds_.emplace_back(response.switchboard())->get()))
        .get();
  } else {
    return fallback_->GetMap(mark, tag, std::move(keys), std::move(values));
  }
}
