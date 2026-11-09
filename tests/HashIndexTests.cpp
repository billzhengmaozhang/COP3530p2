#include <catch2/catch_test_macros.hpp>
#include "minidb/Error.h"
#include "minidb/HashIndex.h"

using namespace minidb;

TEST_CASE("HashIndex rejects invalid configuration", "[hash]") {
    REQUIRE_THROWS_AS(HashIndex(0, 0.75), MiniDBError);
    REQUIRE_THROWS_AS(HashIndex(16, 0.0), MiniDBError);
    REQUIRE_THROWS_AS(HashIndex(16, -1.0), MiniDBError);
    REQUIRE_THROWS_AS(HashIndex(16, std::numeric_limits<double>::infinity()), MiniDBError);
    REQUIRE_THROWS_AS(HashIndex(16, std::numeric_limits<double>::quiet_NaN()), MiniDBError);
    REQUIRE_NOTHROW(HashIndex(1, 0.5));
}

TEST_CASE("HashIndex empty lookup, row zero and duplicates", "[hash]") {
    HashIndex index;
    REQUIRE(index.size() == 0);
    REQUIRE(index.find(1) == std::nullopt);
    REQUIRE(index.validateStructure());

    REQUIRE(index.insert(1, 0) == InsertStatus::Inserted);
    REQUIRE(index.find(1) == std::optional<RowId>{0});
    REQUIRE(index.insert(1, 7) == InsertStatus::DuplicateKey);
    REQUIRE(index.find(1) == std::optional<RowId>{0});
    REQUIRE(index.size() == 1);
    REQUIRE(index.validateStructure());
}

TEST_CASE("HashIndex grows before exceeding the load factor", "[hash]") {
    HashIndex index(4, 0.75);
    REQUIRE(index.bucketCount() == 4);
    for (TradeId id = 1; id <= 3; ++id) {
        REQUIRE(index.insert(id, id - 1) == InsertStatus::Inserted);
    }
    REQUIRE(index.bucketCount() == 4);  // 3 / 4 == 0.75 is allowed
    REQUIRE(index.insert(4, 3) == InsertStatus::Inserted);
    REQUIRE(index.bucketCount() == 8);  // 4 / 4 would exceed 0.75

    for (TradeId id = 5; id <= 5000; ++id) {
        REQUIRE(index.insert(id, id - 1) == InsertStatus::Inserted);
        REQUIRE(index.loadFactor() <= 0.75);
    }
    REQUIRE(index.validateStructure());
    for (TradeId id = 1; id <= 5000; ++id) {
        REQUIRE(index.find(id) == std::optional<RowId>{id - 1});
    }
    REQUIRE(index.find(5001) == std::nullopt);
}

TEST_CASE("HashIndex with one bucket and a huge load factor chains every key", "[hash]") {
    HashIndex index(1, 1e9);  // never rehashes: all keys collide in one bucket
    for (TradeId id = 1; id <= 200; ++id) {
        REQUIRE(index.insert(id * 7919, id) == InsertStatus::Inserted);
    }
    REQUIRE(index.bucketCount() == 1);
    REQUIRE(index.validateStructure());
    for (TradeId id = 1; id <= 200; ++id) {
        REQUIRE(index.find(id * 7919) == std::optional<RowId>{id});
        REQUIRE(index.insert(id * 7919, 0) == InsertStatus::DuplicateKey);
    }
    REQUIRE(index.find(7919 + 1) == std::nullopt);
}

TEST_CASE("HashIndex handles extreme keys", "[hash]") {
    HashIndex index;
    const TradeId max = std::numeric_limits<TradeId>::max();
    REQUIRE(index.insert(max, 1) == InsertStatus::Inserted);
    REQUIRE(index.insert(1, 2) == InsertStatus::Inserted);
    REQUIRE(index.find(max) == std::optional<RowId>{1});
    REQUIRE(index.find(max - 1) == std::nullopt);
}
