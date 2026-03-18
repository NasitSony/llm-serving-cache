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
- available serving nodes
- capacity and utilization
- node liveness
```

### Router
Routes incoming inference requests to the correct node based on cache metadata.

### Placement Policy
Determines where new cache blocks should be stored.

## Example Metadata
Example cache entry:

```bash
model_id: llama-70b
prefix_hash: 9fa21ab
block_id: block-134
node_id: node-a
```

Example session route:

```bash
session_id: sess-101
model_id: llama-70b
node_id: node-a
```

## Project Structure
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

## Roadmap

**v0.1**
- In-memory metadata store
- Cache entry registration
- Session routing
- Node registry

**v0.2**
- Placement policies
- Prefix-aware routing

**v0.3**
- Node failure handling

**v0.4**
- Persistent metadata backend

## Status

Early prototype.
