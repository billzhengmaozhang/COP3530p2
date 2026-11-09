#include <catch2/catch_test_macros.hpp>
#include "minidb/Error.h"
#include "minidb/RecordValidator.h"

using namespace minidb;

namespace {
TradeRecord valid() { return {1, "AAPL", 18925, 100, 1793797200}; }
}

TEST_CASE("RecordValidator accepts a valid record", "[validator]") {
    REQUIRE_NOTHROW(RecordValidator::validate(valid()));
}

TEST_CASE("RecordValidator rejects each invalid field", "[validator]") {
    auto r = valid();
    r.trade_id = 0;
    REQUIRE_THROWS_AS(RecordValidator::validate(r), MiniDBError);
    r = valid(); r.price_cents = 0;
    REQUIRE_THROWS_AS(RecordValidator::validate(r), MiniDBError);
    r = valid(); r.price_cents = -5;
    REQUIRE_THROWS_AS(RecordValidator::validate(r), MiniDBError);
    r = valid(); r.quantity = 0;
    REQUIRE_THROWS_AS(RecordValidator::validate(r), MiniDBError);
    r = valid(); r.timestamp = -1;
    REQUIRE_THROWS_AS(RecordValidator::validate(r), MiniDBError);
    r = valid(); r.timestamp = 0;
    REQUIRE_NOTHROW(RecordValidator::validate(r));
}

TEST_CASE("RecordValidator symbol rules", "[validator]") {
    for (const char* ok : {"A", "BRK.B", "A-1", "ABCDEFGHIJ"}) {
        auto r = valid(); r.symbol = ok;
        REQUIRE_NOTHROW(RecordValidator::validate(r));
    }
    for (const char* bad : {"", "aapl", "1ABC", ".A", "AB CD", "ABCDEFGHIJK", "AB,C"}) {
        auto r = valid(); r.symbol = bad;
        REQUIRE_THROWS_AS(RecordValidator::validate(r), MiniDBError);
    }
}

TEST_CASE("RecordValidator::parse trims and converts fields", "[validator]") {
    const auto r = RecordValidator::parse({" 42 ", " MSFT ", "100", "5", "0"});
    REQUIRE(r.trade_id == 42);
    REQUIRE(r.symbol == "MSFT");
    REQUIRE(r.price_cents == 100);
    REQUIRE(r.quantity == 5);
    REQUIRE(r.timestamp == 0);
}

TEST_CASE("RecordValidator::parse rejects malformed numbers", "[validator]") {
    REQUIRE_THROWS_AS(RecordValidator::parse({"1", "AAPL", "12.5", "1", "1"}), MiniDBError);
    REQUIRE_THROWS_AS(RecordValidator::parse({"1", "AAPL", "10", "-1", "1"}), MiniDBError);
    REQUIRE_THROWS_AS(RecordValidator::parse({"1", "AAPL", "10", "4294967296", "1"}), MiniDBError);
    REQUIRE_THROWS_AS(RecordValidator::parse({"1x", "AAPL", "10", "1", "1"}), MiniDBError);
    REQUIRE_THROWS_AS(RecordValidator::parse({"", "AAPL", "10", "1", "1"}), MiniDBError);
    REQUIRE_THROWS_AS(RecordValidator::parse({"1", "AAPL", "99999999999999999999", "1", "1"}), MiniDBError);
}

TEST_CASE("RecordValidator::parseTradeId", "[validator]") {
    REQUIRE(RecordValidator::parseTradeId(" 18446744073709551615 ") == 18446744073709551615ULL);
    REQUIRE_THROWS_AS(RecordValidator::parseTradeId("0"), MiniDBError);
    REQUIRE_THROWS_AS(RecordValidator::parseTradeId("-1"), MiniDBError);
    REQUIRE_THROWS_AS(RecordValidator::parseTradeId("18446744073709551616"), MiniDBError);
    REQUIRE_THROWS_AS(RecordValidator::parseTradeId("12 34"), MiniDBError);
    REQUIRE_THROWS_AS(RecordValidator::parseTradeId(""), MiniDBError);
}
