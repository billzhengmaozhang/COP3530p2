#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

namespace minidb {
using TradeId = std::uint64_t;
using RowId = std::size_t;

struct TradeRecord {
    TradeId trade_id;
    std::string symbol;
    std::int64_t price_cents;
    std::uint32_t quantity;
    std::int64_t timestamp;
};

struct IndexEntry {
    TradeId key;
    RowId row;
};

enum class InsertStatus { Inserted, DuplicateKey };
enum class IndexKind { Hash, BPlusTree };
enum class InsertionOrder { Sequential, Shuffled };

struct IndexConfig {
    std::size_t initial_bucket_count = 16;
    double max_load_factor = 0.75;
    std::size_t max_children = 32;
};
} // namespace minidb
