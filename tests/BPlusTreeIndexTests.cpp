//
// Created by Jacob on 11/8/2026.
//


#include <catch2/catch_test_macros.hpp>
#include "minidb/BPlusTreeIndex.h"
#include "minidb/Error.h"
#include <algorithm>
#include <numeric>
#include <random>
#include <vector>


using namespace minidb;

TEST_CASE("BPlusTreeIndex rejects invalid max_children configuration", "[bplustree]") {
    REQUIRE_THROWS_AS(BPlusTreeIndex(0), MiniDBError);
    REQUIRE_THROWS_AS(BPlusTreeIndex(1), MiniDBError);
    REQUIRE_THROWS_AS(BPlusTreeIndex(2), MiniDBError);
    REQUIRE_NOTHROW(BPlusTreeIndex(3));
}

TEST_CASE("BPlusTreeIndex handles empty lookup, RowId{0}, and duplicate rejection", "[bplustree]") {
    BPlusTreeIndex tree(4);
    REQUIRE(tree.size() == 0);
    REQUIRE(tree.height() == 1);
    REQUIRE(tree.find(1001) == std::nullopt);
    REQUIRE(tree.validateStructure());

    // RowId{0} is a valid match and must be distinguished from std::nullopt
    REQUIRE(tree.insert(1001, 0) == InsertStatus::Inserted);
    REQUIRE(tree.size() == 1);
    REQUIRE(tree.find(1001) == std::optional<RowId>{0});

    // Duplicate insertion returns DuplicateKey and leaves original mapping & size unchanged
    REQUIRE(tree.insert(1001, 99) == InsertStatus::DuplicateKey);
    REQUIRE(tree.size() == 1);
    REQUIRE(tree.find(1001) == std::optional<RowId>{0});
    REQUIRE(tree.validateStructure());
}

TEST_CASE("BPlusTreeIndex M=4 leaf split and separator equality lookup", "[bplustree]") {
    // Design Doc Section 6 example:
    // With M = 4, inserting keys 10, 20, 30, 40 splits leaf into [10,20] and [30,40] with separator 30.
    BPlusTreeIndex tree(4);

    REQUIRE(tree.insert(10, 0) == InsertStatus::Inserted);
    REQUIRE(tree.insert(20, 1) == InsertStatus::Inserted);
    REQUIRE(tree.insert(30, 2) == InsertStatus::Inserted);
    REQUIRE(tree.height() == 1);
    REQUIRE(tree.validateStructure());

    // 4th key reaches M=4 entries -> triggers leaf split and root growth
    REQUIRE(tree.insert(40, 3) == InsertStatus::Inserted);
    REQUIRE(tree.height() == 2);
    REQUIRE(tree.size() == 4);
    REQUIRE(tree.validateStructure());

    // Exact separator lookup (key == 30) must follow right child and find RowId 2
    REQUIRE(tree.find(10) == std::optional<RowId>{0});
    REQUIRE(tree.find(20) == std::optional<RowId>{1});
    REQUIRE(tree.find(30) == std::optional<RowId>{2});
    REQUIRE(tree.find(40) == std::optional<RowId>{3});
    REQUIRE(tree.find(25) == std::nullopt);
}

TEST_CASE("BPlusTreeIndex cascading internal splits and root growth (M=3 and M=4)", "[bplustree]") {
    for (std::size_t M : {std::size_t{3}, std::size_t{4}, std::size_t{5}}) {
        BPlusTreeIndex tree(M);
        constexpr std::size_t N = 200;

        for (std::size_t i = 0; i < N; ++i) {
            TradeId key = static_cast<TradeId>((i + 1) * 10);
            REQUIRE(tree.insert(key, i) == InsertStatus::Inserted);
            // Validate every structural invariant after each split-inducing insert
            REQUIRE(tree.validateStructure());
        }

        REQUIRE(tree.size() == N);
        REQUIRE(tree.height() >= 3); // Guaranteed multiple internal + root splits

        for (std::size_t i = 0; i < N; ++i) {
            TradeId key = static_cast<TradeId>((i + 1) * 10);
            REQUIRE(tree.find(key) == std::optional<RowId>{i});
            REQUIRE(tree.find(key + 5) == std::nullopt);
        }
    }
}

TEST_CASE("BPlusTreeIndex sequential and shuffled insertion orders preserve mappings", "[bplustree]") {
    constexpr std::size_t N = 5000;
    std::vector<IndexEntry> entries(N);
    for (std::size_t i = 0; i < N; ++i) {
        entries[i] = IndexEntry{static_cast<TradeId>(i + 1), i};
    }

    SECTION("Sequential insertion") {
        BPlusTreeIndex tree(8);
        for (const auto& e : entries) {
            REQUIRE(tree.insert(e.key, e.row) == InsertStatus::Inserted);
        }
        REQUIRE(tree.size() == N);
        REQUIRE(tree.validateStructure());

        for (const auto& e : entries) {
            REQUIRE(tree.find(e.key) == std::optional<RowId>{e.row});
        }
    }

    SECTION("Shuffled insertion with seed 3530") {
        BPlusTreeIndex tree(8);
        std::mt19937_64 rng(3530);
        std::shuffle(entries.begin(), entries.end(), rng);

        for (const auto& e : entries) {
            REQUIRE(tree.insert(e.key, e.row) == InsertStatus::Inserted);
        }
        REQUIRE(tree.size() == N);
        REQUIRE(tree.validateStructure());

        // Re-verify all mappings and duplicate rejection
        for (const auto& e : entries) {
            REQUIRE(tree.find(e.key) == std::optional<RowId>{e.row});
            REQUIRE(tree.insert(e.key, e.row + 1000) == InsertStatus::DuplicateKey);
        }
    }
}