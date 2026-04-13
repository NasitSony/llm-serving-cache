#include "cache/router.h"
#include "cache/placement_policy.h"
#include <iostream>

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
    std::cout << "Node check: " << node.node_id
              << " available=" << (node.available ? "yes" : "no")
              << " total_vram_mb=" << node.total_vram_mb
              << " used_vram_mb=" << node.used_vram_mb
              << " free_vram_mb=" << node.available_vram_mb()
              << " used_capacity=" << node.used_capacity
              << "/" << node.capacity
              << "\n";

    if (!node.available) {
        continue;
    }

    if (node.available_vram_mb() < required_kv_mb) {
        continue;
    }

    eligible_nodes.push_back(node);
}

if (eligible_nodes.empty()) {
    std::cout << "Eligible nodes: none\n";
    std::cout << "Rejected request: insufficient VRAM across all nodes"
              << " required_kv_mb=" << required_kv_mb
              << "\n";
    return std::nullopt;
}

std::cout << "Eligible nodes:\n";
for (const auto& node : eligible_nodes) {
    std::cout << " [" << node.node_id
              << " free_vram_mb=" << node.available_vram_mb()
              << " used_capacity=" << node.used_capacity
              << "/" << node.capacity
              << "]\n";
}
std::cout << "\n";

ServingNode best = eligible_nodes.front();


int best_leftover_vram = best.available_vram_mb() - required_kv_mb;



for (const auto& node : eligible_nodes) {
    int leftover_vram = node.available_vram_mb() - required_kv_mb;

    if (leftover_vram < best_leftover_vram ||
        (leftover_vram == best_leftover_vram &&
         node.used_capacity < best.used_capacity)) {
        best = node;
        best_leftover_vram = leftover_vram;
    }
}

    if (best.used_capacity >= best.capacity) {
    auto evicted = metadata_store_.EvictOne(best.node_id);

    if (evicted.has_value()) {
        node_registry_.DecrementUsedCapacity(best.node_id, 1);
        node_registry_.DecrementUsedVram(best.node_id, evicted->kv_size_mb);

        std::cout << "Evicted cache block from node: "
                  << best.node_id
                  << " freed_kv_mb=" << evicted->kv_size_mb
                  << "\n";

        auto node_state = node_registry_.GetNode(best.node_id);
        if (node_state.has_value()) {
            std::cout << best.node_id << " used_vram_mb: "
                      << node_state->used_vram_mb << "/"
                      << node_state->total_vram_mb << "\n";
        }
    } else {
        std::cout << "Rejected request: node full and nothing to evict\n";
        return std::nullopt;
    }
   }


    std::cout << "Selected node: " << best.node_id
          << " gpu=" << best.gpu_type
          << " free_vram_mb=" << best.available_vram_mb()
          << " required_kv_mb=" << required_kv_mb
          << " leftover_vram_mb=" << best_leftover_vram
          << " reason=best_fit_vram"
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