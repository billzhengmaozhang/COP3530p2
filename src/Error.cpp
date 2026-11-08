#include "minidb/Error.h"

namespace minidb {
MiniDBError::MiniDBError(ErrorCode code, const std::string& message,
                         std::optional<std::size_t> line)
    : std::runtime_error(message), code_(code), line_(line) {}

ErrorCode MiniDBError::code() const noexcept {
    return code_;
}

std::optional<std::size_t> MiniDBError::line() const noexcept {
    return line_;
}
} // namespace minidb
