#include "minidb/Benchmark.h"
#include "minidb/BPlusTreeIndex.h"
#include "minidb/HashIndex.h"
#include "minidb/Error.h"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <locale>
#include <map>
#include <random>
#include <sstream>
#include <tuple>

namespace minidb
{
    namespace
    {
        //deterministic checksum encoding
        // for found status and row position
        // follows measurement rule 4

        inline std::uint64_t mixChecksum(std::uint64_t acc, const std::optional<RowId>& res) {
            constexpr std::uint64_t kPrime = 1099511628211ULL;
            if (res.has_value()) {
                acc = (acc ^ ((*res + 1ULL) * 0x9e3779b97f4a7c15ULL)) * kPrime;
            } else {
                acc = (acc ^ 0xdeadbeefcafebabeULL) * kPrime;
            }
            return acc;
        }

        void recordIndexCapacities(const IIndex& index, IndexKind kind, TrialResult& result) {
        if (kind == IndexKind::BPlusTree) {
            const auto& bpt = static_cast<const BPlusTreeIndex&>(index);
            result.bucket_count = 0;
            result.max_children = bpt.maxChildren();
        } else {
            const auto& hash = static_cast<const HashIndex&>(index);
            result.bucket_count = hash.bucketCount();
            result.max_children = 0;
        }
    }

        void checkStructureOutsideTiming(const IIndex& index, IndexKind kind) {
            bool ok = (kind == IndexKind::BPlusTree)
                          ? static_cast<const BPlusTreeIndex&>(index).validateStructure()
                          : static_cast<const HashIndex&>(index).validateStructure();
            if (!ok) {
                throw MiniDBError(ErrorCode::InvalidRecord, "Index structural validation failed after benchmark trial.");
            }
        }

        const char* orderName(InsertionOrder order) {
            return order == InsertionOrder::Sequential ? "sequential" : "shuffled";
        }

        const char* indexName(IndexKind index) {
            return index == IndexKind::Hash ? "hash" : "bplus";
        }

        const char* workloadName(Workload workload) {
            switch (workload) {
                case Workload::Build: return "build";
                case Workload::Insert: return "insert";
                case Workload::FindExisting: return "find_existing";
                case Workload::FindMissing: return "find_missing";
            }
            throw std::logic_error("Unknown benchmark workload.");
        }

        void writeCsvText(std::ostream& output, const std::string& text) {
            output << '"';
            for (char ch : text) {
                if (ch == '"') {
                    output << '"';
                }
                output << ch;
            }
            output << '"';
        }
        } // namespace

        std::unique_ptr<IIndex> Benchmark::makeIndex(IndexKind kind, const IndexConfig& config) const {
            if (kind == IndexKind::BPlusTree) {
                return std::make_unique<BPlusTreeIndex>(config.max_children);
            }
            return std::make_unique<HashIndex>(config.initial_bucket_count, config.max_load_factor);
        }

        std::vector<IndexEntry> Benchmark::makeEntries(const std::vector<TradeRecord>& records) const {
            std::vector<IndexEntry> entries;
            entries.reserve(records.size());
            for (std::size_t row = 0; row < records.size(); ++row) {
                entries.push_back(IndexEntry{records[row].trade_id, row});
            }
            return entries;
        }

        std::vector<IndexEntry> Benchmark::makeAdditionalEntries(
            const std::vector<TradeRecord>& records, std::size_t count) const
        {
            TradeId max_id = 0;
            for (const auto& r : records) {
                max_id = std::max(max_id, r.trade_id);
            }
            if (count > std::numeric_limits<TradeId>::max() - max_id ||
                count > std::numeric_limits<RowId>::max() - records.size()) {
                throw MiniDBError(ErrorCode::InvalidConfiguration, "Benchmark insertion range would overflow.");
            }
            std::vector<IndexEntry> additions;
            additions.reserve(count);
            std::size_t base_row = records.size();
            for (std::size_t i = 0; i < count; ++i) {
                additions.push_back(IndexEntry{max_id + 1 + i, base_row + i});
            }
            return additions;
        }

