#include <iostream>
#include <chrono>

#include "cache/cache_types.h"        // Defines CacheEntry and SessionRoute data models
#include "cache/kv_metadata_store.h" // Metadata layer for cache and routing
#include "kv/kv_store.h"             // WAL-backed key-value storage

// Approximate KV cache memory usage from token count.
// In production systems, this depends on model architecture,
// number of layers, hidden size, and precision.
static int estimate_kv_cache_mb(int tokens) {
    return tokens / 10;
}

int main(int argc, char* argv[]) {


    bool populate_only = false;
    bool recover_only = false;
    int entries = 5000;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "--populate") {
            populate_only = true;
        }

        if (arg == "--recover") {
            recover_only = true;
        }

        if (arg.rfind("--entries=", 0) == 0) {
            entries = std::stoi(arg.substr(std::string("--entries=").size()));
        }
    }

    // --------------------------------------------------
    // Initialize persistent KV store (WAL-backed)
    // --------------------------------------------------
    kv::KVStore kv_store;

    auto startup_start = std::chrono::steady_clock::now();

    // Open the store using a WAL file for durability.
    // Ensures metadata survives process restarts and failures.
    if (!kv_store.open("/tmp/llm_cache_metadata.wal")) {
        std::cerr << "Failed to open KV store" << std::endl;
        return 1;
    }

    auto startup_end = std::chrono::steady_clock::now();
    auto startup_recovery_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        startup_end - startup_start
    ).count();

    std::cout << "[startup_recovery] open_ms=" << startup_recovery_ms << "\n";

    // --------------------------------------------------
    // Initialize metadata control plane
    // --------------------------------------------------
    // This layer encapsulates domain-specific operations such as:
    // - cache registration
    // - prefix lookup
    // - session routing
    cache::KVMetadataStore metadata(kv_store);


    //const int entries = 5000;

    if (!recover_only) {
    // 👇 THIS is your populate block

    auto populate_start = std::chrono::steady_clock::now();

    for (int i = 0; i < entries; i++) {
        cache::CacheEntry entry{
          "llama-70b",
          "prefix-" + std::to_string(i),
          "block-" + std::to_string(i),
          "node-a",
          0,
          0,
          estimate_kv_cache_mb(1000)
        };

        metadata.RegisterCacheEntry(entry);
    }

        auto populate_end = std::chrono::steady_clock::now();
        auto populate_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            populate_end - populate_start
        ).count();

        std::cout << "[populate] entries=" << entries
          << " write_ms=" << populate_ms << "\n";

    }      


     auto recover_start = std::chrono::steady_clock::now();

int recovered = 0;

for (int i = 0; i < entries; i++) {
    auto found = metadata.FindCacheEntry(
        "llama-70b",
        "prefix-" + std::to_string(i)
    );

    if (found.has_value()) {
        recovered++;
    }
}

auto recover_end = std::chrono::steady_clock::now();
auto recovery_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
    recover_end - recover_start
).count();

std::cout << "[recovery_lookup] entries=" << entries
          << " recovered=" << recovered
          << " lookup_ms=" << recovery_ms << "\n";     

    // --------------------------------------------------
    // Register a persistent cache entry
    // --------------------------------------------------
    // Represents a reusable KV cache block associated with a prompt prefix.
   /* cache::CacheEntry cache_entry{
        "llama-70b",                 // model_id (cache is model-specific)
        "hello-my-name",             // prefix key (currently stored as raw string)
        "block-1",                   // cache block identifier
        "node-a",                    // node where the cache resides
        1,                           // transformer layer index
        1,                           // block index within the layer
        estimate_kv_cache_mb(1000)   // estimated KV cache size (MB)
    };

    // Persist cache metadata.
    if (!metadata.RegisterCacheEntry(cache_entry)) {
        std::cerr << "Failed to register cache entry" << std::endl;
        return 1;
    }

    std::cout << "Registered cache entry:"
              << "\n  model_id    = " << cache_entry.model_id
              << "\n  prefix_key  = " << cache_entry.prefix_hash
              << "\n  block_id    = " << cache_entry.block_id
              << "\n  node_id     = " << cache_entry.node_id
              << "\n"; */

    // --------------------------------------------------
    // Register a persistent session route
    // --------------------------------------------------
    // Maps a session to a specific node to preserve cache locality.
  /*  cache::SessionRoute route{
        "session-1",   // session identifier
        "llama-70b",   // model_id
        "node-a",      // assigned node
        "active"       // session status (should ideally be an enum)
    };

    // Persist session-to-node mapping.
    if (!metadata.AssignSession(route)) {
        std::cerr << "Failed to assign session route" << std::endl;
        return 1;
    }

    std::cout << "\nRegistered session route:"
              << "\n  session_id  = " << route.session_id
              << "\n  model_id    = " << route.model_id
              << "\n  node_id     = " << route.node_id
              << "\n  status      = " << route.status
              << "\n";

    // --------------------------------------------------
    // Exact cache lookup
    // --------------------------------------------------
    // Retrieves cache entry for identical model + prefix.
    // Represents a full cache hit.
    auto exact = metadata.FindCacheEntry("llama-70b", "hello-my-name");

    if (exact.has_value()) {
        std::cout << "\nRecovered exact cache entry:"
                  << "\n  model_id    = " << exact->model_id
                  << "\n  prefix_key  = " << exact->prefix_hash
                  << "\n  block_id    = " << exact->block_id
                  << "\n  node_id     = " << exact->node_id
                  << "\n";
    } else {
        std::cerr << "Exact cache lookup failed" << std::endl;
        return 1;
    }*/

    // --------------------------------------------------
    // Longest-prefix cache lookup
    // --------------------------------------------------
    // Finds the longest cached prefix that matches the query.
    // Enables partial reuse of KV cache to reduce recomputation.
    /*auto prefix_match =
        metadata.FindLongestPrefixMatch("llama-70b", "hello-my-name-is-nasit");

    if (prefix_match.has_value()) {
        std::cout << "\nRecovered prefix-match cache entry:"
                  << "\n  model_id    = " << prefix_match->model_id
                  << "\n  prefix_key  = " << prefix_match->prefix_hash
                  << "\n  block_id    = " << prefix_match->block_id
                  << "\n  node_id     = " << prefix_match->node_id
                  << "\n";
    } else {
        std::cerr << "Prefix-match lookup failed" << std::endl;
        return 1;
    }

    // --------------------------------------------------
    // Session route lookup
    // --------------------------------------------------
    // Confirms that session routing metadata is recoverable.
    auto recovered_route = metadata.GetSessionRoute("session-1");

    if (recovered_route.has_value()) {
        std::cout << "\nRecovered session route:"
                  << "\n  session_id  = " << recovered_route->session_id
                  << "\n  model_id    = " << recovered_route->model_id
                  << "\n  node_id     = " << recovered_route->node_id
                  << "\n  status      = " << recovered_route->status
                  << "\n";
    } else {
        std::cerr << "Session route lookup failed" << std::endl;
        return 1;
    }*/

    // --------------------------------------------------
    // Flush WAL (ensure durability)
    // --------------------------------------------------
    // Forces all pending writes to persistent storage.

    if (populate_only) {

        if (!kv_store.flush_wal()) {
           std::cerr << "Failed to flush WAL" << std::endl;
           return 1;
        }

        std::cout << "\nPersistent metadata demo completed successfully."
              << std::endl;

    }          

    return 0;
}