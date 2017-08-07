// Copyright (c) 2017 Nuxi, https://nuxi.nl/
//
// This file is distributed under a 2-clause BSD license.
// See the LICENSE file for details.

#ifndef SCUBA_RUNTIME_SERVICE_YAML_BUILDER_H
#define SCUBA_RUNTIME_SERVICE_YAML_BUILDER_H

#include <cassert>
#include <sstream>
#include <string>
#include <variant>
#include <vector>

#include <yaml-cpp/anchor.h>
#include <yaml-cpp/emitterstyle.h>
#include <yaml-cpp/eventhandler.h>
#include <yaml-cpp/exceptions.h>
#include <yaml-cpp/mark.h>
#include <yaml-cpp/parser.h>

#include <scuba/runtime_service/yaml_factory.h>

namespace scuba {
namespace runtime_service {

template <typename T>
class YAMLBuilder : YAML::EventHandler {
 public:
  explicit YAMLBuilder(YAMLFactory<T>* factory) : factory_(factory), root_() {
  }

  T Build(const std::string& input) {
    std::istringstream stream(input);
    YAML::Parser parser(stream);
    root_ = factory_->GetNull({});
    parser.HandleNextDocument(*this);
    assert(stack_.empty() &&
           "Composite structures remain on the parsing stack");
    return std::move(root_);
  }

 private:
  struct Sequence {
    YAML::Mark mark;
    std::string tag;
    std::vector<T> elements;
  };

  struct Map {
    YAML::Mark mark;
    std::string tag;
    std::vector<T> keys;
    std::vector<T> values;
  };

  YAMLFactory<T>* const factory_;

  std::vector<std::variant<Sequence, Map>> stack_;
  T root_;

  void OnDocumentStart(const YAML::Mark& mark) override {
  }

  void OnDocumentEnd() override {
  }

  void OnNull(const YAML::Mark& mark, YAML::anchor_t anchor) override {
    Append(factory_->GetNull(mark));
  }

  void OnAlias(const YAML::Mark& mark, YAML::anchor_t anchor) override {
    throw YAML::ParserException(mark, "Unsupported alias");
  }

  void OnScalar(const YAML::Mark& mark, const std::string& tag,
                YAML::anchor_t anchor, const std::string& value) override {
    Append(factory_->GetScalar(mark, tag, value));
  }

  void OnSequenceStart(const YAML::Mark& mark, const std::string& tag,
                       YAML::anchor_t anchor,
                       YAML::EmitterStyle::value style) override {
    stack_.emplace_back(Sequence{mark, tag, {}});
  }

  void OnSequenceEnd() override {
    Sequence sequence = std::move(std::get<Sequence>(stack_.back()));
    stack_.pop_back();
    Append(factory_->GetSequence(sequence.mark, sequence.tag,
                                 std::move(sequence.elements)));
  }

  void OnMapStart(const YAML::Mark& mark, const std::string& tag,
                  YAML::anchor_t anchor,
                  YAML::EmitterStyle::value style) override {
    stack_.emplace_back(Map{mark, tag, {}, {}});
  }

  void OnMapEnd() override {
    Map map = std::move(std::get<Map>(stack_.back()));
    stack_.pop_back();
    Append(factory_->GetMap(map.mark, map.tag, std::move(map.keys),
                            std::move(map.values)));
  }

  void Append(T node) {
    if (stack_.empty()) {
      root_ = std::move(node);
    } else {
      auto composite = &stack_.back();
      if (Sequence* sequence = std::get_if<Sequence>(composite)) {
        sequence->elements.push_back(std::move(node));
      } else {
        Map* map = std::get_if<Map>(composite);
        if (map->keys.size() == map->values.size())
          map->keys.push_back(std::move(node));
        else
          map->values.push_back(std::move(node));
      }
    }
  }
};

}  // namespace runtime_service
}  // namespace scuba

#endif
