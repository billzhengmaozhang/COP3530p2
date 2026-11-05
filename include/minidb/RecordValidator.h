#pragma once
#include "Types.h"
#include <array>
#include <string>
#include <string_view>

namespace minidb {
class RecordValidator {
public:
    // Returns normally if valid; otherwise throws MiniDBError.
    static void validate(const TradeRecord& record);
    // Input fields follow the CSV column order; output is a validated record.
    static TradeRecord parse(const std::array<std::string, 5>& fields);
    // Used by lookup prompts; rejects zero, overflow, negatives and trailing text.
    static TradeId parseTradeId(std::string_view text);

private:
    static bool isValidSymbol(std::string_view symbol);
    static std::string trim(std::string_view text);
};
} // namespace minidb
