# LLM Cache Lifecycle

## Request Flow

Client Request
    ↓
Router
    ↓
Cache lookup

Cache hit
    → reuse existing KV cache

Cache miss
    → choose serving node
    → run inference
    → create cache block
    → register metadata
    → future requests become hits

## Concepts

- cache metadata
- routing decision
- session affinity
- cache lifecycle

🚀 Step 1: Add a strong README section (copy-paste ready)
🖥️ GPU-Aware KV Cache Management

This system implements a GPU-aware control plane for LLM KV-cache placement and lifecycle management.

Key Features
KV Cache Size Estimation
Estimates KV memory usage from request token length
Centralized via shared utility function
GPU-Aware Admission Control
Requests are routed only to nodes with sufficient available VRAM
Requests are rejected if no node can satisfy memory constraints
Best-Fit VRAM Placement
Among eligible nodes, selects the node with minimal leftover VRAM
Improves packing efficiency and reduces fragmentation
Dynamic VRAM Accounting
Cache fill increases node used_vram_mb
Eviction frees exact kv_size_mb
Node state evolves with workload
Explainable Routing Decisions
Logs include:
candidate nodes
VRAM availability
selection reason
rejection cause
📊 Example Output
Node check: node-b available=yes total_vram_mb=500 used_vram_mb=500 free_vram_mb=0 used_capacity=0/1
Node check: node-a available=yes total_vram_mb=1000 used_vram_mb=900 free_vram_mb=100 used_capacity=1/1

Eligible nodes:
 [node-a free_vram_mb=100 used_capacity=1/1]

Evicted cache block from node: node-a freed_kv_mb=100
node-a used_vram_mb: 800/1000

Selected node: node-a gpu=A100 free_vram_mb=100 required_kv_mb=100 leftover_vram_mb=0 reason=best_fit_vram

Registered new cache entry on: node-a
node-a used_vram_mb: 900/1000

Eligible nodes: none
Rejected request: insufficient VRAM across all nodes required_kv_mb=1000
💥 Step 2: One-line project positioning

Use this everywhere (resume / LinkedIn):

Built a GPU-aware LLM KV cache control plane with VRAM-based admission control, best-fit placement, and dynamic memory accounting across cache lifecycle.
