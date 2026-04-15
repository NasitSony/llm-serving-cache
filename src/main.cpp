#include <iostream>
#include <unordered_map>
#include <algorithm>

#include "cache/cache_types.h"
#include "cache/coordinator.h"
#include "cache/metadata_store.h"
#include "cache/node_registry.h"
#include "cache/placement_policy.h"
#include "cache/router.h"


struct InferenceRequest {
    std::string request_id;
    int prompt_tokens{0};
    int output_tokens{0};
    int prefix_hit_tokens{0};
};

struct InferenceResult {
    int uncached_tokens{0};
    int prefill_latency_ms{0};
    int decode_latency_ms{0};
    int total_latency_ms{0};
};


struct CacheBlock {
    std::string block_id;
    bool allocated{false};
};

struct NodeBlockPool {
    std::string node_id;
    int block_size_mb{16};
    int total_blocks{0};
    int free_blocks{0};
    std::vector<CacheBlock> blocks;
};

struct BenchmarkMetrics {
    int total_requests{0};
    int total_latency_ms{0};
    int hit_requests{0};
    int rejected_requests{0};

    std::vector<int> latencies;  // 👈 NEW
};

std::unordered_map<std::string, std::vector<std::string>> request_to_blocks;

int RequiredBlocks(int kv_size_mb, int block_size_mb) {
    return (kv_size_mb + block_size_mb - 1) / block_size_mb;
}

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

NodeBlockPool* SelectPoolForRequest(
    NodeBlockPool& pool_a,
    NodeBlockPool& pool_b,
    int required_blocks
) {
    std::cout << "Trying " << pool_a.node_id
              << ": free_blocks=" << pool_a.free_blocks
              << " required_blocks=" << required_blocks
              << "\n";

    if (pool_a.free_blocks >= required_blocks) {
        return &pool_a;
    }

    std::cout << "Trying " << pool_b.node_id
              << ": free_blocks=" << pool_b.free_blocks
              << " required_blocks=" << required_blocks
              << "\n";

    if (pool_b.free_blocks >= required_blocks) {
        return &pool_b;
    }

    return nullptr;
}

NodeBlockPool* SelectBestFitPool(
    NodeBlockPool& pool_a,
    NodeBlockPool& pool_b,
    int required_blocks
) {
    NodeBlockPool* best = nullptr;
    int best_leftover = 0;

    auto consider = [&](NodeBlockPool& pool) {
        if (pool.free_blocks < required_blocks) {
            std::cout << "Candidate " << pool.node_id
                      << ": free_blocks=" << pool.free_blocks
                      << " required_blocks=" << required_blocks
                      << " rejected=insufficient_blocks\n";
            return;
        }

        int leftover = pool.free_blocks - required_blocks;

        std::cout << "Candidate " << pool.node_id
                  << ": free_blocks=" << pool.free_blocks
                  << " leftover_blocks=" << leftover
                  << "\n";

        if (best == nullptr || leftover < best_leftover) {
            best = &pool;
            best_leftover = leftover;
        }
    };

    consider(pool_a);
    consider(pool_b);

    return best;
}


InferenceResult SimulateInference(const InferenceRequest& req) {
    const int routing_overhead_ms = 5;
    const int prefill_cost_per_token = 1;
    const int decode_cost_per_token = 1;

    int uncached_tokens = req.prompt_tokens - req.prefix_hit_tokens;
    if (uncached_tokens < 0) {
        uncached_tokens = 0;
    }

    int prefill_latency_ms = uncached_tokens * prefill_cost_per_token;
    int decode_latency_ms = req.output_tokens * decode_cost_per_token;
    int total_latency_ms =
        routing_overhead_ms + prefill_latency_ms + decode_latency_ms;

    return {
        uncached_tokens,
        prefill_latency_ms,
        decode_latency_ms,
        total_latency_ms
    };
}

NodeBlockPool InitBlockPool(const cache::ServingNode& node, int block_size_mb) {
    NodeBlockPool pool;
    pool.node_id = node.node_id;
    pool.block_size_mb = block_size_mb;
    pool.total_blocks = node.total_vram_mb / block_size_mb;
    pool.free_blocks = pool.total_blocks;

    for (int i = 0; i < pool.total_blocks; ++i) {
        pool.blocks.push_back(
            CacheBlock{node.node_id + "-block-" + std::to_string(i), false}
        );
    }

    return pool;
}

