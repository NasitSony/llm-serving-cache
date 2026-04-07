#include "cache/placement_policy.h"

namespace cache {

// placeholder implementation for now
int estimate_kv_cache_mb(int tokens) {
    return tokens / 10;
}

}  // namespace cache