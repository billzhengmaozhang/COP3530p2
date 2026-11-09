#include "minidb/DatasetGenerator.h"
#include "minidb/CsvIO.h"
#include "minidb/Error.h"
#include <algorithm>
#include <array>
#include <limits>
#include <random>

namespace minidb {
std::vector<TradeRecord> DatasetGenerator::generate(const DatasetConfig& config) {
    if (config.first_id == 0 || config.id_stride == 0 || (config.count != 0 &&
        config.count - 1 > (std::numeric_limits<TradeId>::max() - config.first_id) / config.id_stride)) {
        throw MiniDBError(ErrorCode::InvalidConfiguration, "Dataset trade IDs are out of range.");
    }
    if (config.order != InsertionOrder::Sequential && config.order != InsertionOrder::Shuffled) {
        throw MiniDBError(ErrorCode::InvalidConfiguration, "Unknown dataset insertion order.");
    }

    constexpr std::array<const char*, 8> symbols{
        "AAPL", "AMZN", "BRK.B", "GOOG", "META", "MSFT", "NVDA", "TSLA"
    };
    std::mt19937_64 rng(config.seed);
    std::uniform_int_distribution<std::size_t> symbol(0, symbols.size() - 1);
    std::uniform_int_distribution<std::int64_t> price(100, 100000);
    std::uniform_int_distribution<std::uint32_t> quantity(1, 10000);
    // Choose a Unix timestamp within the first day of 2026 UTC.
    std::uniform_int_distribution<std::int64_t> timestamp(1767225600, 1767311999);

    std::vector<TradeRecord> records;
    records.reserve(config.count);
    for (std::size_t i = 0; i < config.count; ++i) {
        records.push_back({config.first_id + i * config.id_stride, symbols[symbol(rng)],
                           price(rng), quantity(rng), timestamp(rng)});
    }
    if (config.order == InsertionOrder::Shuffled) {
        std::shuffle(records.begin(), records.end(), rng);
    }
    return records;
}

std::size_t DatasetGenerator::writeCsv(const std::string& path, const DatasetConfig& config) {
    return CsvIO::write(path, generate(config));
}
} // namespace minidb
