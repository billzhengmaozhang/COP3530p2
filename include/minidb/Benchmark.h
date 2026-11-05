#pragma once
#include "DatasetGenerator.h"
#include "IIndex.h"
#include <iosfwd>
#include <memory>
#include <string>
#include <vector>

namespace minidb {
enum class Workload { Build, Insert, FindExisting, FindMissing };

struct BenchmarkConfig {
    IndexConfig indexes;
    std::uint64_t seed = 3530;
    std::size_t warmup_trials = 1;
    std::size_t measured_trials = 5;
    std::size_t lookup_count = 100000;
    std::vector<std::size_t> dataset_sizes = {100000, 250000, 500000};
    std::vector<InsertionOrder> orders = {
        InsertionOrder::Sequential, InsertionOrder::Shuffled
    };
    std::string compiler_description;
    std::string optimization_flags;
    std::string machine_description;
};

struct TrialResult {
    std::size_t dataset_size;
    InsertionOrder order;
    std::uint64_t seed;
    IndexKind index;
    Workload workload;
    std::size_t trial;
    std::size_t operation_count;
    std::int64_t elapsed_ns;
    std::uint64_t checksum;
    std::size_t bucket_count; // 0 for B+ tree.
    std::size_t max_children; // 0 for hash table.
};

struct BenchmarkReport {
    BenchmarkConfig config;
    std::vector<TrialResult> trials;
};

class Benchmark {
public:
    // Generates required datasets; warmups excluded from returned results.
    BenchmarkReport runSuite(const BenchmarkConfig& config) const;
    // Uses supplied records without modifying the active Database.
    BenchmarkReport run(const std::vector<TradeRecord>& records,
                         InsertionOrder order,
                         const BenchmarkConfig& config) const;
    void printSummary(const BenchmarkReport& report, std::ostream& out) const;
    void saveCsv(const BenchmarkReport& report, const std::string& path) const;

private:
    struct LookupBatch {
        std::vector<TradeId> keys;
        std::vector<std::optional<RowId>> expected;
    };

    std::unique_ptr<IIndex> makeIndex(IndexKind kind,
                                     const IndexConfig& config) const;
    std::vector<IndexEntry> makeEntries(
        const std::vector<TradeRecord>& records) const;
    std::vector<IndexEntry> makeAdditionalEntries(
        const std::vector<TradeRecord>& records, std::size_t count) const;
    LookupBatch makeQueries(const std::vector<TradeRecord>& records,
                            std::size_t count, std::uint64_t seed,
                            bool successful) const;
    // Each trial constructs fresh indexes and validates outside timed regions.
    TrialResult timeBuild(IndexKind kind, const std::vector<IndexEntry>& entries,
                           InsertionOrder order, const BenchmarkConfig& config,
                           std::size_t trial) const;
    TrialResult timeInsert(IndexKind kind, const std::vector<IndexEntry>& initial,
                            const std::vector<IndexEntry>& additions,
                            InsertionOrder order, const BenchmarkConfig& config,
                            std::size_t trial) const;
    TrialResult timeLookup(IndexKind kind, const std::vector<IndexEntry>& entries,
                            const LookupBatch& queries, Workload workload,
                            InsertionOrder order, const BenchmarkConfig& config,
                            std::size_t trial) const;
    void verifyMappings(const IIndex& index,
                         const std::vector<IndexEntry>& entries) const;
    void verifyQueries(const IIndex& index, const LookupBatch& queries) const;
    static double median(std::vector<std::int64_t> durations);
};
} // namespace minidb
