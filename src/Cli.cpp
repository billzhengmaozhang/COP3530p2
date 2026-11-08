#include "minidb/Cli.h"
#include "minidb/Error.h"
#include "minidb/RecordValidator.h"
#include <array>
#include <charconv>
#include <istream>
#include <ostream>
#include <string_view>
#include <thread>

namespace minidb {
namespace {
std::string_view trim(std::string_view text) {
    constexpr auto whitespace = " \t\r\n\v\f";
    const auto first = text.find_first_not_of(whitespace);
    if (first == std::string_view::npos) {
        return {};
    }
    return text.substr(first, text.find_last_not_of(whitespace) - first + 1);
}

template<class UInt>
UInt parseUnsigned(std::string_view text, const char* message) {
    text = trim(text);
    if (text.empty()) {
        throw MiniDBError(ErrorCode::InvalidInput, message);
    }
    UInt value{};
    const char* end = text.data() + text.size();
    const auto parsed = std::from_chars(text.data(), end, value);
    if (parsed.ec != std::errc{} || parsed.ptr != end) {
        throw MiniDBError(ErrorCode::InvalidInput, message);
    }
    return value;
}

std::vector<std::size_t> parseSizes(std::string_view text) {
    constexpr auto message = "Dataset sizes must be positive comma-separated integers.";
    std::vector<std::size_t> sizes;
    for (;;) {
        const auto comma = text.find(',');
        const auto size = parseUnsigned<std::size_t>(text.substr(0, comma), message);
        if (size == 0) {
            throw MiniDBError(ErrorCode::InvalidInput, message);
        }
        sizes.push_back(size);
        if (comma == std::string_view::npos) {
            return sizes;
        }
        text.remove_prefix(comma + 1);
    }
}

void describeBuild(BenchmarkConfig& config) {
#if defined(__clang__)
    config.compiler_description = "Clang " __clang_version__;
#elif defined(__GNUC__)
    config.compiler_description = "GCC " __VERSION__;
#elif defined(_MSC_VER)
    config.compiler_description = "MSVC " + std::to_string(_MSC_FULL_VER);
#endif
#ifdef NDEBUG
    config.optimization_flags = "NDEBUG defined";
#else
    config.optimization_flags = "NDEBUG undefined";
#endif
#ifdef __OPTIMIZE__
    config.optimization_flags += ", compiler optimization enabled";
#endif
#if defined(_WIN32)
    config.machine_description = "Windows";
#elif defined(__APPLE__)
    config.machine_description = "macOS";
#elif defined(__linux__)
    config.machine_description = "Linux";
#else
    config.machine_description = "Unknown platform";
#endif
    config.machine_description += ", " + std::to_string(std::thread::hardware_concurrency()) +
                                  " logical threads";
}
} // namespace

Cli::Cli(Database& database, std::istream& input, std::ostream& output)
    : database_(database), input_(input), output_(output) {}

int Cli::run() {
    for (;;) {
        printMenu();
        const auto input = readLine("Choose an option: ");
        if (!input || trim(*input) == "0") {
            return 0;
        }
        const auto choice = trim(*input);
        try {
            if (choice == "1") {
                handleLoad();
            } else if (choice == "2") {
                handleInsert();
            } else if (choice == "3") {
                handleFind();
            } else if (choice == "4") {
                handleBenchmark();
            } else if (choice == "5") {
                handleSave();
            } else {
                printError("Choose a menu option from 0 through 5.");
            }
        } catch (const MiniDBError& error) {
            if (error.line()) {
                printError("Line " + std::to_string(*error.line()) + ": " + error.what());
            } else {
                printError(error.what());
            }
        }
        if (!input_) {
            return 0;
        }
    }
}

void Cli::printMenu() const {
    output_ << "\n1. Load CSV (replaces current records)\n"
               "2. Insert record\n"
               "3. Find trade by ID\n"
               "4. Compare index performance\n"
               "5. Save CSV\n"
               "0. Exit\n";
}

std::optional<std::string> Cli::readLine(const std::string& prompt) {
    output_ << prompt << std::flush;
    std::string line;
    if (std::getline(input_, line)) {
        return line;
    }
    return std::nullopt;
}

void Cli::handleLoad() {
    if (const auto path = readLine("CSV path: ")) {
        const auto count = database_.load(*path);
        output_ << "Loaded " << count << " records.\n";
    }
}

void Cli::handleInsert() {
    const std::array<const char*, 5> prompts{
        "Trade ID: ", "Symbol: ", "Price in cents: ", "Quantity: ", "Timestamp (Unix seconds): "
    };
    std::array<std::string, 5> fields;
    for (std::size_t i = 0; i < fields.size(); ++i) {
        const auto field = readLine(prompts[i]);
        if (!field) {
            return;
        }
        fields[i] = *field;
    }
    const auto record = RecordValidator::parse(fields);
    if (database_.insert(record) == InsertStatus::DuplicateKey) {
        printError("Trade ID " + std::to_string(record.trade_id) + " already exists.");
    } else {
        output_ << "Inserted trade " << record.trade_id << ".\n";
    }
}

void Cli::handleFind() {
    const auto id_text = readLine("Trade ID: ");
    if (!id_text) {
        return;
    }
    const auto id = RecordValidator::parseTradeId(*id_text);
    const auto name = readLine("Index (hash/bplus): ");
    if (!name) {
        return;
    }
    const auto index = parseIndexKind(*name);
    if (!index) {
        printError("Choose hash or bplus.");
        return;
    }
    if (const auto record = database_.find(id, *index)) {
        printRecord(*record);
    } else {
        output_ << "Trade " << id << " not found.\n";
    }
}

void Cli::handleBenchmark() {
    BenchmarkConfig config;
    const auto sizes = readLine("Dataset sizes (comma-separated, default 100000,250000,500000): ");
    if (!sizes) {
        return;
    }
    if (!trim(*sizes).empty()) {
        config.dataset_sizes = parseSizes(*sizes);
    }
    const auto order = readLine("Insertion order (sequential/shuffled/both, default both): ");
    if (!order) {
        return;
    }
    const auto order_name = trim(*order);
    if (order_name == "sequential") {
        config.orders = {InsertionOrder::Sequential};
    } else if (order_name == "shuffled") {
        config.orders = {InsertionOrder::Shuffled};
    } else if (!order_name.empty() && order_name != "both") {
        throw MiniDBError(ErrorCode::InvalidInput, "Choose sequential, shuffled, or both.");
    }
    const auto seed = readLine("Seed (default 3530): ");
    if (!seed) {
        return;
    }
    if (!trim(*seed).empty()) {
        config.seed = parseUnsigned<std::uint64_t>(*seed, "Seed must be an unsigned integer in range.");
    }
    const auto path = readLine("Results CSV path (default results/benchmark.csv): ");
    if (!path) {
        return;
    }
    const auto destination = trim(*path).empty() ? "results/benchmark.csv" : *path;
    describeBuild(config);
    output_ << "Running benchmarks...\n" << std::flush;
    const auto report = benchmark_.runSuite(config);
    benchmark_.saveCsv(report, destination);
    benchmark_.printSummary(report, output_);
    output_ << "Saved benchmark results to " << destination << ".\n";
}

void Cli::handleSave() {
    if (const auto path = readLine("CSV path: ")) {
        const auto count = database_.save(*path);
        output_ << "Saved " << count << " records.\n";
    }
}

void Cli::printRecord(const TradeRecord& record) const {
    const auto cents = record.price_cents % 100;
    output_ << "trade_id=" << record.trade_id << " symbol=" << record.symbol
            << " price=$" << record.price_cents / 100 << '.' << (cents < 10 ? "0" : "")
            << cents << " quantity=" << record.quantity << " timestamp=" << record.timestamp << '\n';
}

void Cli::printError(const std::string& message) const {
    output_ << "Error: " << message << '\n';
}

std::optional<IndexKind> Cli::parseIndexKind(const std::string& text) {
    const auto name = trim(text);
    if (name == "hash") {
        return IndexKind::Hash;
    }
    if (name == "bplus") {
        return IndexKind::BPlusTree;
    }
    return std::nullopt;
}
} // namespace minidb
