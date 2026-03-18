#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "cache/cache_types.h"

namespace cache {

class NodeRegistry {
 public:
  bool RegisterNode(const ServingNode& node);

  bool MarkNodeUnavailable(const std::string& node_id);

  std::optional<ServingNode> GetNode(
      const std::string& node_id) const;

  std::vector<ServingNode> ListAvailableNodes() const;
  std::vector<ServingNode> ListAvailableNodesWithCapacity() const;

 private:
  std::unordered_map<std::string, ServingNode> nodes_;
};

}  // namespace cache