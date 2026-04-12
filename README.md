# 🚀 LLM Serving Cache

A lightweight **control-plane service for LLM KV-cache placement and routing.**

This system tracks where cached attention prefixes live across distributed inference nodes and routes requests to **maximize cache reuse and minimize recomputation.**

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

# 🎯 Why This Matters

This project models a **real AI inference control plane**, focusing on:

```bash
• reducing redundant inference computation
• maximizing cache reuse across nodes
• ensuring correctness under failure
• maintaining consistent distributed state
```

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
