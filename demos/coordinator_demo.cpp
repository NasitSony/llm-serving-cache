#include "cache/coordinator.h"

namespace cache {

Coordinator::Coordinator(MetadataStore& metadata,
                         NodeRegistry& nodes,
                         Router& router,
                         PlacementPolicy& placement)
    : metadata_(metadata),
      nodes_(nodes),
      router_(router),
      placement_(placement) {}

bool Coordinator::RegisterCache(const CacheEntry& entry) {
  return metadata_.RegisterCacheEntry(entry);
}

std::optional<std::string> Coordinator::RouteRequest(
    const std::string& session_id,
    const std::string& model_id,
    const std::string& prefix_hash) {


  int tokens = 1000; // mock value
  return router_.RouteRequest(session_id, model_id, prefix_hash, tokens);
}

}  // namespace cache