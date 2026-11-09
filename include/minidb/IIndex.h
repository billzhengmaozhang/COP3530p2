#pragma once
#include "Types.h"
#include <optional>

namespace minidb {
class IIndex {
public:
    virtual ~IIndex();
    // DuplicateKey leaves the existing mapping unchanged.
    virtual InsertStatus insert(TradeId key, RowId row) = 0;
    // nullopt means missing; RowId{0} is a valid match.
    virtual std::optional<RowId> find(TradeId key) const = 0;
    virtual std::size_t size() const = 0;
    // Untimed structural self-check used by tests and the benchmark.
    virtual bool validateStructure() const = 0;
    // Capacity details recorded with benchmark trials; 0 when not applicable.
    virtual std::size_t bucketCount() const { return 0; }
    virtual std::size_t maxChildren() const { return 0; }
};
} // namespace minidb
