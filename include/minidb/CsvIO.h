#pragma once
#include "Types.h"
#include <array>
#include <string>
#include <vector>

namespace minidb {
struct CsvData {
    std::vector<TradeRecord> records;
    // One physical source line per record, for duplicate-ID diagnostics.
    std::vector<std::size_t> source_lines;
};

class CsvIO {
public:
    // Validates header/fields. Database builds indexes and detects duplicates.
    static CsvData read(const std::string& path);
    // Writes temporary sibling then replaces destination; returns saved count.
    static std::size_t write(const std::string& path,
                             const std::vector<TradeRecord>& records);

private:
    static void validateHeader(const std::string& line);
    static std::array<std::string, 5> splitRow(const std::string& line);
    static TradeRecord parseRow(const std::string& line,
                                std::size_t line_number);
    static std::string formatRow(const TradeRecord& record);
};
} // namespace minidb