        Benchmark::LookupBatch Benchmark::makeQueries(
            const std::vector<TradeRecord>& records,
            std::size_t count,
            std::uint64_t seed,
            bool successful) const
        {
            LookupBatch batch;
            batch.keys.reserve(count);
            batch.expected.reserve(count);

            std::mt19937_64 rng(seed);

            if (successful) {
                std::uniform_int_distribution<std::size_t> dist(0, records.size() - 1);
                for (std::size_t i = 0; i < count; ++i) {
                    std::size_t row = dist(rng);
                    batch.keys.push_back(records[row].trade_id);
                    batch.expected.push_back( static_cast<RowId>(row) );
                }
            } else {
                TradeId max_id = 0;
                for (const auto& r : records) {
                    max_id = std::max(max_id, r.trade_id);
                }
                const auto additions = std::max<std::size_t>(1, records.size() / 10);
                const auto last_id = std::numeric_limits<TradeId>::max();
                if (additions >= last_id - max_id) {
                    throw MiniDBError(ErrorCode::InvalidConfiguration, "Benchmark has no disjoint missing-ID range.");
                }
                std::uniform_int_distribution<TradeId> dist(max_id + additions + 1, last_id);
                for (std::size_t i = 0; i < count; ++i) {
                    batch.keys.push_back(dist(rng));
                    batch.expected.push_back(std::nullopt);
                }
            }
            return batch;
        }

        TrialResult Benchmark::timeBuild(
            IndexKind kind,
            const std::vector<IndexEntry>& entries,
            InsertionOrder order,
            const BenchmarkConfig& config,
            std::size_t trial) const
        {
            auto index = makeIndex(kind, config.indexes);
            std::uint64_t checksum = 0;

            auto t0 = std::chrono::steady_clock::now();
            for (const auto& e : entries) {
                InsertStatus status = index->insert(e.key, e.row);
                checksum += (status == InsertStatus::Inserted) ? (e.row + 1ULL) : 0ULL;
            }
            auto t1 = std::chrono::steady_clock::now();

            // Validate structure and mappings OUTSIDE timed region
            checkStructureOutsideTiming(*index, kind);
            verifyMappings(*index, entries);

            TrialResult result{};
            result.dataset_size = entries.size();
            result.order = order;
            result.seed = config.seed;
            result.index = kind;
            result.workload = Workload::Build;
            result.trial = trial;
            result.operation_count = entries.size();
            result.elapsed_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
            result.checksum = checksum;
            recordIndexCapacities(*index, kind, result);
            return result;
        }

        TrialResult Benchmark::timeInsert(
            IndexKind kind,
            const std::vector<IndexEntry>& initial,
            const std::vector<IndexEntry>& additions,
            InsertionOrder order,
            const BenchmarkConfig& config,
            std::size_t trial) const
        {
            auto index = makeIndex(kind, config.indexes);
            // Prepopulate outside timed region (Measurement Rule 1)
            for (const auto& e : initial) {
                index->insert(e.key, e.row);
            }

            std::uint64_t checksum = 0;
            auto t0 = std::chrono::steady_clock::now();
            for (const auto& e : additions) {
                InsertStatus status = index->insert(e.key, e.row);
                checksum += (status == InsertStatus::Inserted) ? (e.row + 1ULL) : 0ULL;
            }
            auto t1 = std::chrono::steady_clock::now();

            // Validate outside timed region
            checkStructureOutsideTiming(*index, kind);
            verifyMappings(*index, initial);
            verifyMappings(*index, additions);

            TrialResult result{};
            result.dataset_size = initial.size();
            result.order = order;
            result.seed = config.seed;
            result.index = kind;
            result.workload = Workload::Insert;
            result.trial = trial;
            result.operation_count = additions.size();
            result.elapsed_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
            result.checksum = checksum;
            recordIndexCapacities(*index, kind, result);
            return result;
        }

