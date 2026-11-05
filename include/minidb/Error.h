#pragma once
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>

namespace minidb {
enum class ErrorCode {
    InvalidInput, InvalidRecord, DuplicateId,
    InvalidRow, FileRead, FileWrite, InvalidConfiguration
};

// Recoverable errors carry a message and, for CSV data, a physical line number.
class MiniDBError : public std::runtime_error {
public:
    MiniDBError(ErrorCode code, const std::string& message,
                std::optional<std::size_t> line = std::nullopt);
    ErrorCode code() const noexcept;
    std::optional<std::size_t> line() const noexcept;

private:
    ErrorCode code_;
    std::optional<std::size_t> line_;
};
} // namespace minidb
