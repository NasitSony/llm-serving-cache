#pragma once

namespace cache {

class PlacementPolicy {
 public:
  PlacementPolicy() = default;
  int estimate_kv_cache_mb(int tokens);
};

}  // namespace cache