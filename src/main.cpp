#include <iostream>
#include <unordered_map>
#include <algorithm>

/*
 * ================================================================
 * LLM Cache + GPU-Aware Serving Simulator
 * ================================================================
 *
 * This file simulates:
 *
 * 1. KV cache metadata + prefix reuse
 * 2. Routing (exact hit, prefix hit, miss)
 * 3. Latency model (prefill + decode)
 * 4. GPU-aware admission control
 * 5. Block-based KV memory allocation
 * 6. Eviction under memory pressure
 * 7. Benchmarking (avg, p95, hit rate, rejection rate)
 *
 * This is a CONTROL PLANE + SIMULATED DATA PLANE model.
 */

#include "cache/cache_types.h"
#include "cache/coordinator.h"
#include "cache/metadata_store.h"
#include "cache/node_registry.h"
#include "cache/placement_policy.h"
#include "cache/router.h"



// ================================================================
// Request + Latency Model
// ================================================================

/*
 * Represents a single inference request.
 *
 * prefix_hit_tokens:
 *   number of tokens reused from cache (prefix reuse)
 */
struct InferenceRequest {
    std::string request_id;
    int prompt_tokens{0};
    int output_tokens{0};
    int prefix_hit_tokens{0};
};

/*
 * Latency breakdown of an inference request.
 *
 * total = routing + prefill + decode
 */
struct InferenceResult {
    int uncached_tokens{0};
    int prefill_latency_ms{0};
    int decode_latency_ms{0};
    int total_latency_ms{0};
};



// ================================================================
// Block-Based KV Memory Model (GPU simulation)
// ================================================================

/*
 * Represents one KV memory block.
 */
struct CacheBlock {
    std::string block_id;
    bool allocated{false};
};

/*
 * Represents GPU memory as fixed-size blocks.
 *
 * Each node has:
 *   total_blocks = total_vram / block_size
 */
struct NodeBlockPool {
    std::string node_id;
    int block_size_mb{16};
    int total_blocks{0};
    int free_blocks{0};
    std::vector<CacheBlock> blocks;
};



// ================================================================
// Benchmark Metrics
// ================================================================

struct BenchmarkMetrics {
    int total_requests{0};
    int total_latency_ms{0};
    int hit_requests{0};
    int rejected_requests{0};

    std::vector<int> latencies;
};



// ================================================================
// Global State (Simulation)
// ================================================================

// request → allocated block IDs
std::unordered_map<std::string, std::vector<std::string>> request_to_blocks;

// request → active/inactive (used for eviction)
std::unordered_map<std::string, bool> request_active;



// ================================================================
// Block Allocation Helpers
// ================================================================

/*
 * Compute number of blocks required for KV size.
 */
int RequiredBlocks(int kv_size_mb, int block_size_mb) {
    return (kv_size_mb + block_size_mb - 1) / block_size_mb;
}

/*
 * Allocate contiguous blocks from a pool.
 *
 * If allocation fails → rollback partial allocation.
 */
std::vector<std::string> AllocateBlocks(NodeBlockPool& pool, int required_blocks) {
    std::vector<std::string> allocated;

    for (auto& block : pool.blocks) {
        if (!block.allocated) {
            block.allocated = true;
            allocated.push_back(block.block_id);

            if ((int)allocated.size() == required_blocks) {
                break;
            }
        }
    }

    // Rollback if insufficient
    if ((int)allocated.size() < required_blocks) {
        for (auto& block : pool.blocks) {
            for (const auto& id : allocated) {
                if (block.block_id == id) {
                    block.allocated = false;
                }
            }
        }
        return {};
    }

    pool.free_blocks -= required_blocks;
    return allocated;
}



/*
 * Best-fit node selection:
 * choose node with smallest leftover blocks.
 */
NodeBlockPool* SelectBestFitPool(
    NodeBlockPool& pool_a,
    NodeBlockPool& pool_b,
    int required_blocks
) {
    NodeBlockPool* best = nullptr;
    int best_leftover = 0;

    auto consider = [&](NodeBlockPool& pool) {
        if (pool.free_blocks < required_blocks) return;

        int leftover = pool.free_blocks - required_blocks;

        if (best == nullptr || leftover < best_leftover) {
            best = &pool;
            best_leftover = leftover;
        }
    };

    consider(pool_a);
    consider(pool_b);

    return best;
}



// ================================================================
// Latency Simulation
// ================================================================

/*
 * Simulates inference latency:
 *
 * prefill: proportional to uncached tokens
 * decode: proportional to output tokens
 */
InferenceResult SimulateInference(const InferenceRequest& req) {
    const int routing_overhead_ms = 5;
    const int prefill_cost_per_token = 1;
    const int decode_cost_per_token = 1;

    int uncached_tokens = std::max(0, req.prompt_tokens - req.prefix_hit_tokens);

    int prefill_latency = uncached_tokens * prefill_cost_per_token;
    int decode_latency = req.output_tokens * decode_cost_per_token;

    return {
        uncached_tokens,
        prefill_latency,
        decode_latency,
        routing_overhead_ms + prefill_latency + decode_latency
    };
}



