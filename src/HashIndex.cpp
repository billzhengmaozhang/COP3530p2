#include "minidb/HashIndex.h"
#include "minidb/Error.h"
#include <algorithm>
#include <cmath>

namespace minidb {
HashIndex::HashIndex(std::size_t initial_buckets, double max_load_factor)
    : max_load_factor_(max_load_factor) {
    if (initial_buckets == 0 || initial_buckets > buckets_.max_size() ||
        !std::isfinite(max_load_factor) || max_load_factor <= 0.0) {
        throw MiniDBError(ErrorCode::InvalidConfiguration,
                          "Hash index requires a positive bucket count and a finite positive load factor.");
    }
    buckets_.resize(initial_buckets);
}

InsertStatus HashIndex::insert(TradeId key, RowId row) {
    if (find(key)) {
        return InsertStatus::DuplicateKey;
    }
    auto capacity = buckets_.size();
    while (static_cast<double>(size_ + 1) / capacity > max_load_factor_) {
        if (capacity > buckets_.max_size() / 2) {
            throw std::length_error("Hash index capacity exceeded.");
        }
        capacity *= 2;
    }
    if (capacity != buckets_.size()) {
        rehash(capacity);
    }
    buckets_[bucketFor(key)].push_back({key, row});
    ++size_;
    return InsertStatus::Inserted;
}

std::optional<RowId> HashIndex::find(TradeId key) const {
    for (const auto& entry : buckets_[bucketFor(key)]) {
        if (entry.key == key) {
            return entry.row;
        }
    }
    return std::nullopt;
}

std::size_t HashIndex::size() const {
    return size_;
}

std::size_t HashIndex::bucketCount() const {
    return buckets_.size();
}

double HashIndex::loadFactor() const {
    return static_cast<double>(size_) / bucketCount();
}

bool HashIndex::validateStructure() const {
    std::size_t count = 0;
    for (std::size_t i = 0; i < buckets_.size(); ++i) {
        const auto& bucket = buckets_[i];
        for (auto entry = bucket.begin(); entry != bucket.end(); ++entry) {
            if (bucketFor(entry->key) != i ||
                std::any_of(bucket.begin(), entry, [&](const IndexEntry& previous) {
                    return previous.key == entry->key;
                })) {
                return false;
            }
        }
        count += bucket.size();
    }
    return count == size_ && loadFactor() <= max_load_factor_;
}

std::uint64_t HashIndex::hashKey(TradeId key) {
    // SplitMix64 finalizer mixes both high and low bits of sequential IDs.
    key = (key ^ (key >> 30)) * 0xbf58476d1ce4e5b9ULL;
    key = (key ^ (key >> 27)) * 0x94d049bb133111ebULL;
    return key ^ (key >> 31);
}

std::size_t HashIndex::bucketFor(TradeId key) const {
    return hashKey(key) % buckets_.size();
}

void HashIndex::rehash(std::size_t new_bucket_count) {
    std::vector<std::vector<IndexEntry>> new_buckets(new_bucket_count);
    for (const auto& bucket : buckets_) {
        for (const auto& entry : bucket) {
            new_buckets[hashKey(entry.key) % new_bucket_count].push_back(entry);
        }
    }
    buckets_.swap(new_buckets);
}
} // namespace minidb
