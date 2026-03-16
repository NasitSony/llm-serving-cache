#pragma once

#include <optional>
#include <string>

#include "cache/metadata_store.h"
#include "cache/node_registry.h"

namespace cache {

class Router {
 public:
  Router(MetadataStore& metadata_store,
         NodeRegistry& node_registry);

  std::optional<std::string> RouteRequest(
      const std::string& session_id,
      const std::string& model_id,
      const std::string& prefix_hash);

 private:
  MetadataStore& metadata_store_;
  NodeRegistry& node_registry_;
};

}  // namespace cache