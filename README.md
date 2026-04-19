# 🚀 LLM Serving Cache

## Overview

A simulated GPU-aware LLM inference system that models:

- KV cache reuse (exact + prefix)
- GPU memory constraints and admission control
- Block-based KV cache allocation
- Eviction under memory pressure
- End-to-end latency impact via benchmarking

Includes comparative benchmarks across cache strategies and GPU-aware admission policies.

## System Pipeline

```bash
Workload → Router → Cache Reuse → GPU Memory → Block Allocation → Eviction → Inference → Latency
```

## 🎯 Why This Matters

This project models a **real AI inference control plane**, focusing on:

```bash
• reducing redundant inference computation
• maximizing cache reuse across nodes
• ensuring correctness under failure
• maintaining consistent distributed state
```

# 📊 Benchmark Results



| Scenario               | Avg Latency (ms) | P95 Latency (ms) | Hit Rate | Rejection Rate |
|------------------------|------------------|------------------|----------|----------------|
| No Cache               | 1405             | 1405             | 0%       | 0%             |
| Prefix Reuse           | 985              | 1405             | 50%      | 0%             |
| Exact Cache            | 205              | 205              | 100%     | 0%             |
| GPU-Aware              | 843              | 1405             | 25%      | 25%            |
| GPU-Aware + Eviction   | 1895             | 4205             | 25%      | 0%             |


**Observation:**
- Exact cache reuse eliminates most prefill cost, resulting in the lowest latency.
- Prefix reuse provides partial improvement proportional to reused tokens.
- No cache represents the baseline with full prefill cost.
- Prefix reuse improves average latency, but tail latency remains high when misses are still present in the workload.
- GPU-aware admission reduces average latency for admitted requests, but introduces rejection under memory pressure. Prefix reuse improves average latency, while tail latency remains sensitive to misses.
- GPU-aware admission reduces average latency by rejecting oversized requests under memory pressure. Adding eviction lowers rejection rate, but increases average and tail latency by admitting previously rejected expensive requests.

# Key Insights

- Exact cache reuse reduces latency by ~85% vs no cache
- Prefix reuse improves average latency but not tail latency (p95 remains high)
- GPU-aware admission reduces latency by rejecting oversized requests
- Eviction reduces rejection but increases latency by admitting expensive requests



# 🧠 Motivation

Large Language Model inference can be significantly accelerated through **KV-cache reuse.**

In distributed serving systems, this introduces key challenges:

```bash
• Where are cached prefixes stored?
• Which nodes hold reusable KV blocks?
• How should requests be routed for maximum reuse?
• How do we avoid redundant recomputation?
```

This project implements a **centralized metadata-driven control plane** to solve these problems.


## 🔗 Part of the Veri* AI Infrastructure Stack

llm-serving-cache is the **inference serving layer** of a layered AI infrastructure stack:

