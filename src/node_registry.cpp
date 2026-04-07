#include "cache/node_registry.h"

namespace cache {

bool NodeRegistry::RegisterNode(const ServingNode& node) {
    nodes_[node.node_id] = node;
    return true;
}

bool NodeRegistry::MarkNodeUnavailable(const std::string& node_id) {
    auto it = nodes_.find(node_id);

    if (it == nodes_.end()) {
        return false;
    }

    it->second.available = false;
    return true;
}

bool NodeRegistry::IncrementUsedVram(
    const std::string& node_id,
    int mb
) {
    auto it = nodes_.find(node_id);
    if (it == nodes_.end()) {
        return false;
    }

    auto& node = it->second;
    if (node.used_vram_mb + mb > node.total_vram_mb) {
        return false;
    }

    node.used_vram_mb += mb;
    return true;
}

bool NodeRegistry::DecrementUsedVram(
    const std::string& node_id,
    int mb
) {
    auto it = nodes_.find(node_id);
    if (it == nodes_.end()) {
        return false;
    }

    auto& node = it->second;
    if (mb > node.used_vram_mb) {
        node.used_vram_mb = 0;
        return true;
    }

    node.used_vram_mb -= mb;
    return true;
}

std::optional<ServingNode> NodeRegistry::GetNode(
    const std::string& node_id
) const {
    auto it = nodes_.find(node_id);

    if (it == nodes_.end()) {
        return std::nullopt;
    }

    return it->second;
}

std::vector<ServingNode> NodeRegistry::ListAvailableNodes() const {
    std::vector<ServingNode> result;

    for (const auto& [_, node] : nodes_) {
        if (node.available) {
            result.push_back(node);
        }
    }

    return result;
}

std::vector<ServingNode> NodeRegistry::ListAvailableNodesWithCapacity() const {
    std::vector<ServingNode> result;

    for (const auto& [_, node] : nodes_) {
        if (node.available && node.used_capacity < node.capacity) {
            result.push_back(node);
        }
    }

    return result;
}

bool NodeRegistry::IncrementUsedCapacity(
    const std::string& node_id,
    int amount
) {
    auto it = nodes_.find(node_id);

    if (it == nodes_.end()) {
        return false;
    }

    it->second.used_capacity += amount;

    if (it->second.used_capacity > it->second.capacity) {
        it->second.used_capacity = it->second.capacity;
    }

    return true;
}

bool NodeRegistry::DecrementUsedCapacity(
    const std::string& node_id,
    int amount
) {
    auto it = nodes_.find(node_id);

    if (it == nodes_.end()) {
        return false;
    }

    it->second.used_capacity -= amount;

    if (it->second.used_capacity < 0) {
        it->second.used_capacity = 0;
    }

    return true;
}

} // namespace cache