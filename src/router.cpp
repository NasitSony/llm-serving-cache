#include "cache/router.h"

namespace cache {

Router::Router(MetadataStore& metadata_store,
               NodeRegistry& node_registry)
    : metadata_store_(metadata_store),
      node_registry_(node_registry) {}

std::optional<std::string> Router::RouteRequest(
    const std::string& session_id,
    const std::string& model_id,
    const std::string& prefix_hash) {

  // Check for cache hit
  auto entry = metadata_store_.FindCacheEntry(model_id, prefix_hash);

  if (entry.has_value()) {
    return entry->node_id;
  }

  // No cache — choose first available node
  auto nodes = node_registry_.ListAvailableNodes();

  if (nodes.empty()) {
    return std::nullopt;
  }

  return nodes.front().node_id;
}

}  // namespace cache