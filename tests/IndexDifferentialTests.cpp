#include <catch2/catch_test_macros.hpp>
#include "minidb/BPlusTreeIndex.h"
#include "minidb/HashIndex.h"
#include <map>
#include <memory>
#include <random>

using namespace minidb;

// Both indexes must behave exactly like a reference std::map (tests only).
TEST_CASE("Indexes agree with a reference map under random operations", "[differential]") {
    for (std::size_t max_children : {std::size_t{3}, std::size_t{4}, std::size_t{7}, std::size_t{32}}) {
        BPlusTreeIndex tree(max_children);
        HashIndex hash(2, 0.75);
        std::map<TradeId, RowId> reference;
        std::mt19937_64 rng(3530 + max_children);
        std::uniform_int_distribution<TradeId> key_dist(1, 2000); // small range forces duplicates

        for (std::size_t step = 0; step < 6000; ++step) {
            const TradeId key = key_dist(rng);
            if (rng() % 3 != 0) {
                const bool is_new = reference.emplace(key, step).second;
                const auto expected = is_new ? InsertStatus::Inserted : InsertStatus::DuplicateKey;
                REQUIRE(tree.insert(key, step) == expected);
                REQUIRE(hash.insert(key, step) == expected);
            } else {
                const auto it = reference.find(key);
                const std::optional<RowId> expected =
                    it == reference.end() ? std::nullopt : std::optional<RowId>{it->second};
                REQUIRE(tree.find(key) == expected);
                REQUIRE(hash.find(key) == expected);
            }
            REQUIRE(tree.size() == reference.size());
            REQUIRE(hash.size() == reference.size());
        }
        REQUIRE(tree.validateStructure());
        REQUIRE(hash.validateStructure());
        for (TradeId key = 1; key <= 2100; ++key) {
            const auto it = reference.find(key);
            const std::optional<RowId> expected =
                it == reference.end() ? std::nullopt : std::optional<RowId>{it->second};
            REQUIRE(tree.find(key) == expected);
            REQUIRE(hash.find(key) == expected);
        }
    }
}

TEST_CASE("IIndex exposes validation and capacity through the interface", "[differential]") {
    std::unique_ptr<IIndex> hash = std::make_unique<HashIndex>(8, 0.75);
    std::unique_ptr<IIndex> tree = std::make_unique<BPlusTreeIndex>(16);
    REQUIRE(hash->validateStructure());
    REQUIRE(tree->validateStructure());
    REQUIRE(hash->bucketCount() == 8);
    REQUIRE(hash->maxChildren() == 0);
    REQUIRE(tree->maxChildren() == 16);
    REQUIRE(tree->bucketCount() == 0);
}