void FreeBlocks(NodeBlockPool& pool, const std::vector<std::string>& block_ids) {
    int freed = 0;

    for (auto& block : pool.blocks) {
        for (const auto& id : block_ids) {
            if (block.block_id == id && block.allocated) {
                block.allocated = false;
                freed++;
                break;
            }
        }
    }

    pool.free_blocks += freed;
    if (pool.free_blocks > pool.total_blocks) {
        pool.free_blocks = pool.total_blocks;
    }
}

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

        auto active_it = request_active.find(victim);
        if (active_it != request_active.end() && active_it->second) {
            std::cout << "Skipping active request " << victim << "\n";
            continue;
        }

        auto it = request_to_blocks.find(victim);
        if (it == request_to_blocks.end()) {
            continue;
        }

        NodeBlockPool* victim_pool = nullptr;
        if (!it->second.empty()) {
            if (it->second[0].find("node-a") != std::string::npos) {
                victim_pool = &pool_a;
            } else if (it->second[0].find("node-b") != std::string::npos) {
                victim_pool = &pool_b;
            }
        }

        if (victim_pool == nullptr) {
            continue;
        }

        FreeBlocks(*victim_pool, it->second);

        std::cout << "Evicted oldest inactive request " << victim << " blocks=[";
        for (size_t i = 0; i < it->second.size(); ++i) {
            std::cout << it->second[i];
            if (i + 1 < it->second.size()) std::cout << ",";
        }
        std::cout << "]\n";

        std::cout << victim_pool->node_id
                  << " free_blocks=" << victim_pool->free_blocks
                  << " allocated_blocks="
                  << (victim_pool->total_blocks - victim_pool->free_blocks)
                  << "\n";

        request_to_blocks.erase(it);
        request_active.erase(victim);
        return true;
    }

    return false;
}



int ComputePrefixHitTokens(
    bool cache_hit,
    bool is_prefix,
    int prompt_tokens
) {
    if (!cache_hit) {
        return 0;
    }

    if (is_prefix) {
        return prompt_tokens * 0.7;  // simulate partial reuse
    }

    // exact hit
    return prompt_tokens;
}

std::unordered_map<std::string, bool> request_active;

void PrintMetrics(const std::string& scenario, const BenchmarkMetrics& metrics) {
    std::cout << "Scenario: " << scenario << "\n";

    int avg_latency = 0;
    if (metrics.total_requests > 0) {
        avg_latency = metrics.total_latency_ms / metrics.total_requests;
    }

    int hit_rate = 0;
    int rejection_rate = 0;
    if (metrics.total_requests > 0) {
        hit_rate = (100 * metrics.hit_requests) / metrics.total_requests;
        rejection_rate = (100 * metrics.rejected_requests) / metrics.total_requests;
    }

    int p95_latency = 0;

    if (!metrics.latencies.empty()) {
       std::vector<int> sorted = metrics.latencies;
       std::sort(sorted.begin(), sorted.end());

       int index = static_cast<int>(0.95 * sorted.size());
       if (index >= sorted.size()) index = sorted.size() - 1;

       p95_latency = sorted[index];
    }

    std::cout << "avg_latency=" << avg_latency << " ms\n";
    std::cout << "p95_latency=" << p95_latency << " ms\n";
    std::cout << "hit_rate=" << hit_rate << "%\n";
    std::cout << "rejection_rate=" << rejection_rate << "%\n\n";
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
    800,
    true
};

cache::ServingNode node_b{
    "node-b",
    "127.0.0.1:9002",
    1,
    0,
    "L4",
    500,
    500,
    true
};

  nodes.RegisterNode(node_a);
  nodes.RegisterNode(node_b);



// 🔥 NEW BLOCK SYSTEM
auto pool_a = InitBlockPool(node_a, 16);
auto pool_b = InitBlockPool(node_b, 16);

std::cout << pool_a.node_id
          << " total_blocks=" << pool_a.total_blocks
          << " free_blocks=" << pool_a.free_blocks
          << " allocated_blocks=" << (pool_a.total_blocks - pool_a.free_blocks)
          << "\n";

std::cout << pool_b.node_id
          << " total_blocks=" << pool_b.total_blocks
          << " free_blocks=" << pool_b.free_blocks
          << " allocated_blocks=" << (pool_b.total_blocks - pool_b.free_blocks)
          << "\n";

// ⬇️ existing routing starts here


  auto a = nodes.GetNode("node-a");
  if (a.has_value()) {
    std::cout << "node-a init: gpu=" << a->gpu_type
              << " total_vram_mb=" << a->total_vram_mb
              << " used_vram_mb=" << a->used_vram_mb
              << " free_vram_mb=" << a->available_vram_mb()
              << " available=" << (a->available ? "yes" : "no")
              << "\n";
  }

  // Existing cache for a shorter prefix
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

    int prompt_tokens = 1200;

