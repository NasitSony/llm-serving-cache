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
    const std::string& prefix_hash) const {

  const std::string key = MakeCacheKey(model_id, prefix_hash);

  auto it = cache_entries_.find(key);
  if (it == cache_entries_.end()) {
    return std::nullopt;
  }

  return it->second;
}

std::optional<CacheEntry> MetadataStore::FindLongestPrefixMatch(
    const std::string& model_id,
    const std::string& request_prefix) const {

  std::optional<CacheEntry> best_match;
  std::size_t best_length = 0;

  for (const auto& [_, entry] : cache_entries_) {
    if (entry.model_id != model_id) {
      continue;
    }

    if (request_prefix.rfind(entry.prefix_hash, 0) == 0) {
      if (entry.prefix_hash.size() > best_length) {
        best_match = entry;
        best_length = entry.prefix_hash.size();
      }
    }
  }

  return best_match;
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

}  // namespace cache