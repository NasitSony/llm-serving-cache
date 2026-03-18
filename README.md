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
```

# ✨ Features Implemented

```bash
✔ Exact cache lookup
✔ Longest-prefix cache reuse
✔ Session-affinity routing
✔ Metadata-driven control plane
✔ Cache fill after miss
✔ Node capacity tracking
✔ Capacity-aware routing
✔ Eviction when node is full (LRU-style via access time)
```

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

### v0.1

```bash
• In-memory metadata store
• Cache entry registration
• Session routing
• Node registry
```

### v0.2

```bash
• Prefix-aware routing
• Placement policies
```

### v0.3

```bash
• Node capacity tracking
• Cache lifecycle management
• Eviction policy
```

### v0.4 (Next)

```bash
• Persistent metadata backend
• Distributed control plane (Raft-based)
```

# 🎯 Why This Matters

This project models a ***real LLM serving control plane***, focusing on:

```bash
• reducing redundant inference computation
• maximizing cache reuse across nodes
• maintaining consistency in distributed serving
• handling resource constraints safely
```

# 📌 Status

```bash
Prototype complete (v0.3)
Ready for extension into distributed / persistent control plane
```
