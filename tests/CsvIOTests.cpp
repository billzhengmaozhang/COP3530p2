#include <catch2/catch_test_macros.hpp>
#include "minidb/CsvIO.h"
#include "minidb/Error.h"
#include <filesystem>
#include <fstream>
#include <functional>
#include <random>
#include <sstream>

using namespace minidb;
namespace fs = std::filesystem;

namespace {
struct TempDir {
    fs::path path;
    TempDir() {
        path = fs::temp_directory_path() / ("minidb-test-" + std::to_string(std::random_device{}()));
        fs::create_directories(path);
    }
    ~TempDir() { std::error_code ignored; fs::remove_all(path, ignored); }
    std::string file(const std::string& name) const { return (path / name).string(); }
};

void writeFile(const std::string& path, const std::string& text) {
    std::ofstream out(path, std::ios::binary);
    out << text;
}

const char* header = "trade_id,symbol,price_cents,quantity,timestamp\n";

std::size_t errorLine(const std::function<void()>& fn) {
    try { fn(); } catch (const MiniDBError& e) { return e.line().value_or(0); }
    return 0;
}
}

TEST_CASE("CsvIO round trip preserves records", "[csv]") {
    TempDir dir;
    const std::vector<TradeRecord> records{{1, "AAPL", 18925, 100, 1793797200},
                                           {900, "BRK.B", 1, 4000000000u, 0}};
    REQUIRE(CsvIO::write(dir.file("t.csv"), records) == 2);
    const auto data = CsvIO::read(dir.file("t.csv"));
    REQUIRE(data.records.size() == 2);
    REQUIRE(data.records[1].symbol == "BRK.B");
    REQUIRE(data.records[1].quantity == 4000000000u);
    REQUIRE(data.source_lines == std::vector<std::size_t>{2, 3});
    // No staging leftovers beside the destination.
    std::size_t entries = 0;
    for (const auto& e : fs::directory_iterator(dir.path)) { (void)e; ++entries; }
    REQUIRE(entries == 1);
}

TEST_CASE("CsvIO overwrites an existing destination", "[csv]") {
    TempDir dir;
    CsvIO::write(dir.file("t.csv"), {{1, "AAPL", 10, 1, 1}, {2, "AAPL", 10, 1, 1}});
    CsvIO::write(dir.file("t.csv"), {{3, "MSFT", 10, 1, 1}});
    const auto data = CsvIO::read(dir.file("t.csv"));
    REQUIRE(data.records.size() == 1);
    REQUIRE(data.records[0].trade_id == 3);
}

TEST_CASE("CsvIO read tolerates CRLF, blank lines, spaces and a UTF-8 BOM", "[csv]") {
    TempDir dir;
    writeFile(dir.file("t.csv"),
              std::string("\xEF\xBB\xBF") + "trade_id,symbol,price_cents,quantity,timestamp\r\n"
              "\r\n1, AAPL ,100,5,10\r\n   \r\n2,MSFT,200,6,11\r\n");
    const auto data = CsvIO::read(dir.file("t.csv"));
    REQUIRE(data.records.size() == 2);
    REQUIRE(data.records[0].symbol == "AAPL");
    REQUIRE(data.source_lines == std::vector<std::size_t>{3, 5});
}

TEST_CASE("CsvIO read reports errors with line numbers", "[csv]") {
    TempDir dir;
    REQUIRE_THROWS_AS(CsvIO::read(dir.file("missing.csv")), MiniDBError);

    writeFile(dir.file("empty.csv"), "");
    REQUIRE_THROWS_AS(CsvIO::read(dir.file("empty.csv")), MiniDBError);

    writeFile(dir.file("badheader.csv"), "id,symbol,price,qty,ts\n");
    REQUIRE(errorLine([&] { CsvIO::read(dir.file("badheader.csv")); }) == 1);

    writeFile(dir.file("fields.csv"), std::string(header) + "1,AAPL,100,5\n");
    REQUIRE(errorLine([&] { CsvIO::read(dir.file("fields.csv")); }) == 2);

    writeFile(dir.file("extra.csv"), std::string(header) + "1,AAPL,100,5,1,9\n");
    REQUIRE(errorLine([&] { CsvIO::read(dir.file("extra.csv")); }) == 2);

    writeFile(dir.file("value.csv"), std::string(header) + "1,AAPL,100,5,1\n2,aapl,100,5,1\n");
    REQUIRE(errorLine([&] { CsvIO::read(dir.file("value.csv")); }) == 3);
}

TEST_CASE("CsvIO write rejects invalid input without clobbering the destination", "[csv]") {
    TempDir dir;
    REQUIRE_THROWS_AS(CsvIO::write("", {}), MiniDBError);
    CsvIO::write(dir.file("t.csv"), {{1, "AAPL", 10, 1, 1}});
    REQUIRE_THROWS_AS(CsvIO::write(dir.file("t.csv"), {{0, "AAPL", 10, 1, 1}}), MiniDBError);
    REQUIRE(CsvIO::read(dir.file("t.csv")).records.size() == 1);
    REQUIRE_THROWS_AS(CsvIO::write(dir.file("no-such-dir/t.csv"), {}), MiniDBError);
}
