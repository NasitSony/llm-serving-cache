#include "cache/router.h"
#include <iostream>

namespace cache {

static int estimate_kv_cache_mb(int tokens) {
    return tokens / 10;
}

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
    const std::string& prefix_hash,
    int tokens
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

    // 4. Cache miss -> choose available GPU-aware node
    auto nodes = node_registry_.ListAvailableNodes();

    int required_kv_mb = estimate_kv_cache_mb(tokens);

    if (nodes.empty()) {
        return std::nullopt;
    }

    std::vector<ServingNode> eligible_nodes;
    for (const auto& node : nodes) {
        if (!node.available) {
            continue;
        }

        if (node.available_vram_mb() < required_kv_mb) {
            continue;
        }

        eligible_nodes.push_back(node);
    }

    if (eligible_nodes.empty()) {
    std::cout << "Rejected request: insufficient VRAM across all nodes"
              << " required_kv_mb=" << required_kv_mb
              << "\n";
    return std::nullopt;
}

    // pick least-loaded eligible node
    ServingNode best = eligible_nodes.front();

    for (const auto& node : eligible_nodes) {
        if (node.used_capacity < best.used_capacity) {
            best = node;
        }
    }

    if (best.used_capacity >= best.capacity) {
        auto evicted = metadata_store_.EvictOne(best.node_id);

        if (evicted.has_value()) {
            node_registry_.DecrementUsedCapacity(best.node_id, 1);
            std::cout << "Evicted cache block from node: "
                      << best.node_id << "\n";
        } else {
            std::cout << "Rejected request: node full and nothing to evict\n";
            return std::nullopt;
        }
    }

    std::cout << "Selected node: " << best.node_id
              << " gpu=" << best.gpu_type
              << " free_vram_mb=" << best.available_vram_mb()
              << " required_kv_mb=" << required_kv_mb
              << "\n";

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