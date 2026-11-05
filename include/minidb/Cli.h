#pragma once
#include "Benchmark.h"
#include "Database.h"
#include <iosfwd>
#include <optional>
#include <string>

namespace minidb {
class Cli {
public:
    Cli(Database& database, std::istream& input, std::ostream& output);
    // Returns an exit code; EOF and option 0 exit normally, without autosaving.
    int run();

private:
    void printMenu() const;
    std::optional<std::string> readLine(const std::string& prompt);
    void handleLoad();
    void handleInsert();
    void handleFind();
    void handleBenchmark();
    void handleSave();
    void printRecord(const TradeRecord& record) const;
    void printError(const std::string& message) const;
    static std::optional<IndexKind> parseIndexKind(const std::string& text);

    Database& database_;
    std::istream& input_;
    std::ostream& output_;
    Benchmark benchmark_;
};
} // namespace minidb
