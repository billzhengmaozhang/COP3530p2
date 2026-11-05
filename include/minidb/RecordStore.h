#pragma once
#include "Types.h"
#include <vector>

namespace minidb {
class Database;

class RecordStore {
public:
    TradeRecord get(RowId row) const;
    std::size_t size() const;
    // Read-only export view; do not retain across appends or database replacement.
    const std::vector<TradeRecord>& records() const;

private:
    friend class Database;
    // Only Database may append, so callers cannot bypass index maintenance.
    RowId append(const TradeRecord& record);
    std::vector<TradeRecord> rows_;
};
} // namespace minidb
