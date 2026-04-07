#include "cache/coordinator.h"

namespace cache {

Coordinator::Coordinator(
    MetadataStore& metadata,
    NodeRegistry& nodes,
    Router& router,
    PlacementPolicy& placement
)
    : metadata_(metadata),
      nodes_(nodes),
      router_(router),
      placement_(placement)
{
}

bool Coordinator::RegisterCache(const CacheEntry& entry) {
    const bool ok = metadata_.RegisterCacheEntry(entry);

    if (!ok) {
        return false;
    }

    // Simulate each cache block consuming 1 unit of capacity
    nodes_.IncrementUsedCapacity(entry.node_id, 1);

    return true;
}

std::optional<RoutingDecision> Coordinator::RouteRequest(
    const std::string& session_id,
    const std::string& model_id,
    const std::string& prefix_hash,
    int tokens
) {
    return router_.RouteRequest(session_id, model_id, prefix_hash,tokens);
}

} // namespace cache