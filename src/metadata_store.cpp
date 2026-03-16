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