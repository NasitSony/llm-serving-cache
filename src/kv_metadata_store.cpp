#include "cache/kv_metadata_store.h"

#include <sstream>
#include <vector>

namespace cache {
namespace {

std::vector<std::string> Split(const std::string& s, char delim) {
    std::vector<std::string> parts;
    std::stringstream ss(s);
    std::string item;

    while (std::getline(ss, item, delim)) {
        parts.push_back(item);
    }

    return parts;
}

} // namespace

KVMetadataStore::KVMetadataStore(kv::KVStore& kv)
    : kv_(kv) {}

std::string KVMetadataStore::MakeCacheKey(
    const std::string& model_id,
    const std::string& prefix_hash
) const {
    return "cache:" + model_id + ":" + prefix_hash;
}

std::string KVMetadataStore::MakeSessionKey(
    const std::string& session_id
) const {
    return "session:" + session_id;
}

std::string KVMetadataStore::SerializeCacheEntry(
    const CacheEntry& entry
) const {
    return entry.model_id + "|" +
           entry.prefix_hash + "|" +
           entry.block_id + "|" +
           entry.node_id + "|" +
           std::to_string(entry.created_at_ms) + "|" +
           std::to_string(entry.last_access_ms);
}

std::optional<CacheEntry> KVMetadataStore::DeserializeCacheEntry(
    const std::string& raw
) const {
    auto parts = Split(raw, '|');
    if (parts.size() != 6) {
        return std::nullopt;
    }

    CacheEntry entry;
    entry.model_id = parts[0];
    entry.prefix_hash = parts[1];
    entry.block_id = parts[2];
    entry.node_id = parts[3];
    entry.created_at_ms = std::stoull(parts[4]);
    entry.last_access_ms = std::stoull(parts[5]);

    return entry;
}

std::string KVMetadataStore::SerializeSessionRoute(
    const SessionRoute& route
) const {
    return route.session_id + "|" +
           route.model_id + "|" +
           route.node_id + "|" +
           route.status;
}

std::optional<SessionRoute> KVMetadataStore::DeserializeSessionRoute(
    const std::string& raw
) const {
    auto parts = Split(raw, '|');
    if (parts.size() != 4) {
        return std::nullopt;
    }

    SessionRoute route;
    route.session_id = parts[0];
    route.model_id = parts[1];
    route.node_id = parts[2];
    route.status = parts[3];

    return route;
}

bool KVMetadataStore::RegisterCacheEntry(const CacheEntry& entry) {
    kv_.put(MakeCacheKey(entry.model_id, entry.prefix_hash),
            SerializeCacheEntry(entry));
    return true;
}

std::optional<CacheEntry> KVMetadataStore::FindCacheEntry(
    const std::string& model_id,
    const std::string& prefix_hash
) {
    auto raw = kv_.get(MakeCacheKey(model_id, prefix_hash));
    if (!raw.has_value()) {
        return std::nullopt;
    }

    return DeserializeCacheEntry(*raw);
}

std::optional<CacheEntry> KVMetadataStore::FindLongestPrefixMatch(
    const std::string& model_id,
    const std::string& request_prefix
) {
    auto keys = kv_.list_keys_with_prefix("cache:" + model_id + ":");

    std::optional<CacheEntry> best_match;
    std::size_t best_length = 0;

    for (const auto& key : keys) {
        auto raw = kv_.get(key);
        if (!raw.has_value()) {
            continue;
        }

        auto entry = DeserializeCacheEntry(*raw);
        if (!entry.has_value()) {
            continue;
        }

        if (request_prefix.rfind(entry->prefix_hash, 0) == 0) {
            if (entry->prefix_hash.size() > best_length) {
                best_match = entry;
                best_length = entry->prefix_hash.size();
            }
        }
    }

    return best_match;
}

bool KVMetadataStore::AssignSession(const SessionRoute& route) {
    kv_.put(MakeSessionKey(route.session_id),
            SerializeSessionRoute(route));
    return true;
}

std::optional<SessionRoute> KVMetadataStore::GetSessionRoute(
    const std::string& session_id
) {
    auto raw = kv_.get(MakeSessionKey(session_id));
    if (!raw.has_value()) {
        return std::nullopt;
    }

    return DeserializeSessionRoute(*raw);
}

std::optional<CacheEntry> KVMetadataStore::EvictOne(
    const std::string& node_id
) {
    auto keys = kv_.list_keys_with_prefix("cache:");

    std::optional<CacheEntry> victim;
    std::string victim_key;
    std::uint64_t oldest = UINT64_MAX;

    for (const auto& key : keys) {
        auto raw = kv_.get(key);
        if (!raw.has_value()) {
            continue;
        }

        auto entry = DeserializeCacheEntry(*raw);
        if (!entry.has_value()) {
            continue;
        }

        if (entry->node_id != node_id) {
            continue;
        }

        if (entry->last_access_ms < oldest) {
            oldest = entry->last_access_ms;
            victim = entry;
            victim_key = key;
        }
    }

    if (!victim.has_value()) {
        return std::nullopt;
    }

    kv_.del(victim_key);
    return victim;
}

} // namespace cache