int reused = ComputePrefixHitTokens(
    prefix->cache_hit,
    true,  // prefix case
    prompt_tokens
);

InferenceRequest req{
    "req-prefix",
    prompt_tokens,
    200,
    reused
};

   

    auto res = SimulateInference(req);

    std::cout << "Latency (PREFIX HIT): total=" << res.total_latency_ms
              << " prefill=" << res.prefill_latency_ms
              << " decode=" << res.decode_latency_ms
              << " uncached_tokens=" << res.uncached_tokens
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

    
   int prompt_tokens = 1200;

   // MISS case → no reuse
   int reused = ComputePrefixHitTokens(
      false,
      false,
      prompt_tokens
   );

   InferenceRequest req{
      "req-miss",
      prompt_tokens,
      200,
      reused
  };

    auto res = SimulateInference(req);

    std::cout << "Latency (MISS): total=" << res.total_latency_ms
              << " prefill=" << res.prefill_latency_ms
              << " decode=" << res.decode_latency_ms
              << " uncached_tokens=" << res.uncached_tokens
              << "\n";          

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
            cache::estimate_kv_cache_mb(1000)
        };

        coordinator.RegisterCache(new_entry);

        std::cout << "Registered new cache entry on: "
                  << miss->node_id << "\n";

       
       // manual vram entry is removed
       // nodes.IncrementUsedVram(
         //   miss->node_id,
          //  new_entry.kv_size_mb
       // );

       


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
            cache::estimate_kv_cache_mb(1000)
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

/*std::vector<InferenceRequest> requests = {
    {"r1", 1200, 200, 0},   // miss
    {"r2", 1200, 200, 800}, // prefix
    {"r3", 1200, 200, 0},   // miss
    {"r4", 1200, 200, 900}, // prefix
};*/

int total_latency = 0;

std::vector<InferenceRequest> requests = {
    {"b1", 1200, 200, 0},
    {"b2", 1200, 200, 0},
    {"b3", 1200, 200, 0},
    {"b4", 1200, 200, 0}
};

BenchmarkMetrics no_cache_metrics;

for (const auto& req : requests) {
    InferenceRequest sim_req = req;
    sim_req.prefix_hit_tokens = 0;

    auto res = SimulateInference(sim_req);

    no_cache_metrics.total_requests++;
    no_cache_metrics.total_latency_ms += res.total_latency_ms;
    no_cache_metrics.latencies.push_back(res.total_latency_ms);
}

BenchmarkMetrics prefix_metrics;

for (size_t i = 0; i < requests.size(); ++i) {
    InferenceRequest sim_req = requests[i];

    if (i % 2 == 1) {
        sim_req.prefix_hit_tokens = sim_req.prompt_tokens * 0.7;
        prefix_metrics.hit_requests++;
    } else {
        sim_req.prefix_hit_tokens = 0;
    }

    auto res = SimulateInference(sim_req);

    prefix_metrics.total_requests++;
    prefix_metrics.total_latency_ms += res.total_latency_ms;
    prefix_metrics.latencies.push_back(res.total_latency_ms);
}

BenchmarkMetrics exact_cache_metrics;

for (const auto& req : requests) {
    InferenceRequest sim_req = req;

    // Exact cache hit: full prompt reused
    sim_req.prefix_hit_tokens = sim_req.prompt_tokens;
    exact_cache_metrics.hit_requests++;

    auto res = SimulateInference(sim_req);

    exact_cache_metrics.total_requests++;
    exact_cache_metrics.total_latency_ms += res.total_latency_ms;
    exact_cache_metrics.latencies.push_back(res.total_latency_ms);
}

PrintMetrics("No Cache", no_cache_metrics);
PrintMetrics("Exact Cache", exact_cache_metrics);
PrintMetrics("Prefix Reuse", prefix_metrics);


/*for (const auto& req : requests) {
    auto res = SimulateInference(req);
    total_latency += res.total_latency_ms;
}

std::cout << "Average latency: "
          << total_latency / requests.size()
          << " ms\n";*/

    int total_requests = 0;
     total_latency = 0;

    int hit_requests = 0;
    int miss_requests = 0;

    int hit_latency_sum = 0;
    int miss_latency_sum = 0;    


    for (const auto& req : requests){auto res = SimulateInference(req);

    total_requests++;
    total_latency += res.total_latency_ms;

    if (req.prefix_hit_tokens > 0) {
       hit_requests++;
       hit_latency_sum += res.total_latency_ms;
    } else {
      miss_requests++;
      miss_latency_sum += res.total_latency_ms;
    }   

    }

    std::cout << "Average latency: "
          << total_latency / total_requests
          << " ms\n";

