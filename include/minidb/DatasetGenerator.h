#pragma once
#include "Types.h"
#include <string>
#include <vector>

namespace minidb {
struct DatasetConfig {
    std::size_t count = 100000;
    std::uint64_t seed = 3530;
    InsertionOrder order = InsertionOrder::Sequential;
    TradeId first_id = 1; // Use a disjoint range for additional insertion records.
};

class DatasetGenerator {
public:
    static std::vector<TradeRecord> generate(const DatasetConfig& config);
    static std::size_t writeCsv(const std::string& path,
                                const DatasetConfig& config);
};
} // namespace minidb