        TrialResult Benchmark::timeLookup(
            IndexKind kind,
            const std::vector<IndexEntry>& entries,
            const LookupBatch& queries,
            Workload workload,
            InsertionOrder order,
            const BenchmarkConfig& config,
            std::size_t trial) const
        {
            auto index = makeIndex(kind, config.indexes);
            for (const auto& e : entries) {
                index->insert(e.key, e.row);
            }

            std::uint64_t checksum = 14695981039346656037ULL;
            auto t0 = std::chrono::steady_clock::now();
            for (TradeId key : queries.keys) {
                auto res = index->find(key);
                checksum = mixChecksum(checksum, res);
            }
            auto t1 = std::chrono::steady_clock::now();

            // Verify every single lookup matches expected outside timed region
            verifyQueries(*index, queries);

            TrialResult result{};
            result.dataset_size = entries.size();
            result.order = order;
            result.seed = config.seed;
            result.index = kind;
            result.workload = workload;
            result.trial = trial;
            result.operation_count = queries.keys.size();
            result.elapsed_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
            result.checksum = checksum;
            recordIndexCapacities(*index, kind, result);
            return result;
        }

        void Benchmark::verifyMappings(const IIndex& index, const std::vector<IndexEntry>& entries) const {
            for (const auto& e : entries) {
                auto found = index.find(e.key);
                if (!found.has_value() || *found != e.row) {
                    throw MiniDBError(ErrorCode::InvalidRow, "Benchmark verification failed: mapping mismatch.");
                }
            }
        }

        void Benchmark::verifyQueries(const IIndex& index, const LookupBatch& queries) const {
            for (std::size_t i = 0; i < queries.keys.size(); ++i) {
                auto actual = index.find(queries.keys[i]);
                if (actual != queries.expected[i]) {
                    throw MiniDBError(ErrorCode::InvalidRow, "Benchmark verification failed: query result mismatch.");
                }
            }
        }

        double Benchmark::median(std::vector<std::int64_t> durations) {
            if (durations.empty()) return 0.0;
            std::sort(durations.begin(), durations.end());
            std::size_t n = durations.size();
            if (n % 2 == 1) {
                return static_cast<double>(durations[n / 2]);
            }
            return 0.5 * (static_cast<double>(durations[n / 2 - 1]) + static_cast<double>(durations[n / 2]));
        }

        BenchmarkReport Benchmark::run(
            const std::vector<TradeRecord>& records,
            InsertionOrder order,
            const BenchmarkConfig& config) const
        {
            BenchmarkReport report{config, {}};
            if (records.empty()) return report;
            if (config.lookup_count == 0 || config.measured_trials == 0 ||
                config.warmup_trials > std::numeric_limits<std::size_t>::max() - config.measured_trials) {
                throw MiniDBError(ErrorCode::InvalidConfiguration, "Invalid benchmark trial or lookup count.");
            }

            auto entries = makeEntries(records);
            std::size_t add_count = std::max<std::size_t>(1, records.size() / 10); // m = max(1, n/10) per Design Doc
            auto additions = makeAdditionalEntries(records, add_count);
            if (order == InsertionOrder::Shuffled) {
                std::mt19937_64 rng(config.seed + 1);
                std::shuffle(additions.begin(), additions.end(), rng);
            }

            auto existing_queries = makeQueries(records, config.lookup_count, config.seed + 2, true);
            auto missing_queries  = makeQueries(records, config.lookup_count, config.seed + 3, false);

            std::size_t total_trials = config.warmup_trials + config.measured_trials;

            auto run_pair = [&](std::size_t t, auto&& fn) {
                // Measurement Rule 3: Alternate which implementation runs first between trials
                IndexKind first  = (t % 2 == 0) ? IndexKind::Hash : IndexKind::BPlusTree;
                IndexKind second = (t % 2 == 0) ? IndexKind::BPlusTree : IndexKind::Hash;

                TrialResult r1 = fn(first);
                TrialResult r2 = fn(second);
                if (r1.checksum != r2.checksum) {
                    throw MiniDBError(ErrorCode::InvalidRecord, "Hash and B+ Tree checksums diverged!");
                }
                if (t >= config.warmup_trials) {
                    std::size_t measured_idx = t - config.warmup_trials + 1;
                    r1.trial = measured_idx;
                    r2.trial = measured_idx;
                    // Store in consistent order (Hash then BPlusTree)
                    if (first == IndexKind::Hash) {
                        report.trials.push_back(r1);
                        report.trials.push_back(r2);
                    } else {
                        report.trials.push_back(r2);
                        report.trials.push_back(r1);
                    }
                }
            };

            for (std::size_t t = 0; t < total_trials; ++t) {
                run_pair(t, [&](IndexKind k) { return timeBuild(k, entries, order, config, t); });
                run_pair(t, [&](IndexKind k) { return timeInsert(k, entries, additions, order, config, t); });
                run_pair(t, [&](IndexKind k) { return timeLookup(k, entries, existing_queries, Workload::FindExisting, order, config, t); });
                run_pair(t, [&](IndexKind k) { return timeLookup(k, entries, missing_queries, Workload::FindMissing, order, config, t); });
            }

            return report;
        }

