#include <iostream>

#include "cache/cache_types.h"
#include "cache/kv_metadata_store.h"
#include "kv/kv_store.h"

static int estimate_kv_cache_mb(int tokens) {
    return tokens / 10;
}

int main() {
    kv::KVStore kv_store;

    if (!kv_store.open("/tmp/llm_cache_metadata.wal")) {
        std::cerr << "Failed to open KV store" << std::endl;
        return 1;
    }

    cache::KVMetadataStore metadata(kv_store);

    // --------------------------------------------------
    // Register a persistent cache entry
    // --------------------------------------------------
    cache::CacheEntry cache_entry{
        "llama-70b",
        "hello-my-name",
        "block-1",
        "node-a",
        1,
        1,
        estimate_kv_cache_mb(1000)
    };

    if (!metadata.RegisterCacheEntry(cache_entry)) {
        std::cerr << "Failed to register cache entry" << std::endl;
        return 1;
    }

    std::cout << "Registered cache entry:"
              << "\n  model_id      = " << cache_entry.model_id
              << "\n  prefix_hash   = " << cache_entry.prefix_hash
              << "\n  block_id      = " << cache_entry.block_id
              << "\n  node_id       = " << cache_entry.node_id
              << "\n";
    
    // --------------------------------------------------
    // Register a persistent session route
    // --------------------------------------------------
    cache::SessionRoute route{
        "session-1",
        "llama-70b",
        "node-a",
        "active"
    };

    if (!metadata.AssignSession(route)) {
        std::cerr << "Failed to assign session route" << std::endl;
        return 1;
    }

    std::cout << "\nRegistered session route:"
              << "\n  session_id    = " << route.session_id
              << "\n  model_id      = " << route.model_id
              << "\n  node_id       = " << route.node_id
              << "\n  status        = " << route.status
              << "\n";

    // --------------------------------------------------
    // Exact cache lookup
    // --------------------------------------------------
    auto exact = metadata.FindCacheEntry("llama-70b", "hello-my-name");

    if (exact.has_value()) {
        std::cout << "\nRecovered exact cache entry:"
                  << "\n  model_id      = " << exact->model_id
                  << "\n  prefix_hash   = " << exact->prefix_hash
                  << "\n  block_id      = " << exact->block_id
                  << "\n  node_id       = " << exact->node_id
                  << "\n";
    } else {
        std::cerr << "Exact cache lookup failed" << std::endl;
        return 1;
    }

    // --------------------------------------------------
    // Prefix reuse lookup
    // --------------------------------------------------
    auto prefix_match =
        metadata.FindLongestPrefixMatch("llama-70b", "hello-my-name-is-nasit");

    if (prefix_match.has_value()) {
        std::cout << "\nRecovered prefix-match cache entry:"
                  << "\n  model_id      = " << prefix_match->model_id
                  << "\n  prefix_hash   = " << prefix_match->prefix_hash
                  << "\n  block_id      = " << prefix_match->block_id
                  << "\n  node_id       = " << prefix_match->node_id
                  << "\n";
    } else {
        std::cerr << "Prefix-match lookup failed" << std::endl;
        return 1;
    }

    // --------------------------------------------------
    // Session route lookup
    // --------------------------------------------------
    auto recovered_route = metadata.GetSessionRoute("session-1");

    if (recovered_route.has_value()) {
        std::cout << "\nRecovered session route:"
                  << "\n  session_id    = " << recovered_route->session_id
                  << "\n  model_id      = " << recovered_route->model_id
                  << "\n  node_id       = " << recovered_route->node_id
                  << "\n  status        = " << recovered_route->status
                  << "\n";
    } else {
        std::cerr << "Session route lookup failed" << std::endl;
        return 1;
    }

    if (!kv_store.flush_wal()) {
       std::cerr << "Failed to flush WAL" << std::endl;
       return 1;
    }

    std::cout << "\nPersistent metadata demo completed successfully."
              << std::endl;

    return 0;
}