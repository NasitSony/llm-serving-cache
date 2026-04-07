#pragma once

#include <optional>
#include <string>

#include "cache/cache_types.h"
#include "cache/metadata_store.h"
#include "cache/node_registry.h"

namespace cache {

class Router {
public:
    Router(
        MetadataStore& metadata_store,
        NodeRegistry& node_registry
    );

    std::optional<RoutingDecision> RouteRequest(
        const std::string& session_id,
        const std::string& model_id,
        const std::string& prefix_hash,
         int tokens

    );

private:
    MetadataStore& metadata_store_;
    NodeRegistry& node_registry_;
};

} // namespace cache