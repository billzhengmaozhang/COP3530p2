#include <catch2/catch_test_macros.hpp>
#include "minidb/Benchmark.h"
#include "minidb/Cli.h"
#include "minidb/Error.h"
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>

using namespace minidb;
namespace fs = std::filesystem;

namespace {
BenchmarkConfig smallConfig() {
    BenchmarkConfig c;
    c.dataset_sizes = {300};
    c.lookup_count = 200;
    c.warmup_trials = 1;
    c.measured_trials = 2;
    return c;
}
}

TEST_CASE("Benchmark produces verified trials for both indexes", "[benchmark]") {
    Benchmark bench;
    std::ostringstream progress;
    const auto report = bench.runSuite(smallConfig(), &progress);
    // 1 size x 2 orders x 2 trials x 4 workloads x 2 indexes
    REQUIRE(report.trials.size() == 2 * 2 * 4 * 2);
    REQUIRE(progress.str().find("finished 300 records") != std::string::npos);
    std::set<int> workloads;
    for (const auto& t : report.trials) {
        REQUIRE(t.elapsed_ns >= 0);
        REQUIRE(t.trial >= 1);
        workloads.insert(static_cast<int>(t.workload));
        if (t.index == IndexKind::Hash) { REQUIRE(t.bucket_count > 0); REQUIRE(t.max_children == 0); }
        else { REQUIRE(t.bucket_count == 0); REQUIRE(t.max_children == 32); }
    }
    REQUIRE(workloads.size() == 4);
}

TEST_CASE("Benchmark is deterministic apart from timing", "[benchmark]") {
    Benchmark bench;
    const auto a = bench.runSuite(smallConfig());
    const auto b = bench.runSuite(smallConfig());
    REQUIRE(a.trials.size() == b.trials.size());
    for (std::size_t i = 0; i < a.trials.size(); ++i) {
        REQUIRE(a.trials[i].checksum == b.trials[i].checksum);
    }
}

TEST_CASE("Benchmark handles dense IDs and tiny datasets", "[benchmark]") {
    Benchmark bench;
    auto cfg = smallConfig();
    cfg.lookup_count = 10;
    const auto dense = DatasetGenerator::generate({50, 1, InsertionOrder::Sequential, 1, 1});
    REQUIRE_NOTHROW(bench.run(dense, InsertionOrder::Sequential, cfg));
    const auto one = DatasetGenerator::generate({1, 1, InsertionOrder::Sequential, 1, 1});
    REQUIRE_NOTHROW(bench.run(one, InsertionOrder::Shuffled, cfg));
    REQUIRE(bench.run({}, InsertionOrder::Sequential, cfg).trials.empty());
    cfg.measured_trials = 0;
    REQUIRE_THROWS_AS(bench.run(dense, InsertionOrder::Sequential, cfg), MiniDBError);
}

TEST_CASE("Benchmark summary and CSV output", "[benchmark]") {
    Benchmark bench;
    const auto report = bench.runSuite(smallConfig());
    std::ostringstream summary;
    bench.printSummary(report, summary);
    REQUIRE(summary.str().find("find_missing") != std::string::npos);

    const auto path = (fs::temp_directory_path() / "minidb-bench-test.csv").string();
    bench.saveCsv(report, path);
    std::ifstream in(path);
    std::string line;
    std::getline(in, line);
    REQUIRE(line.rfind("dataset_size,order,seed,index,workload", 0) == 0);
    std::size_t rows = 0;
    while (std::getline(in, line)) ++rows;
    REQUIRE(rows == report.trials.size());
    fs::remove(path);
    REQUIRE_THROWS_AS(bench.saveCsv(report, "/no/such/dir/x.csv"), MiniDBError);
}

namespace {
std::string runCli(Database& db, const std::string& script) {
    std::istringstream in(script);
    std::ostringstream out;
    Cli cli(db, in, out);
    REQUIRE(cli.run() == 0);
    return out.str();
}
}

TEST_CASE("Cli insert, find and error handling", "[cli]") {
    Database db;
    const auto out = runCli(db,
        "2\n1001\nAAPL\n18925\n100\n1793797200\n"   // insert
        "3\n1001\nhash\n"                            // find via hash
        "3\n1001\nbplus\n"                           // find via tree
        "3\n999\nhash\n"                             // missing
        "3\nabc\n"                                   // bad id
        "3\n1001\nbtree\n"                           // bad index name
        "2\n1001\nAAPL\n1\n1\n1\n"                   // duplicate
        "2\n1002\nbad symbol\n1\n1\n1\n"             // invalid record
        "9\n"                                        // bad menu choice
        "0\n");
    REQUIRE(out.find("Inserted trade 1001.") != std::string::npos);
    REQUIRE(out.find("price=$189.25") != std::string::npos);
    REQUIRE(out.find("Trade 999 not found.") != std::string::npos);
    REQUIRE(out.find("Choose hash or bplus.") != std::string::npos);
    REQUIRE(out.find("Trade ID 1001 already exists.") != std::string::npos);
    REQUIRE(out.find("Choose a menu option from 0 through 5.") != std::string::npos);
    REQUIRE(db.size() == 1);
}

TEST_CASE("Cli load, save and EOF handling", "[cli]") {
    Database db;
    const auto dir = fs::temp_directory_path() / "minidb-cli-test";
    fs::create_directories(dir);
    const auto path = (dir / "t.csv").string();
    runCli(db, "2\n5\nMSFT\n100\n2\n3\n5\n" + path + "\n0\n");
    Database other;
    const auto out = runCli(other, "1\n" + path + "\n1\n/no/such/file.csv\n");  // ends at EOF
    REQUIRE(out.find("Loaded 1 records.") != std::string::npos);
    REQUIRE(out.find("Error: Cannot open CSV") != std::string::npos);
    REQUIRE(other.size() == 1);
    fs::remove_all(dir);
}

TEST_CASE("Cli benchmark prompts validate input", "[cli]") {
    Database db;
    const auto out = runCli(db, "4\n0\n"      // zero size rejected
                                "4\n10,x\n"
                                "4\n\nfoo\n"
                                "0\n");
    REQUIRE(out.find("Dataset sizes must be positive") != std::string::npos);
    REQUIRE(out.find("Choose sequential, shuffled, or both.") != std::string::npos);

    const auto path = (fs::temp_directory_path() / "minidb-cli-bench.csv").string();
    const auto ran = runCli(db, "4\n200\nsequential\n7\n" + path + "\n0\n");
    REQUIRE(ran.find("Records  Order  Index") != std::string::npos);
    REQUIRE(fs::exists(path));
    fs::remove(path);
}
