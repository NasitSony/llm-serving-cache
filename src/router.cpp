#include "cache/router.h"

namespace cache {

Router::Router(MetadataStore& metadata_store,
               NodeRegistry& node_registry)
    : metadata_store_(metadata_store),
      node_registry_(node_registry) {}

std::optional<RoutingDecision> Router::RouteRequest(
    const std::string& session_id,
    const std::string& model_id,
    const std::string& prefix_hash) {

  (void)session_id;

  auto entry = metadata_store_.FindCacheEntry(model_id, prefix_hash);

  if (entry.has_value()) {
    RoutingDecision decision;
    decision.node_id = entry->node_id;
    decision.cache_hit = true;
    return decision;
  }

  auto nodes = node_registry_.ListAvailableNodes();
  if (nodes.empty()) {
    return std::nullopt;
  }

  RoutingDecision decision;
  decision.node_id = nodes.front().node_id;
  decision.cache_hit = false;
  return decision;
}

}  // namespace cache