#pragma once

#include <optional>
#include <string>
#include <unordered_map>

#include "cache/cache_types.h"

namespace cache {

class MetadataStore {
 public:
  bool RegisterCacheEntry(const CacheEntry& entry);

  std::optional<CacheEntry> FindCacheEntry(
      const std::string& model_id,
      const std::string& prefix_hash) const;

  std::optional<CacheEntry> FindLongestPrefixMatch(
      const std::string& model_id,
      const std::string& request_prefix) const;

  bool AssignSession(const SessionRoute& route);

  std::optional<SessionRoute> GetSessionRoute(
      const std::string& session_id) const;

 private:
  std::unordered_map<std::string, CacheEntry> cache_entries_;
  std::unordered_map<std::string, SessionRoute> session_routes_;
};

}  // namespace cache