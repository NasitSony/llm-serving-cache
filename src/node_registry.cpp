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

std::optional<ServingNode> NodeRegistry::GetNode(
    const std::string& node_id) const {

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

}  // namespace cache