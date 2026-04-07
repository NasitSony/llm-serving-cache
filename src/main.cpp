#include <iostream>

#include "cache/cache_types.h"
#include "cache/coordinator.h"
#include "cache/metadata_store.h"
#include "cache/node_registry.h"
#include "cache/placement_policy.h"
#include "cache/router.h"

static int estimate_kv_cache_mb(int tokens) {
    return tokens / 10;
}

int main() {

  cache::MetadataStore metadata;
  cache::NodeRegistry nodes;
  cache::PlacementPolicy placement;

  cache::Router router(metadata, nodes);
  cache::Coordinator coordinator(metadata, nodes, router, placement);

  cache::ServingNode node_a{
      "node-a",
      "127.0.0.1:9001",
      1,
      1,
      "A100",
      1000,
      700,
      true
  };

  cache::ServingNode node_b{
      "node-b",
      "127.0.0.1:9002",
      1,
      0,
      "L4",
      500,
      450,
      true
  };

  nodes.RegisterNode(node_a);
  nodes.RegisterNode(node_b);

  // Existing cache for a shorter prefix
  cache::CacheEntry entry{
      "llama-70b",
      "hello-my-name",
      "block-1",
      "node-a",
      0,
      0,
      estimate_kv_cache_mb(1000)
  };

  coordinator.RegisterCache(entry);

  // 1. Exact hit
  auto exact = coordinator.RouteRequest(
      "session-1",
      "llama-70b",
      "hello-my-name",
      100
  );

  if (exact.has_value()) {
    std::cout << "Exact request routed to: "
              << exact->node_id << "\n";
    std::cout << "Cache hit: "
              << (exact->cache_hit ? "yes" : "no")
              << "\n";
  }

  // 2. Longer request prefix -> should reuse shorter cached prefix
  auto prefix = coordinator.RouteRequest(
      "session-2",
      "llama-70b",
      "hello-my-name-is-nasit",
      100
  );

  if (prefix.has_value()) {
    std::cout << "Prefix-match request routed to: "
              << prefix->node_id << "\n";
    std::cout << "Cache hit: "
              << (prefix->cache_hit ? "yes" : "no")
              << "\n";
  }

  // 3. Full miss
  auto miss = coordinator.RouteRequest(
      "session-3",
      "llama-70b",
      "completely-different-prefix",
      1000
  );

  if (miss.has_value()) {
    std::cout << "Miss request routed to: "
              << miss->node_id << "\n";

    std::cout << "Cache hit: "
              << (miss->cache_hit ? "yes" : "no")
              << "\n";

    if (!miss->cache_hit) {
        cache::CacheEntry new_entry{
            "llama-70b",
            "completely-different-prefix",
            "block-2",
            miss->node_id,
            0,
            0,
            estimate_kv_cache_mb(1000)
        };

        coordinator.RegisterCache(new_entry);

        std::cout << "Registered new cache entry on: "
                  << miss->node_id << "\n";

        nodes.IncrementUsedVram(
            miss->node_id,
            new_entry.kv_size_mb
        );

       


        auto node_state = nodes.GetNode(miss->node_id);
        if (node_state.has_value()) {
             std::cout << miss->node_id << " used_vram_mb: "
              << node_state->used_vram_mb << "/"
              << node_state->total_vram_mb << "\n";
            }          
        }
        
       auto node_b_state = nodes.GetNode("node-b");

       if (node_b_state.has_value()) {
          std::cout << "node-b used_capacity: "
              << node_b_state->used_capacity
              << "/"
              << node_b_state->capacity
              << "\n";
        }
   }

   auto after_fill = coordinator.RouteRequest(
      "session-4",
      "llama-70b",
      "completely-different-prefix",
      5000
   );

   if (after_fill.has_value()) {
      std::cout << "After fill routed to: "
              << after_fill->node_id << "\n";

     std::cout << "Cache hit: "
              << (after_fill->cache_hit ? "yes" : "no")
              << "\n";
   }

   auto force_evict = coordinator.RouteRequest(
    "session-5",
    "llama-70b",
    "another-new-prefix",
    10000
);

if (force_evict.has_value()) {
    std::cout << "Eviction test routed to: "
              << force_evict->node_id << "\n";

    std::cout << "Cache hit: "
              << (force_evict->cache_hit ? "yes" : "no")
              << "\n";

    if (!force_evict->cache_hit) {
        cache::CacheEntry evicted_fill{
            "llama-70b",
            "another-new-prefix",
            "block-3",
            force_evict->node_id,
            0,
            0,
            estimate_kv_cache_mb(1000)
        };

        coordinator.RegisterCache(evicted_fill);

        std::cout << "Registered new cache entry for eviction test on: "
                  << force_evict->node_id << "\n";

        auto node_state = nodes.GetNode(force_evict->node_id);
        if (node_state.has_value()) {
            std::cout << force_evict->node_id << " used_capacity: "
                      << node_state->used_capacity << "/"
                      << node_state->capacity << "\n";
        }
    }
}

  return 0;
}