std::cout << "Hit rate: "
          << (100 * hit_requests / total_requests)
          << "%\n";

if (hit_requests > 0) {
    std::cout << "Average hit latency: "
              << hit_latency_sum / hit_requests
              << " ms\n";
}

if (miss_requests > 0) {
    std::cout << "Average miss latency: "
              << miss_latency_sum / miss_requests
              << " ms\n";
}


std::vector<std::string> eviction_order;

int kv_size_mb = 100;
int required_blocks = RequiredBlocks(kv_size_mb, pool_a.block_size_mb);

auto allocated = AllocateBlocks(pool_a, required_blocks);

std::cout << "Allocating request req-1 on " << pool_a.node_id << "\n";
std::cout << "required_kv_mb=" << kv_size_mb
          << " required_blocks=" << required_blocks << "\n";

if (allocated.empty()) {
    std::cout << "Allocation failed\n";
} else {
    request_to_blocks["req-1"] = allocated;
    eviction_order.push_back("req-1");

    std::cout << "allocated_blocks=[";
    for (size_t i = 0; i < allocated.size(); ++i) {
        std::cout << allocated[i];
        if (i + 1 < allocated.size()) std::cout << ",";
    }
    std::cout << "]\n";

    std::cout << pool_a.node_id
              << " free_blocks=" << pool_a.free_blocks
              << " allocated_blocks=" << (pool_a.total_blocks - pool_a.free_blocks)
              << "\n";

    auto it = request_to_blocks.find("req-1");
    if (it != request_to_blocks.end()) {
        FreeBlocks(pool_a, it->second);

        std::cout << "Freed request req-1 blocks=[";
        for (size_t i = 0; i < it->second.size(); ++i) {
            std::cout << it->second[i];
            if (i + 1 < it->second.size()) std::cout << ",";
        }
        std::cout << "]\n";

        std::cout << pool_a.node_id
                  << " free_blocks=" << pool_a.free_blocks
                  << " allocated_blocks=" << (pool_a.total_blocks - pool_a.free_blocks)
                  << "\n";

        request_to_blocks.erase(it);
    }
}


int large_kv_mb = 1200;
int large_required_blocks = RequiredBlocks(large_kv_mb, pool_a.block_size_mb);

auto failed_alloc = AllocateBlocks(pool_a, large_required_blocks);

std::cout << "Allocating request req-2 on " << pool_a.node_id << "\n";
std::cout << "required_kv_mb=" << large_kv_mb
          << " required_blocks=" << large_required_blocks << "\n";

if (failed_alloc.empty()) {
    std::cout << "Allocation failed: insufficient free blocks\n";
    std::cout << pool_a.node_id
              << " free_blocks=" << pool_a.free_blocks
              << " allocated_blocks=" << (pool_a.total_blocks - pool_a.free_blocks)
              << "\n";
}


int req3_kv_mb = 180;
int req3_required_blocks = RequiredBlocks(req3_kv_mb, pool_a.block_size_mb);

std::cout << "\nAllocating request req-3\n";
std::cout << "required_kv_mb=" << req3_kv_mb
          << " required_blocks=" << req3_required_blocks
          << "\n";

// Optional: make node-a look tighter so node-b gets chosen
//pool_a.free_blocks = 5;

int req4_kv_mb = 180;
int req4_required_blocks = RequiredBlocks(req4_kv_mb, pool_a.block_size_mb);



std::cout << "\nAllocating request req-4\n";
std::cout << "required_kv_mb=" << req4_kv_mb
          << " required_blocks=" << req4_required_blocks
          << "\n";

pool_a.free_blocks = 20;
pool_b.free_blocks = 31;



NodeBlockPool* selected_pool =
    SelectBestFitPool(pool_a, pool_b, req4_required_blocks);

