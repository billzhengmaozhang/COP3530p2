#include <catch2/catch_test_macros.hpp>
#include "minidb/CsvIO.h"
#include "minidb/Database.h"
#include "minidb/Error.h"
#include <filesystem>
#include <fstream>
#include <random>

using namespace minidb;
namespace fs = std::filesystem;

namespace {
TradeRecord rec(TradeId id, const char* symbol = "AAPL") { return {id, symbol, 100, 1, 1}; }

std::string tempFile(const char* name) {
    static const auto dir = [] {
        auto p = fs::temp_directory_path() / ("minidb-db-test-" + std::to_string(std::random_device{}()));
        fs::create_directories(p);
        return p;
    }();
    return (dir / name).string();
}
}

TEST_CASE("Database insert and find through both indexes", "[database]") {
    Database db;
    REQUIRE(db.size() == 0);
    REQUIRE(db.find(1, IndexKind::Hash) == std::nullopt);
    REQUIRE(db.insert(rec(1, "AAPL")) == InsertStatus::Inserted);
    REQUIRE(db.insert(rec(2, "MSFT")) == InsertStatus::Inserted);
    REQUIRE(db.insert(rec(1, "TSLA")) == InsertStatus::DuplicateKey);
    REQUIRE(db.size() == 2);
    for (auto kind : {IndexKind::Hash, IndexKind::BPlusTree}) {
        REQUIRE(db.find(1, kind)->symbol == "AAPL");  // duplicate did not overwrite
        REQUIRE(db.find(2, kind)->symbol == "MSFT");
        REQUIRE(db.find(3, kind) == std::nullopt);
    }
    REQUIRE(db.validateConsistency());
}

TEST_CASE("Database rejects invalid records without changing state", "[database]") {
    Database db;
    db.insert(rec(1));
    REQUIRE_THROWS_AS(db.insert(rec(0)), MiniDBError);
    REQUIRE_THROWS_AS(db.insert(rec(2, "bad symbol")), MiniDBError);
    REQUIRE(db.size() == 1);
    REQUIRE(db.validateConsistency());
}

TEST_CASE("Database stays consistent across many inserts and splits", "[database]") {
    IndexConfig cfg;
    cfg.initial_bucket_count = 2;
    cfg.max_children = 4;
    Database db(cfg);
    for (TradeId id = 1; id <= 3000; ++id) {
        db.insert(rec((id * 7919) % 100003 + 1)); // some collide: duplicates are fine
    }
    REQUIRE(db.validateConsistency());
}

TEST_CASE("Database::replaceAll is all-or-nothing", "[database]") {
    Database db;
    db.insert(rec(1));
    REQUIRE_THROWS_AS(db.replaceAll({rec(5), rec(6), rec(5)}), MiniDBError);
    REQUIRE(db.size() == 1);
    REQUIRE(db.find(1, IndexKind::Hash).has_value());
    REQUIRE(db.find(5, IndexKind::Hash) == std::nullopt);

    db.replaceAll({rec(5), rec(6)});
    REQUIRE(db.size() == 2);
    REQUIRE(db.find(1, IndexKind::BPlusTree) == std::nullopt);
    REQUIRE(db.find(6, IndexKind::BPlusTree).has_value());
    REQUIRE(db.validateConsistency());
}

TEST_CASE("Database load replaces data, reports duplicate lines, and keeps old data on failure", "[database]") {
    const auto good = tempFile("good.csv");
    const auto dup = tempFile("dup.csv");
    const auto bad = tempFile("bad.csv");
    CsvIO::write(good, {rec(10), rec(11)});
    {
        std::ofstream out(dup);
        out << "trade_id,symbol,price_cents,quantity,timestamp\n1,AAPL,1,1,1\n2,AAPL,1,1,1\n1,AAPL,1,1,1\n";
    }
    {
        std::ofstream out(bad);
        out << "trade_id,symbol,price_cents,quantity,timestamp\n1,AAPL,1,1,1\nnope\n";
    }

    Database db;
    db.insert(rec(1));
    REQUIRE(db.load(good) == 2);
    REQUIRE(db.find(1, IndexKind::Hash) == std::nullopt);
    REQUIRE(db.find(10, IndexKind::BPlusTree).has_value());

    try {
        db.load(dup);
        FAIL("expected duplicate error");
    } catch (const MiniDBError& e) {
        REQUIRE(e.code() == ErrorCode::DuplicateId);
        REQUIRE(e.line() == std::optional<std::size_t>{4});
    }
    REQUIRE_THROWS_AS(db.load(bad), MiniDBError);
    REQUIRE_THROWS_AS(db.load(tempFile("missing.csv")), MiniDBError);
    REQUIRE(db.size() == 2);  // still the "good" dataset
    REQUIRE(db.validateConsistency());
}

TEST_CASE("Database save then load round trips", "[database]") {
    Database db;
    for (TradeId id = 1; id <= 50; ++id) db.insert(rec(id * 3));
    const auto path = tempFile("roundtrip.csv");
    REQUIRE(db.save(path) == 50);
    Database other;
    REQUIRE(other.load(path) == 50);
    for (TradeId id = 1; id <= 50; ++id) {
        const auto loaded = other.find(id * 3, IndexKind::Hash);
        const auto original = db.find(id * 3, IndexKind::BPlusTree);
        REQUIRE(loaded.has_value());
        REQUIRE(original.has_value());
        REQUIRE(loaded->symbol == original->symbol);
        REQUIRE(loaded->price_cents == original->price_cents);
    }
}

TEST_CASE("Database rejects invalid index configuration", "[database]") {
    IndexConfig cfg;
    cfg.max_children = 2;
    REQUIRE_THROWS_AS(Database(cfg), MiniDBError);
    cfg = {};
    cfg.initial_bucket_count = 0;
    REQUIRE_THROWS_AS(Database(cfg), MiniDBError);
}
