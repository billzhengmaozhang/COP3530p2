#include "minidb/Database.h"
#include "minidb/BPlusTreeIndex.h"
#include "minidb/CsvIO.h"
#include "minidb/Error.h"
#include "minidb/HashIndex.h"
#include "minidb/RecordStore.h"
#include "minidb/RecordValidator.h"
#include <stdexcept>

namespace minidb {
struct Database::State {
    explicit State(const IndexConfig& config)
        : hash(config.initial_bucket_count, config.max_load_factor),
          tree(config.max_children) {}

    InsertStatus insert(const TradeRecord& record) {
        RecordValidator::validate(record);
        if (hash.find(record.trade_id)) {
            return InsertStatus::DuplicateKey;
        }
        const RowId row = store.append(record);
        if (hash.insert(record.trade_id, row) != InsertStatus::Inserted ||
            tree.insert(record.trade_id, row) != InsertStatus::Inserted) {
            throw std::logic_error("Database indexes disagree on a new trade ID.");
        }
        return InsertStatus::Inserted;
    }

    RecordStore store;
    HashIndex hash;
    BPlusTreeIndex tree;
};

Database::Database(const IndexConfig& config)
    : config_(config), state_(std::make_unique<State>(config)) {}

Database::~Database() = default;

InsertStatus Database::insert(const TradeRecord& record) {
    const auto committed = state_->store.size();
    try {
        return state_->insert(record);
    } catch (...) {
        // Validation errors are thrown before anything is stored. Any other
        // failure (for example bad_alloc) may leave the new row in the store
        // and in only some indexes, so restore the committed prefix.
        if (state_->store.size() != committed) {
            auto rows = state_->store.records();
            rows.erase(rows.begin() + static_cast<std::ptrdiff_t>(committed), rows.end());
            state_ = buildState(rows);
        }
        throw;
    }
}

std::optional<TradeRecord> Database::find(TradeId id, IndexKind index) const {
    const auto row = index == IndexKind::Hash ? state_->hash.find(id) : state_->tree.find(id);
    if (row) {
        return state_->store.get(*row);
    }
    return std::nullopt;
}

std::size_t Database::size() const {
    return state_->store.size();
}

void Database::replaceAll(const std::vector<TradeRecord>& records) {
    state_ = buildState(records);
}

std::size_t Database::load(const std::string& path) {
    const auto data = CsvIO::read(path);
    state_ = buildState(data.records, &data.source_lines);
    return size();
}

std::size_t Database::save(const std::string& path) const {
    return CsvIO::write(path, state_->store.records());
}

bool Database::validateConsistency() const {
    if (state_->hash.size() != size() || state_->tree.size() != size() ||
        !state_->hash.validateStructure() || !state_->tree.validateStructure()) {
        return false;
    }
    const auto& records = state_->store.records();
    for (RowId row = 0; row < records.size(); ++row) {
        const auto id = records[row].trade_id;
        if (state_->hash.find(id) != row || state_->tree.find(id) != row) {
            return false;
        }
    }
    return true;
}

std::unique_ptr<Database::State> Database::buildState(
    const std::vector<TradeRecord>& records, const std::vector<std::size_t>* source_lines) const {
    auto next = std::make_unique<State>(config_);
    for (RowId row = 0; row < records.size(); ++row) {
        if (next->insert(records[row]) == InsertStatus::DuplicateKey) {
            std::optional<std::size_t> line;
            if (source_lines) {
                line = (*source_lines)[row];
            }
            throw MiniDBError(ErrorCode::DuplicateId,
                              "Trade ID " + std::to_string(records[row].trade_id) + " already exists.",
                              line);
        }
    }
    return next;
}
} // namespace minidb
