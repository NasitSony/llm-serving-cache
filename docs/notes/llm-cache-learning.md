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