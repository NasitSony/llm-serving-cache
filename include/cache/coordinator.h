#pragma once

#include <optional>
#include <string>

#include "cache/cache_types.h"
#include "cache/metadata_store.h"
#include "cache/node_registry.h"
#include "cache/router.h"
#include "cache/placement_policy.h"

namespace cache {

class Coordinator {
 public:
  Coordinator(MetadataStore& metadata,
              NodeRegistry& nodes,
              Router& router,
              PlacementPolicy& placement);

  bool RegisterCache(const CacheEntry& entry);

  std::optional<RoutingDecision> RouteRequest(
      const std::string& session_id,
      const std::string& model_id,
      const std::string& prefix_hash);

 private:
  MetadataStore& metadata_;
  NodeRegistry& nodes_;
  Router& router_;
  PlacementPolicy& placement_;
};

}  // namespace cache