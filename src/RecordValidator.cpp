#include "minidb/RecordValidator.h"
#include "minidb/Error.h"
#include <charconv>

namespace minidb {
namespace {
template<class Integer>
Integer parseInteger(std::string_view text, const char* field) {
    Integer value{};
    const char* end = text.data() + text.size();
    const auto result = std::from_chars(text.data(), end, value);
    if (result.ec != std::errc{} || result.ptr != end) {
        throw MiniDBError(ErrorCode::InvalidInput,
                          std::string(field) + " must be an integer in range.");
    }
    return value;
}
} // namespace

void RecordValidator::validate(const TradeRecord& record) {
    if (record.trade_id == 0) {
        throw MiniDBError(ErrorCode::InvalidRecord, "trade_id must be greater than zero.");
    }
    if (!isValidSymbol(record.symbol)) {
        throw MiniDBError(ErrorCode::InvalidRecord,
                          "symbol must start with A-Z and contain 1-10 characters from A-Z, 0-9, . or -.");
    }
    if (record.price_cents <= 0) {
        throw MiniDBError(ErrorCode::InvalidRecord, "price_cents must be greater than zero.");
    }
    if (record.quantity == 0) {
        throw MiniDBError(ErrorCode::InvalidRecord, "quantity must be greater than zero.");
    }
    if (record.timestamp < 0) {
        throw MiniDBError(ErrorCode::InvalidRecord, "timestamp must be nonnegative.");
    }
}

TradeRecord RecordValidator::parse(const std::array<std::string, 5>& fields) {
    TradeRecord record{
        parseTradeId(fields[0]),
        trim(fields[1]),
        parseInteger<std::int64_t>(trim(fields[2]), "price_cents"),
        parseInteger<std::uint32_t>(trim(fields[3]), "quantity"),
        parseInteger<std::int64_t>(trim(fields[4]), "timestamp")
    };
    validate(record);
    return record;
}

TradeId RecordValidator::parseTradeId(std::string_view text) {
    const auto id = parseInteger<TradeId>(trim(text), "trade_id");
    if (id == 0) {
        throw MiniDBError(ErrorCode::InvalidInput, "trade_id must be greater than zero.");
    }
    return id;
}

bool RecordValidator::isValidSymbol(std::string_view symbol) {
    if (symbol.empty() || symbol.size() > 10 || symbol.front() < 'A' || symbol.front() > 'Z') {
        return false;
    }
    for (char ch : symbol.substr(1)) {
        if (!((ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '.' || ch == '-')) {
            return false;
        }
    }
    return true;
}

std::string RecordValidator::trim(std::string_view text) {
    constexpr std::string_view whitespace = " \t\r\n\v\f";
    const auto start = text.find_first_not_of(whitespace);
    if (start == std::string_view::npos) {
        return {};
    }
    return std::string(text.substr(start, text.find_last_not_of(whitespace) - start + 1));
}
} // namespace minidb
