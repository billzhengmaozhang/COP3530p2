#include "minidb/RecordStore.h"
#include "minidb/Error.h"

namespace minidb {
RowId RecordStore::append(const TradeRecord& record) {
    const auto row = rows_.size();
    rows_.push_back(record);
    return row;
}

TradeRecord RecordStore::get(RowId row) const {
    if (row >= rows_.size()) {
        throw MiniDBError(ErrorCode::InvalidRow, "Record row is out of bounds.");
    }
    return rows_[row];
}

std::size_t RecordStore::size() const {
    return rows_.size();
}

const std::vector<TradeRecord>& RecordStore::records() const {
    return rows_;
}
} // namespace minidb