| Layer | Project | Role |
|-------|---------|------|
| Storage | [VeriStore](https://github.com/NasitSony/VeriStore) | WAL durability, crash recovery, Raft replication, object storage |
| Inference Serving | **llm-serving-cache** (this project) | KV-cache placement and routing control plane, backed by VeriStore |
| Workload Orchestration | [Veriflow](https://github.com/NasitSony/Veriflow) | GPU-aware scheduler for training, inference, and evaluation jobs |

This project depends on **VeriStore** ([github.com/NasitSony/VeriStore](https://github.com/NasitSony/VeriStore)) as its durable metadata backend — all cache entries, session routes, and node registry state are persisted using VeriStore's WAL-backed KV engine, inheriting its crash-consistency and deterministic recovery guarantees.


# 🟠 KV Cache Memory Model

## Overview

This phase introduces a fixed-size block abstraction to model GPU memory for KV cache allocation.

Instead of tracking VRAM as a single value, memory is represented as discrete blocks, enabling realistic simulation of allocation, placement, and reclamation behavior in LLM serving systems.


## Design

### Block Abstraction

GPU memory is divided into fixed-size blocks:

```bash
block_size = 16 MB
```

### Per-Node Block Pool

Each node maintains a pool of blocks derived from its VRAM:

```bash
total_blocks = total_vram_mb / block_size
```

Each pool tracks:

- free blocks
- allocated blocks


### Allocation

For each request:

```bash
required_blocks = ceil(kv_size_mb / block_size)
```

Blocks are allocated from a selected node’s pool.


### Placement Policy

A *** best-fit strategy *** is used:

- consider nodes with sufficient free blocks
- compute leftover blocks after allocation
- select the node with the minimum leftover

### Request Mapping

Allocated blocks are tracked per request:

```bash
request_id → list of block_ids
```

This enables precise memory management and cleanup.

### Freeing 

On request completion:
- allocated blocks are released
- block pool state is restored


## Example: Best-Fit Allocation

```bash
Allocating request req-4
required_kv_mb=180 required_blocks=12

Candidate node-a: free_blocks=20 leftover_blocks=8
Candidate node-b: free_blocks=31 leftover_blocks=19

Selected pool=node-a reason=best_fit_blocks

allocated_blocks=[node-a-block-0,...,node-a-block-11]
node-a free_blocks=8 allocated_blocks=54
```


## Example: Freeing Blocks

```bash
Freed request req-4 blocks=[node-a-block-0,...,node-a-block-11]
node-a free_blocks=20 allocated_blocks=42
```


## Example: Allocation Failure

```bash
Allocating request req-5
required_kv_mb=600 required_blocks=38

Candidate node-a: free_blocks=20 rejected=insufficient_blocks
Candidate node-b: free_blocks=31 rejected=insufficient_blocks

Allocation failed: no node has enough free blocks
```


## Key Takeaways

- GPU memory is modeled as discrete allocation units
- Best-fit placement improves memory utilization
- Request-level tracking enables accurate allocation and freeing
- Failure handling ensures correctness under resource constraints


## Summary

This phase transforms the system from:

```bash
VRAM tracking → memory management system
```

and demonstrates how:

```bash
Request → KV size → Block allocation → Placement → Free
```

drive realistic GPU memory behavior in LLM serving systems.


# 🔴 Eviction + Pressure Handling

## Overview

This phase adds pressure handling to the KV cache block system.

When a request cannot be allocated because no node has enough free blocks, the system:

- triggers eviction
- skips active requests
- evicts the oldest inactive request
- retries allocation once
- rejects the request if eviction is still insufficient

This models realistic overload behavior in LLM serving systems.


## Pressure Flow

Under block pressure, the allocator follows this sequence:

```bash
Allocate request
→ insufficient free blocks
→ trigger eviction
→ skip active requests
→ evict oldest inactive request
→ retry allocation
→ reject if still insufficient
```


## Eviction Policy

A simple oldest-request policy is used as the initial eviction strategy.

**Behavior**
- requests are tracked in allocation order
- active requests are protected from eviction
- inactive requests are eligible for eviction
- stale eviction entries are skipped safely

## Example: Pressure Handling Under Overload

```bash
Triggering eviction...
Skipping active request req-1
Evicted oldest inactive request req-4 blocks=[node-a-block-0,node-a-block-1,node-a-block-2,node-a-block-3,node-a-block-4,node-a-block-5,node-a-block-6,node-a-block-7,node-a-block-8,node-a-block-9,node-a-block-10,node-a-block-11]
node-a free_blocks=20 allocated_blocks=42

Retrying allocation for req-5
Candidate node-a: free_blocks=20 required_blocks=38 rejected=insufficient_blocks
Candidate node-b: free_blocks=31 required_blocks=38 rejected=insufficient_blocks
Rejected request req-5: eviction insufficient
```


## Key Behaviors

- **Eviction is demand-driven**
- **Triggered only when allocation fails under pressure**
- **Active requests are protected**
- **Prevents reclaiming memory from in-use allocations**
- **Eviction is safe**
- **Stale queue entries are skipped without corrupting allocator state**
- **Rejection is explicit**
- **If eviction cannot free enough space, the request is rejected cleanly**


## Summary

This phase makes the system behave more realistically under overload:

```bash
Pressure → Eviction → Retry → Reject
```

It demonstrates how a cache-aware memory system can preserve correctness while responding to GPU memory pressure.


# ⚙️ Core Components

### Metadata Store

Tracks:

```bash
• cache entries (prefix → node mapping)
• session routing decisions
• cache access patterns (for eviction)
```

### Node Registry

Maintains: 

```bash
• available serving nodes
• capacity and utilization
• node liveness
```

### Router

Responsible for request routing:

```bash
exact cache hit        → reuse cached result
prefix match           → reuse partial computation
cache miss             → select node + trigger cache fill
```

### Coordinator

Orchestrates control-plane operations:

```bash
• cache registration
• node capacity updates
• routing delegation
• lifecycle management
```

### Placement Policy

Determines where new cache blocks are placed:

```bash
• least-loaded node selection
• capacity-aware routing
```

### GPU-Aware KV Cache Management

```bash
• KV cache size estimated per request
• routing constrained by GPU memory
• cache allocation consumes VRAM
• eviction reclaims VRAM
• requests rejected when memory insufficient
```bash

# 🔁 Cache Lifecycle

```bash
Client Request
      ↓
Router
      ↓
Session Affinity (if exists)
      ↓
Exact Cache Hit?
      ↓
Prefix Match?
      ↓
Cache Miss
      ↓
Node Selection (least-loaded)
      ↓
[If full] Evict old cache entry
      ↓
Route request to node
      ↓
Inference completes
      ↓
Register new cache entry
      ↓
Update node capacity
      ↓
KV-backed Metadata Store (WAL)
```


## Overview

This project includes a mock inference engine that simulates the latency impact of KV cache reuse in LLM serving systems.

The simulation models:

• routing overhead
• prefill (prompt processing) cost
• decode (generation) cost
• cache reuse via prefix matching

## Latency Model

Each request is defined by:

• prompt_tokens
• output_tokens
• prefix_hit_tokens

Derived:

```bash
uncached_tokens = prompt_tokens - prefix_hit_tokens
```

Latency is computed as:

```bash
prefill_latency  = uncached_tokens × prefill_cost
decode_latency   = output_tokens × decode_cost

total_latency =
    routing_overhead +
    prefill_latency +
    decode_latency
```

## Workload

A mixed workload of requests is simulated, including:

• cache misses (no reuse)
• prefix hits (partial reuse)

Routing decisions determine cache reuse, which directly affects latency.

## Results

```bash
Eligible nodes: none
Rejected request: insufficient VRAM across all nodes required_kv_mb=1000

Average latency: 980 ms
Hit rate: 50%
Average hit latency: 555 ms
Average miss latency: 1405 ms
```
## Analysis

• Cache hits significantly reduce latency (~2.5× lower than misses)
• Prefill cost dominates latency for cache misses
• Prefix reuse reduces uncached tokens, lowering total latency
• Overall system latency improves with higher cache hit rates

## Key Takeaways

• KV cache reuse is critical for efficient LLM serving
• Routing decisions impact end-to-end latency, not just placement
• GPU memory constraints influence both admission and performance
• Even simple cache-aware policies yield substantial gains

## Summary

This simulation demonstrates how:

```bash
Routing → Cache reuse → GPU memory → Latency
```

are tightly coupled in LLM serving systems.







# ✨ Features Implemented

```bash
- ✅ Exact cache lookup
- ✅ Longest-prefix cache reuse
- ✅ Session-affinity routing
- ✅ Cache fill after miss
- ✅ Node capacity tracking
- ✅ Capacity-aware placement
- ✅ Eviction on full node (LRU-style via access time)
- ✅ WAL-backed persistent metadata
- ✅ Crash recovery via deterministic WAL replay
```
---

# 📦 Example Metadata

### Cache Entry

```bash
model_id: llama-70b
prefix_hash: 9fa21ab
block_id: block-134
node_id: node-a
```
### Session Route

```bash
session_id: sess-101
model_id: llama-70b
node_id: node-a
```

# 🧱 Project Structure

```bash
llm-serving-cache/
├── CMakeLists.txt
├── README.md
│
├── include/
│   └── cache/
│       ├── cache_types.h
│       ├── metadata_store.h
│       ├── placement_policy.h
│       ├── router.h
│       ├── node_registry.h
│       └── coordinator.h
│
├── src/
│   ├── metadata_store.cpp
│   ├── placement_policy.cpp
│   ├── router.cpp
│   ├── node_registry.cpp
│   ├── coordinator.cpp
│   └── main.cpp
│
├── demos/
│   ├── cache_register_demo.cpp
│   ├── routing_demo.cpp
│   └── node_failure_demo.cpp
│
├── tests/
│   ├── metadata_store_test.cpp
│   ├── routing_test.cpp
│   └── placement_policy_test.cpp
│
└── docs/
    ├── architecture.md
    ├── api.md
    └── roadmap.md
```

# 🧪 Example Output

```bash
Exact request routed to: node-a
Cache hit: yes

Prefix-match request routed to: node-a
Cache hit: yes

Miss request routed to: node-b
Cache hit: no

Registered new cache entry on: node-b
node-b used_capacity: 1/1

After fill routed to: node-b
Cache hit: yes

Evicted cache block from node: node-b
```

# 🗺️ Roadmap

### v0.1 — In-Memory Control Plane

```bash
• In-memory metadata store
• Cache registration
• Session routing
• Node registry
```

### v0.2 — Routing & Cache Reuse

```bash
• Exact cache hits
• Prefix-aware routing
• Session-affinity routing
• Cache fill after miss
```

### v0.3 — Capacity & Eviction

```bash
• Node capacity tracking
• Capacity-aware placement
• Eviction on full nodes
• Complete cache lifecycle
```

### v0.4 — Persistent Metadata Backend

```bash
• KV-backed metadata store using KV Shuttle
• WAL-based durability for cache and session metadata
• Crash recovery via WAL replay
• Deterministic reconstruction after restart
```

### v0.5 — Distributed Control Plane (Future)

```bash
• Raft-backed metadata replication
• Leader-based coordination
• Fault-tolerant cache placement decisions
```






## Output of Phase 5

comparative results  
meaningful numbers  



# 📌 Status

```bash
v0.4 complete — persistent, crash-consistent control plane prototype
```


---

# 🔗 Related Work

This project builds on:

```bash
- - VeriStore — WAL-based, crash-consistent KV storage engine with Raft replication
  https://github.com/NasitSony/VeriStore
```