if (selected_pool == nullptr) {
    std::cout << "Allocation failed: no node has enough free blocks\n";
} else {
    std::cout << "Selected pool=" << selected_pool->node_id
              << " reason=best_fit_blocks\n";

    auto allocated_req4 = AllocateBlocks(*selected_pool, req4_required_blocks);
    request_active["req-1"] = true;
    request_active["req-4"] = false;

    if (allocated_req4.empty()) {
        std::cout << "Allocation failed on selected pool\n";
    } else {
        request_to_blocks["req-4"] = allocated_req4;
        eviction_order.push_back("req-4");

        std::cout << "allocated_blocks=[";
        for (size_t i = 0; i < allocated_req4.size(); ++i) {
            std::cout << allocated_req4[i];
            if (i + 1 < allocated_req4.size()) std::cout << ",";
        }
        std::cout << "]\n";

        std::cout << selected_pool->node_id
                  << " free_blocks=" << selected_pool->free_blocks
                  << " allocated_blocks="
                  << (selected_pool->total_blocks - selected_pool->free_blocks)
                  << "\n";

       /* auto it_req4 = request_to_blocks.find("req-4");
        if (it_req4 != request_to_blocks.end()) {
            FreeBlocks(*selected_pool, it_req4->second);

            std::cout << "Freed request req-4 blocks=[";
            for (size_t i = 0; i < it_req4->second.size(); ++i) {
                std::cout << it_req4->second[i];
                if (i + 1 < it_req4->second.size()) std::cout << ",";
            }
            std::cout << "]\n";

            std::cout << selected_pool->node_id
                      << " free_blocks=" << selected_pool->free_blocks
                      << " allocated_blocks="
                      << (selected_pool->total_blocks - selected_pool->free_blocks)
                      << "\n";

            //request_to_blocks.erase(it_req4);
        }*/
    }
}


std::cout << "\nAllocating request req-5\n";

int req5_kv_mb = 600;  // 600 / 16 -> 38 blocks
int req5_required_blocks = RequiredBlocks(req5_kv_mb, pool_a.block_size_mb);

std::cout << "required_kv_mb=" << req5_kv_mb
          << " required_blocks=" << req5_required_blocks
          << "\n";





NodeBlockPool* selected_pool_req5 =
    SelectBestFitPool(pool_a, pool_b, req5_required_blocks);

if (selected_pool_req5 == nullptr) {
    std::cout << "Allocation failed: no node has enough free blocks\n";

    std::cout << pool_a.node_id
              << " free_blocks=" << pool_a.free_blocks
              << " allocated_blocks="
              << (pool_a.total_blocks - pool_a.free_blocks)
              << "\n";

    std::cout << pool_b.node_id
              << " free_blocks=" << pool_b.free_blocks
              << " allocated_blocks="
              << (pool_b.total_blocks - pool_b.free_blocks)
              << "\n";

    std::cout << "Triggering eviction...\n";

    bool evicted = EvictOldestRequest(eviction_order, request_to_blocks, request_active, pool_a, pool_b);

    if (evicted) {
        std::cout << "Retrying allocation for req-5\n";

        selected_pool_req5 =
            SelectBestFitPool(pool_a, pool_b, req5_required_blocks);

        if (selected_pool_req5 == nullptr) {
            std::cout << "Rejected request req-5: eviction insufficient\n";
        } else {
            std::cout << "Selected pool=" << selected_pool_req5->node_id
                      << " reason=best_fit_blocks\n";

            auto allocated_req5 =
                AllocateBlocks(*selected_pool_req5, req5_required_blocks);

            if (allocated_req5.empty()) {
                std::cout << "Allocation failed on selected pool after eviction\n";
            } else {
                request_to_blocks["req-5"] = allocated_req5;
                eviction_order.push_back("req-5");

                std::cout << "allocated_blocks=[";
                for (size_t i = 0; i < allocated_req5.size(); ++i) {
                    std::cout << allocated_req5[i];
                    if (i + 1 < allocated_req5.size()) std::cout << ",";
                }
                std::cout << "]\n";

                std::cout << selected_pool_req5->node_id
                          << " free_blocks=" << selected_pool_req5->free_blocks
                          << " allocated_blocks="
                          << (selected_pool_req5->total_blocks - selected_pool_req5->free_blocks)
                          << "\n";
            }
        }
    } else {
        std::cout << "Rejected request req-5: no evictable request found\n";
    }
} else {
    std::cout << "Selected pool=" << selected_pool_req5->node_id
              << " reason=best_fit_blocks\n";

    auto allocated_req5 =
        AllocateBlocks(*selected_pool_req5, req5_required_blocks);

    if (allocated_req5.empty()) {
        std::cout << "Allocation failed on selected pool\n";
    } else {
        request_to_blocks["req-5"] = allocated_req5;
        eviction_order.push_back("req-5");

        std::cout << "allocated_blocks=[";
        for (size_t i = 0; i < allocated_req5.size(); ++i) {
            std::cout << allocated_req5[i];
            if (i + 1 < allocated_req5.size()) std::cout << ",";
        }
        std::cout << "]\n";

        std::cout << selected_pool_req5->node_id
                  << " free_blocks=" << selected_pool_req5->free_blocks
                  << " allocated_blocks="
                  << (selected_pool_req5->total_blocks - selected_pool_req5->free_blocks)
                  << "\n";
    }
}


  return 0;
}