        BenchmarkReport Benchmark::runSuite(const BenchmarkConfig& config) const {
            BenchmarkReport full_report{config, {}};
            for (std::size_t n : config.dataset_sizes) {
                for (InsertionOrder order : config.orders) {
                    DatasetConfig ds_cfg{n, config.seed, order, 1};
                    auto records = DatasetGenerator::generate(ds_cfg);
                    auto sub = run(records, order, config);
                    full_report.trials.insert(full_report.trials.end(), sub.trials.begin(), sub.trials.end());
                }
            }
            return full_report;
        }

        void Benchmark::printSummary(const BenchmarkReport& report, std::ostream& out) const {
            using Group = std::tuple<std::size_t, InsertionOrder, IndexKind, Workload, std::size_t>;
            std::map<Group, std::vector<std::int64_t>> groups;
            for (const auto& result : report.trials) {
                groups[{result.dataset_size, result.order, result.index, result.workload,
                        result.operation_count}].push_back(result.elapsed_ns);
            }
            std::ostringstream summary;
            summary.imbue(std::locale::classic());
            summary << "Records  Order  Index  Workload  Median ms  ns/op  ops/s\n"
                    << std::fixed << std::setprecision(2);
            for (const auto& [group, times] : groups) {
                const auto [size, order, index, workload, operations] = group;
                const double elapsed = median(times);
                summary << size << "  " << orderName(order) << "  " << indexName(index)
                        << "  " << workloadName(workload) << "  " << elapsed / 1e6
                        << "  " << elapsed / operations << "  ";
                if (elapsed > 0) {
                    summary << operations * 1e9 / elapsed;
                } else {
                    summary << "n/a";
                }
                summary << '\n';
            }
            out << summary.str();
        }

        void Benchmark::saveCsv(const BenchmarkReport& report, const std::string& path) const {
            std::ofstream output(path, std::ios::binary);
            if (!output) {
                throw MiniDBError(ErrorCode::FileWrite, "Cannot open benchmark results: " + path);
            }
            output.imbue(std::locale::classic());
            output << "dataset_size,order,seed,index,workload,trial,operation_count,elapsed_ns,"
                      "checksum,bucket_count,max_children,compiler,optimization,machine\n";
            for (const auto& result : report.trials) {
                output << result.dataset_size << ',' << orderName(result.order) << ',' << result.seed
                       << ',' << indexName(result.index) << ',' << workloadName(result.workload)
                       << ',' << result.trial << ',' << result.operation_count << ',' << result.elapsed_ns
                       << ',' << result.checksum << ',' << result.bucket_count << ',' << result.max_children
                       << ',';
                writeCsvText(output, report.config.compiler_description);
                output << ',';
                writeCsvText(output, report.config.optimization_flags);
                output << ',';
                writeCsvText(output, report.config.machine_description);
                output << '\n';
            }
            output.close();
            if (!output) {
                throw MiniDBError(ErrorCode::FileWrite, "Could not finish writing benchmark results: " + path);
            }
        }

} // namespace minidb
