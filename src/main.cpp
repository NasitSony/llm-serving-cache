#include <iostream>

#include "cache/cache_types.h"
#include "cache/coordinator.h"
#include "cache/metadata_store.h"
#include "cache/node_registry.h"
#include "cache/placement_policy.h"
#include "cache/router.h"

int main() {

  cache::MetadataStore metadata;
  cache::NodeRegistry nodes;
  cache::PlacementPolicy placement;

  cache::Router router(metadata, nodes);
  cache::Coordinator coordinator(metadata, nodes, router, placement);

  cache::ServingNode node_a{
      "node-a",
      "127.0.0.1:9001",
      100,
      0,
      true
  };

  cache::ServingNode node_b{
      "node-b",
      "127.0.0.1:9002",
      100,
      0,
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
      0
  };

  coordinator.RegisterCache(entry);

  // 1. Exact hit
  auto exact = coordinator.RouteRequest(
      "session-1",
      "llama-70b",
      "hello-my-name"
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
      "hello-my-name-is-nasit"
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
      "completely-different-prefix"
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
            0
        };

        coordinator.RegisterCache(new_entry);

        std::cout << "Registered new cache entry on: "
                  << miss->node_id << "\n";
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
      "completely-different-prefix"
   );

   if (after_fill.has_value()) {
      std::cout << "After fill routed to: "
              << after_fill->node_id << "\n";

     std::cout << "Cache hit: "
              << (after_fill->cache_hit ? "yes" : "no")
              << "\n";
   }

  return 0;
}