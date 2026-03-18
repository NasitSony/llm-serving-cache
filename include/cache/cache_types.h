#pragma once

#include <cstdint>
#include <string>

namespace cache {

struct CacheEntry {
    std::string model_id;
    std::string prefix_hash;
    std::string block_id;
    std::string node_id;

    std::uint64_t created_at_ms{0};
    std::uint64_t last_access_ms{0};
};

struct SessionRoute {
    std::string session_id;
    std::string model_id;
    std::string node_id;
    std::string status;
};

struct ServingNode {
    std::string node_id;
    std::string address;

    int capacity{0};
    int used_capacity{0};

    bool available{true};
};

struct RoutingDecision {
    std::string node_id;
    bool cache_hit{false};
};

} // namespace cache