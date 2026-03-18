#include "cache/metadata_store.h"

namespace cache {

namespace {

std::string MakeCacheKey(const std::string& model_id,
                         const std::string& prefix_hash) {
  return model_id + ":" + prefix_hash;
}

}  // namespace

bool MetadataStore::RegisterCacheEntry(const CacheEntry& entry) {
  const std::string key = MakeCacheKey(entry.model_id, entry.prefix_hash);
  cache_entries_[key] = entry;
  return true;
}

std::optional<CacheEntry> MetadataStore::FindCacheEntry(
    const std::string& model_id,
    const std::string& prefix_hash) {

  const std::string key = MakeCacheKey(model_id, prefix_hash);

  auto it = cache_entries_.find(key);
  if (it == cache_entries_.end()) {
    return std::nullopt;
  }

  // Update access time here
  it->second.last_access_ms++;

  return it->second;
}

std::optional<CacheEntry> MetadataStore::FindLongestPrefixMatch(
    const std::string& model_id,
    const std::string& request_prefix) {

    CacheEntry* best_match = nullptr;
    std::size_t best_length = 0;

    for (auto& [_, entry] : cache_entries_) {
        if (entry.model_id != model_id) {
            continue;
        }

        if (request_prefix.rfind(entry.prefix_hash, 0) == 0) {
            if (entry.prefix_hash.size() > best_length) {
                best_match = &entry;
                best_length = entry.prefix_hash.size();
            }
        }
    }

    if (best_match == nullptr) {
        return std::nullopt;
    }

    // Update access time here
    best_match->last_access_ms++;

    return *best_match;
}

bool MetadataStore::AssignSession(const SessionRoute& route) {
  session_routes_[route.session_id] = route;
  return true;
}

std::optional<SessionRoute> MetadataStore::GetSessionRoute(
    const std::string& session_id) const {

  auto it = session_routes_.find(session_id);
  if (it == session_routes_.end()) {
    return std::nullopt;
  }

  return it->second;
}

std::optional<CacheEntry> MetadataStore::EvictOne(
    const std::string& node_id
) {
    std::string victim_key;
    std::uint64_t oldest = UINT64_MAX;
    std::optional<CacheEntry> victim_entry;

    for (const auto& [key, entry] : cache_entries_) {
        if (entry.node_id != node_id) {
            continue;
        }

        if (entry.last_access_ms < oldest) {
            oldest = entry.last_access_ms;
            victim_key = key;
            victim_entry = entry;
        }
    }

    if (!victim_entry.has_value()) {
        return std::nullopt;
    }

    cache_entries_.erase(victim_key);
    return victim_entry;
}

}  // namespace cache