# LLM Serving Cache

A lightweight **metadata and control-plane service for LLM KV-cache placement and routing**.

This project tracks where cached attention prefixes are stored across inference nodes and routes requests to maximize cache reuse and reduce recomputation.

## Motivation

Large language model inference benefits significantly from **KV-cache reuse**.
However, in distributed serving environments it is necessary to track:
- where cached prefixes are stored
- which nodes hold cache blocks
- how to route sessions to the correct node

This system provides a **centralized metadata layer** for managing KV-cache placement and routing decisions.

## Core Components

### Metadata Store
Tracks cached KV blocks and session routes.

### Node Registry
Maintains a registry of available serving nodes.

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
├── include/cache/
├── src/
├── demos/
├── tests/
└── docs/
```