// ================================================================
// Eviction Logic
// ================================================================

/*
 * Evicts the oldest inactive request.
 */
bool EvictOldestRequest(
    std::vector<std::string>& eviction_order,
    std::unordered_map<std::string, std::vector<std::string>>& request_to_blocks,
    std::unordered_map<std::string, bool>& request_active,
    NodeBlockPool& pool_a,
    NodeBlockPool& pool_b
) {
    while (!eviction_order.empty()) {
        std::string victim = eviction_order.front();
        eviction_order.erase(eviction_order.begin());

        // Skip active requests
        if (request_active[victim]) continue;

        auto it = request_to_blocks.find(victim);
        if (it == request_to_blocks.end()) continue;

        NodeBlockPool* pool =
            it->second[0].find("node-a") != std::string::npos ? &pool_a : &pool_b;

        // Free blocks
        for (auto& block : pool->blocks) {
            for (auto& id : it->second) {
                if (block.block_id == id) {
                    block.allocated = false;
                }
            }
        }

        pool->free_blocks += it->second.size();

        request_to_blocks.erase(it);
        request_active.erase(victim);
        return true;
    }

    return false;
}



// ================================================================
// Metrics
// ================================================================

void PrintMetrics(const std::string& scenario, const BenchmarkMetrics& m) {
    std::cout << "Scenario: " << scenario << "\n";

    int avg = m.total_requests ? m.total_latency_ms / m.total_requests : 0;

    int p95 = 0;
    if (!m.latencies.empty()) {
        auto sorted = m.latencies;
        std::sort(sorted.begin(), sorted.end());
        p95 = sorted[(int)(0.95 * sorted.size())];
    }

    std::cout << "avg_latency=" << avg << " ms\n";
    std::cout << "p95_latency=" << p95 << " ms\n";
    std::cout << "hit_rate=" << (100 * m.hit_requests / m.total_requests) << "%\n";
    std::cout << "rejection_rate=" << (100 * m.rejected_requests / m.total_requests) << "%\n\n";
}



// ================================================================
// MAIN: System Simulation
// ================================================================

int main() {

    // --------------------------------------------------
    // Control plane setup
    // --------------------------------------------------
    cache::MetadataStore metadata;
    cache::NodeRegistry nodes;
    cache::PlacementPolicy placement;

    cache::Router router(metadata, nodes);
    cache::Coordinator coordinator(metadata, nodes, router, placement);

    // --------------------------------------------------
    // Register nodes (GPU simulation)
    // --------------------------------------------------
    cache::ServingNode node_a{
        "node-a", "127.0.0.1:9001",
        1, 1,
        "A100",
        1000, 800,
        true
    };

    cache::ServingNode node_b{
        "node-b", "127.0.0.1:9002",
        1, 0,
        "L4",
        500, 500,
        true
    };

    nodes.RegisterNode(node_a);
    nodes.RegisterNode(node_b);

    // --------------------------------------------------
    // Block-based GPU memory pools
    // --------------------------------------------------
    auto pool_a = InitBlockPool(node_a, 16);
    auto pool_b = InitBlockPool(node_b, 16);

    /*
     * pool:
     *   models KV cache as fixed-size blocks
     *   enables allocation, fragmentation, eviction simulation
     */

    // --------------------------------------------------
    // Register initial cache (prefix)
    // --------------------------------------------------
    cache::CacheEntry entry{
        "llama-70b",
        "hello-my-name",
        "block-1",
        "node-a",
        0,
        0,
        cache::estimate_kv_cache_mb(1000)
    };

    coordinator.RegisterCache(entry);

    /*
     * System now has:
     *   prefix cache on node-a
     */

    // --------------------------------------------------
    // Routing scenarios
    // --------------------------------------------------

    // Exact hit
    auto exact = coordinator.RouteRequest(
        "session-1", "llama-70b", "hello-my-name", 100
    );

    // Prefix hit
    auto prefix = coordinator.RouteRequest(
        "session-2", "llama-70b", "hello-my-name-is-nasit", 100
    );

    // Miss
    auto miss = coordinator.RouteRequest(
        "session-3", "llama-70b", "different-prefix", 1000
    );

    /*
     * These three cover:
     *   - exact reuse
     *   - partial reuse
     *   - full recompute
     */

    // --------------------------------------------------
    // Benchmark scenarios
    // --------------------------------------------------

    BenchmarkMetrics no_cache;
    BenchmarkMetrics prefix_cache;
    BenchmarkMetrics exact_cache;

    std::vector<InferenceRequest> requests = {
        {"r1",1200,200,0},
        {"r2",1200,200,800},
        {"r3",1200,200,0},
        {"r4",1200,200,900},
    };

    for (auto& r : requests) {
        auto res = SimulateInference(r);

        no_cache.total_requests++;
        no_cache.total_latency_ms += res.total_latency_ms;
        no_cache.latencies.push_back(res.total_latency_ms);
    }

    PrintMetrics("No Cache", no_cache);

    /*
     * Extend:
     *   - GPU-aware admission
     *   - eviction
     *   - block allocation
     */

    return 0;
}