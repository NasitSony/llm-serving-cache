#pragma once

#include <optional>
#include <string>

#include "cache/cache_types.h"
#include "kv/kv_store.h"

namespace cache {

class KVMetadataStore {
public:
    explicit KVMetadataStore(kv::KVStore& kv);

    bool RegisterCacheEntry(const CacheEntry& entry);

    std::optional<CacheEntry> FindCacheEntry(
        const std::string& model_id,
        const std::string& prefix_hash
    );

    std::optional<CacheEntry> FindLongestPrefixMatch(
        const std::string& model_id,
        const std::string& request_prefix
    );

    bool AssignSession(const SessionRoute& route);

    std::optional<SessionRoute> GetSessionRoute(
        const std::string& session_id
    );

    std::optional<CacheEntry> EvictOne(const std::string& node_id);

private:
    kv::KVStore& kv_;

    std::string MakeCacheKey(
        const std::string& model_id,
        const std::string& prefix_hash
    ) const;

    std::string MakeSessionKey(
        const std::string& session_id
    ) const;

    std::string SerializeCacheEntry(const CacheEntry& entry) const;
    std::optional<CacheEntry> DeserializeCacheEntry(const std::string& raw) const;

    std::string SerializeSessionRoute(const SessionRoute& route) const;
    std::optional<SessionRoute> DeserializeSessionRoute(const std::string& raw) const;
};

} // namespace cache