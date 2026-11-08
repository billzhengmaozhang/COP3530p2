#include "minidb/CsvIO.h"
#include "minidb/Error.h"
#include "minidb/RecordValidator.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <utility>

namespace minidb {
namespace {
namespace fs = std::filesystem;
constexpr auto header = "trade_id,symbol,price_cents,quantity,timestamp";
constexpr auto whitespace = " \t\r\n\v\f";

class SaveStaging {
public:
    explicit SaveStaging(const fs::path& destination) {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        for (int attempt = 0; attempt < 100; ++attempt) {
            auto candidate = destination.parent_path() /
                (".minidb-save-" + std::to_string(stamp) + "-" + std::to_string(attempt));
            std::error_code error;
            if (fs::create_directory(candidate, error)) {
                directory_ = std::move(candidate);
                return;
            }
            if (error && error != std::errc::file_exists) {
                throw MiniDBError(ErrorCode::FileWrite,
                                  "Cannot stage CSV beside " + destination.string() + ": " + error.message());
            }
        }
        throw MiniDBError(ErrorCode::FileWrite, "Could not create temporary CSV staging directory.");
    }

    ~SaveStaging() {
        std::error_code ignored;
        fs::remove(file(), ignored);
        fs::remove(directory_, ignored);
    }

    fs::path file() const {
        return directory_ / "trades.csv";
    }

private:
    fs::path directory_;
};
} // namespace

CsvData CsvIO::read(const std::string& path) {
    std::ifstream input(path);
    if (!input) {
        throw MiniDBError(ErrorCode::FileRead, "Cannot open CSV: " + path);
    }

    CsvData data;
    bool has_header = false;
    std::string line;
    std::size_t line_number = 0;
    while (std::getline(input, line)) {
        ++line_number;
        if (line.find_first_not_of(whitespace) == std::string::npos) {
            continue;
        }
        if (!has_header) {
            try {
                validateHeader(line);
            } catch (const MiniDBError& error) {
                throw MiniDBError(error.code(), error.what(), line_number);
            }
            has_header = true;
        } else {
            data.records.push_back(parseRow(line, line_number));
            data.source_lines.push_back(line_number);
        }
    }
    if (input.bad() || !input.eof()) {
        throw MiniDBError(ErrorCode::FileRead, "Could not finish reading CSV: " + path, line_number + 1);
    }
    if (!has_header) {
        throw MiniDBError(ErrorCode::InvalidRow, "Missing CSV header.", line_number + 1);
    }
    return data;
}

std::size_t CsvIO::write(const std::string& path, const std::vector<TradeRecord>& records) {
    if (path.empty()) {
        throw MiniDBError(ErrorCode::FileWrite, "CSV destination is empty.");
    }
    const fs::path destination(path);
    const SaveStaging staging(destination);
    const auto temporary = staging.file();
    std::ofstream output(temporary, std::ios::binary);
    if (!output) {
        throw MiniDBError(ErrorCode::FileWrite, "Cannot open temporary CSV for: " + path);
    }
    output << header << '\n';
    for (const auto& record : records) {
        RecordValidator::validate(record);
        output << formatRow(record) << '\n';
        if (!output) {
            break;
        }
    }
    output.close();
    if (!output) {
        throw MiniDBError(ErrorCode::FileWrite, "Could not finish writing CSV: " + path);
    }
    std::error_code error;
    fs::rename(temporary, destination, error);
    if (error) {
        throw MiniDBError(ErrorCode::FileWrite, "Could not replace CSV " + path + ": " + error.message());
    }
    return records.size();
}

void CsvIO::validateHeader(const std::string& line) {
    const std::array<std::string, 5> expected{"trade_id", "symbol", "price_cents", "quantity", "timestamp"};
    if (splitRow(line) != expected) {
        throw MiniDBError(ErrorCode::InvalidRow, std::string("Expected CSV header: ") + header);
    }
}

std::array<std::string, 5> CsvIO::splitRow(const std::string& line) {
    std::array<std::string, 5> fields;
    std::size_t start = 0;
    for (std::size_t i = 0; i < fields.size(); ++i) {
        const auto comma = line.find(',', start);
        if ((comma == std::string::npos) != (i + 1 == fields.size())) {
            throw MiniDBError(ErrorCode::InvalidRow, "Expected exactly five CSV fields.");
        }
        auto field = line.substr(start, comma == std::string::npos ? comma : comma - start);
        const auto first = field.find_first_not_of(whitespace);
        if (first != std::string::npos) {
            fields[i] = field.substr(first, field.find_last_not_of(whitespace) - first + 1);
        }
        if (comma != std::string::npos) {
            start = comma + 1;
        }
    }
    return fields;
}

TradeRecord CsvIO::parseRow(const std::string& line, std::size_t line_number) {
    try {
        return RecordValidator::parse(splitRow(line));
    } catch (const MiniDBError& error) {
        throw MiniDBError(ErrorCode::InvalidRow, error.what(), line_number);
    }
}

std::string CsvIO::formatRow(const TradeRecord& record) {
    return std::to_string(record.trade_id) + ',' + record.symbol + ',' +
           std::to_string(record.price_cents) + ',' + std::to_string(record.quantity) + ',' +
           std::to_string(record.timestamp);
}
} // namespace minidb
