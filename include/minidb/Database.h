#pragma once
#include "Types.h"
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace minidb {
class Database {
public:
    explicit Database(const IndexConfig& config = {});
    ~Database(); // Defined in .cpp after State is complete.
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    // Invalid records throw MiniDBError; duplicates return DuplicateKey. If an
    // insert fails part-way (e.g. out of memory) the database is rebuilt from
    // the previously committed records, so the store and indexes never disagree.
    // Not thread-safe.
    InsertStatus insert(const TradeRecord& record);
    // Queries the chosen index and returns a copy of the complete record.
    std::optional<TradeRecord> find(TradeId id, IndexKind index) const;
    std::size_t size() const;

    // Stage and replace all records/indexes together; failures preserve old state.
    void replaceAll(const std::vector<TradeRecord>& records);
    std::size_t load(const std::string& path);
    std::size_t save(const std::string& path) const;
    bool validateConsistency() const;

private:
    struct State; // RecordStore + HashIndex + BPlusTreeIndex; defined in .cpp.
    std::unique_ptr<State> buildState(
        const std::vector<TradeRecord>& records,
        const std::vector<std::size_t>* source_lines = nullptr) const;

    IndexConfig config_;
    std::unique_ptr<State> state_;
};
} // namespace minidb
