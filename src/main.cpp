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

  // Register nodes
  cache::ServingNode node_a{
      "node-a",
      "127.0.0.1:9001",
      100,
      0,
      true};

  cache::ServingNode node_b{
      "node-b",
      "127.0.0.1:9002",
      100,
      0,
      true};

  nodes.RegisterNode(node_a);
  nodes.RegisterNode(node_b);

  // Register cache entry
  cache::CacheEntry entry{
      "llama-70b",
      "prefix-123",
      "block-1",
      "node-a",
      0,
      0};

  coordinator.RegisterCache(entry);

  // Route request
  auto node = coordinator.RouteRequest(
      "session-1",
      "llama-70b",
      "prefix-123");

  if (node.has_value()) {
    std::cout << "Request routed to: " << *node << std::endl;
  } else {
    std::cout << "No route found" << std::endl;
  }

  return 0;
}