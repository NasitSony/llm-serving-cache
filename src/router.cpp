#include "cache/router.h"

namespace cache {

Router::Router(
    MetadataStore& metadata_store,
    NodeRegistry& node_registry
)
    : metadata_store_(metadata_store),
      node_registry_(node_registry)
{
}

std::optional<RoutingDecision> Router::RouteRequest(
    const std::string& session_id,
    const std::string& model_id,
    const std::string& prefix_hash
)
{
    // 1. Check existing session routing
    auto existing = metadata_store_.GetSessionRoute(session_id);

    if (existing.has_value()) {
        RoutingDecision decision;
        decision.node_id   = existing->node_id;
        decision.cache_hit = false;

        return decision;
    }

    // 2. Exact cache hit
    auto exact = metadata_store_.FindCacheEntry(model_id, prefix_hash);

    if (exact.has_value()) {
        SessionRoute route;
        route.session_id = session_id;
        route.model_id   = model_id;
        route.node_id    = exact->node_id;
        route.status     = "active";

        metadata_store_.AssignSession(route);

        RoutingDecision decision;
        decision.node_id   = exact->node_id;
        decision.cache_hit = true;

        return decision;
    }

    // 3. Prefix reuse
    auto prefix_match =
        metadata_store_.FindLongestPrefixMatch(model_id, prefix_hash);

    if (prefix_match.has_value()) {
        SessionRoute route;
        route.session_id = session_id;
        route.model_id   = model_id;
        route.node_id    = prefix_match->node_id;
        route.status     = "active";

        metadata_store_.AssignSession(route);

        RoutingDecision decision;
        decision.node_id   = prefix_match->node_id;
        decision.cache_hit = true;

        return decision;
    }

    // 4. Cache miss → choose available node
   // 4. Full miss → choose least-loaded node
auto nodes = node_registry_.ListAvailableNodesWithCapacity();

if (nodes.empty()) {
    return std::nullopt;
}

// pick least-loaded node
ServingNode best = nodes.front();

for (const auto& node : nodes) {
    if (node.used_capacity < best.used_capacity) {
        best = node;
    }
}

// assign session
SessionRoute route;
route.session_id = session_id;
route.model_id   = model_id;
route.node_id    = best.node_id;
route.status     = "active";

metadata_store_.AssignSession(route);

// return decision
RoutingDecision decision;
decision.node_id   = best.node_id;
decision.cache_hit = false;

return decision;
}

} // namespace cache