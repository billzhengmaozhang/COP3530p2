#include <catch2/catch_test_macros.hpp>
#include "minidb/DatasetGenerator.h"
#include "minidb/Error.h"
#include "minidb/RecordValidator.h"
#include <algorithm>
#include <limits>
#include <set>

using namespace minidb;

TEST_CASE("DatasetGenerator is deterministic for a seed", "[generator]") {
    DatasetConfig cfg{500, 3530, InsertionOrder::Shuffled, 1, 1};
    const auto a = DatasetGenerator::generate(cfg);
    const auto b = DatasetGenerator::generate(cfg);
    REQUIRE(a.size() == 500);
    for (std::size_t i = 0; i < a.size(); ++i) {
        REQUIRE(a[i].trade_id == b[i].trade_id);
        REQUIRE(a[i].symbol == b[i].symbol);
        REQUIRE(a[i].price_cents == b[i].price_cents);
    }
    cfg.seed = 1;
    const auto c = DatasetGenerator::generate(cfg);
    bool differs = false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        differs = differs || a[i].trade_id != c[i].trade_id;
    }
    REQUIRE(differs);
}

TEST_CASE("DatasetGenerator sequential and shuffled hold the same valid unique IDs", "[generator]") {
    auto seq = DatasetGenerator::generate({1000, 7, InsertionOrder::Sequential, 10, 1});
    auto shuf = DatasetGenerator::generate({1000, 7, InsertionOrder::Shuffled, 10, 1});
    REQUIRE(std::is_sorted(seq.begin(), seq.end(), [](auto& x, auto& y) { return x.trade_id < y.trade_id; }));
    REQUIRE(seq.front().trade_id == 10);
    REQUIRE(seq.back().trade_id == 1009);
    REQUIRE_FALSE(std::is_sorted(shuf.begin(), shuf.end(), [](auto& x, auto& y) { return x.trade_id < y.trade_id; }));
    std::set<TradeId> ids;
    for (const auto& r : shuf) {
        REQUIRE_NOTHROW(RecordValidator::validate(r));
        ids.insert(r.trade_id);
    }
    REQUIRE(ids.size() == 1000);
    REQUIRE(*ids.begin() == 10);
}

TEST_CASE("DatasetGenerator id_stride leaves gaps", "[generator]") {
    const auto recs = DatasetGenerator::generate({5, 1, InsertionOrder::Sequential, 1, 2});
    REQUIRE(recs[0].trade_id == 1);
    REQUIRE(recs[4].trade_id == 9);
}

TEST_CASE("DatasetGenerator validates its configuration", "[generator]") {
    REQUIRE_THROWS_AS(DatasetGenerator::generate({10, 1, InsertionOrder::Sequential, 0, 1}), MiniDBError);
    REQUIRE_THROWS_AS(DatasetGenerator::generate({10, 1, InsertionOrder::Sequential, 1, 0}), MiniDBError);
    const auto max = std::numeric_limits<TradeId>::max();
    REQUIRE_THROWS_AS(DatasetGenerator::generate({10, 1, InsertionOrder::Sequential, max - 3, 1}), MiniDBError);
    REQUIRE_THROWS_AS(DatasetGenerator::generate({10, 1, InsertionOrder::Sequential, 1, max / 4}), MiniDBError);
    REQUIRE(DatasetGenerator::generate({0, 1, InsertionOrder::Sequential, 1, 1}).empty());
}
