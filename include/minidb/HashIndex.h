#pragma once
#include "IIndex.h"
#include <vector>

namespace minidb {
class HashIndex : public IIndex {
public:
    explicit HashIndex(std::size_t initial_buckets = 16,
                       double max_load_factor = 0.75);
    InsertStatus insert(TradeId key, RowId row) override;
    std::optional<RowId> find(TradeId key) const override;
    std::size_t size() const override;
    bool validateStructure() const override;
    std::size_t bucketCount() const override;

    double loadFactor() const;

private:
    static std::uint64_t hashKey(TradeId key);
    std::size_t bucketFor(TradeId key) const;
    void rehash(std::size_t new_bucket_count);

    std::vector<std::vector<IndexEntry>> buckets_;
    std::size_t size_ = 0;
    double max_load_factor_;
};
} // namespace minidb
