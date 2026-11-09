# MiniDB: proposed file structure and header declarations

C++17. This package contains declarations only: no function implementations.
The original MiniDB design document is unchanged. Function declarations use the
`minidb` namespace. Default field values initialize configuration data; they are
not function implementations.

## Proposed directories

| Path | Purpose |
|---|---|
| `CMakeLists.txt` | Future build configuration |
| `include/minidb/` | Header files included in this package |
| `src/` | Future implementations matching the headers, plus `main.cpp` |
| `tests/` | Future Catch2 tests |
| `data/` | Generated input CSV files |
| `results/` | Benchmark output CSV files |
| `docs/` | Design and benchmark analysis |

## Files and their functions

| Header | Future implementation | Main declarations |
|---|---|---|
| `Types.h` | None | TradeRecord, IndexEntry, enums, IndexConfig |
| `Error.h` | `src/Error.cpp` | MiniDBError constructor, code, line |
| `RecordValidator.h` | `src/RecordValidator.cpp` | validate, parse, parseTradeId |
| `IIndex.h` | `src/IIndex.cpp` | destructor, insert, find, size |
| `RecordStore.h` | `src/RecordStore.cpp` | get, size, records; private append |
| `HashIndex.h` | `src/HashIndex.cpp` | insert, find, size, bucketCount, loadFactor, validateStructure; hashing/rehash helpers |
| `BPlusTreeIndex.h` | `src/BPlusTreeIndex.cpp` | insert, find, size, maxChildren, height, validateStructure; split/search helpers |
| `Database.h` | `src/Database.cpp` | insert, find, size, replaceAll, load, save, validateConsistency |
| `CsvIO.h` | `src/CsvIO.cpp` | read, write; header/row parsing helpers |
| `DatasetGenerator.h` | `src/DatasetGenerator.cpp` | generate, writeCsv |
| `Benchmark.h` | `src/Benchmark.cpp` | runSuite, run, printSummary, saveCsv; timing/query/verification helpers |
| `Cli.h` | `src/Cli.cpp` | run; menu handlers and formatting helpers |
| No header | `src/main.cpp` | int main(); creates Database and Cli, then calls run |

`IIndex.cpp` must define the declared virtual destructor, even though the class
is abstract. The index constructors reject invalid capacities or load factors.
`Database.cpp` defines its private State with a RecordStore and both indexes.
Only Database appends records; it publishes replacement State only after full
validation and successful index construction. CsvData retains source line
numbers so Database::load can report duplicate IDs at their physical CSV lines.

Recoverable validation, configuration, CSV, and duplicate-import errors use
MiniDBError. Index/database insertion returns DuplicateKey for an existing ID.
A missing lookup returns nullopt. Structural failures are fatal internal errors;
validateStructure/validateConsistency provide untimed checks for tests/debugging.
Optional range queries are deliberately not included in the initial API.

Suggested future test files: RecordValidatorTests.cpp, HashIndexTests.cpp,
BPlusTreeIndexTests.cpp, DatabaseTests.cpp, CsvIOTests.cpp, and BenchmarkTests.cpp.

## Header contents

### include/minidb/Types.h

```cpp
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

namespace minidb {
using TradeId = std::uint64_t;
using RowId = std::size_t;

struct TradeRecord {
    TradeId trade_id;
    std::string symbol;
    std::int64_t price_cents;
    std::uint32_t quantity;
    std::int64_t timestamp;
};

struct IndexEntry {
    TradeId key;
    RowId row;
};

enum class InsertStatus { Inserted, DuplicateKey };
enum class IndexKind { Hash, BPlusTree };
enum class InsertionOrder { Sequential, Shuffled };

struct IndexConfig {
    std::size_t initial_bucket_count = 16;
    double max_load_factor = 0.75;
    std::size_t max_children = 32;
};
} // namespace minidb
```

### include/minidb/Error.h

```cpp
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
```

### include/minidb/RecordValidator.h

```cpp
#pragma once
#include "Types.h"
#include <array>
#include <string>
#include <string_view>

namespace minidb {
class RecordValidator {
public:
    // Returns normally if valid; otherwise throws MiniDBError.
    static void validate(const TradeRecord& record);
    // Input fields follow the CSV column order; output is a validated record.
    static TradeRecord parse(const std::array<std::string, 5>& fields);
    // Used by lookup prompts; rejects zero, overflow, negatives and trailing text.
    static TradeId parseTradeId(std::string_view text);

private:
    static bool isValidSymbol(std::string_view symbol);
    static std::string trim(std::string_view text);
};
} // namespace minidb
```

### include/minidb/IIndex.h

```cpp
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
```

### include/minidb/RecordStore.h

```cpp
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
```

### include/minidb/HashIndex.h

```cpp
#pragma once
#include "IIndex.h"
#include <vector>

namespace minidb {
class HashIndex : public IIndex {
public:
    explicit HashIndex(std::size_t initial_buckets = 16,
                       double max_load_factor = 0.75);
    InsertStatus insert(TradeId key, RowId row) override;
    std::optional<RowId> find(TradeId key) const override;
    std::size_t size() const override;
    bool validateStructure() const override;
    std::size_t bucketCount() const override;

    double loadFactor() const;

private:
    static std::uint64_t hashKey(TradeId key);
    std::size_t bucketFor(TradeId key) const;
    void rehash(std::size_t new_bucket_count);

    std::vector<std::vector<IndexEntry>> buckets_;
    std::size_t size_ = 0;
    double max_load_factor_;
};
} // namespace minidb
```

### include/minidb/BPlusTreeIndex.h

```cpp
#pragma once
#include "IIndex.h"
#include <memory>
#include <vector>

namespace minidb {
class BPlusTreeIndex : public IIndex {
public:
    explicit BPlusTreeIndex(std::size_t max_children = 32);
    InsertStatus insert(TradeId key, RowId row) override;
    std::optional<RowId> find(TradeId key) const override;
    std::size_t size() const override;
    bool validateStructure() const override;
    std::size_t maxChildren() const override;

    std::size_t height() const;

private:
    struct Node {
        bool is_leaf;
        std::vector<TradeId> keys;
        std::vector<RowId> rows; // Leaves only; aligned with keys.
        std::vector<std::unique_ptr<Node>> children; // Internal nodes only.
        Node* next = nullptr; // Non-owning leaf link.
    };

    struct SplitResult {
        TradeId separator;
        std::unique_ptr<Node> right;
    };

    const Node* findLeaf(TradeId key) const;
    std::size_t childFor(const Node& node, TradeId key) const;
    // Called after the public duplicate precheck. Returns a propagated split.
    std::optional<SplitResult> insertRecursive(Node& node, TradeId key, RowId row);
    SplitResult splitLeaf(Node& leaf);
    SplitResult splitInternal(Node& node);
    void createNewRoot(SplitResult split);
    bool validateNode(const Node& node, bool is_root, std::size_t depth,
                      std::optional<TradeId> lower_inclusive,
                      std::optional<TradeId> upper_exclusive,
                      std::optional<std::size_t>& leaf_depth,
                      std::size_t& entry_count) const;
    bool validateLeafLinks() const;

    std::unique_ptr<Node> root_;
    std::size_t size_ = 0;
    std::size_t max_children_;
};
} // namespace minidb
```

### include/minidb/Database.h

```cpp
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

    // Invalid records throw MiniDBError; duplicates return DuplicateKey.
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
```

### include/minidb/CsvIO.h

```cpp
#pragma once
#include "Types.h"
#include <array>
#include <string>
#include <vector>

namespace minidb {
struct CsvData {
    std::vector<TradeRecord> records;
    // One physical source line per record, for duplicate-ID diagnostics.
    std::vector<std::size_t> source_lines;
};

class CsvIO {
public:
    // Validates header/fields. Database builds indexes and detects duplicates.
    static CsvData read(const std::string& path);
    // Writes temporary sibling then replaces destination; returns saved count.
    static std::size_t write(const std::string& path,
                             const std::vector<TradeRecord>& records);

private:
    static void validateHeader(const std::string& line);
    static std::array<std::string, 5> splitRow(const std::string& line);
    static TradeRecord parseRow(const std::string& line,
                                std::size_t line_number);
    static std::string formatRow(const TradeRecord& record);
};
} // namespace minidb
```

### include/minidb/DatasetGenerator.h

```cpp
#pragma once
#include "Types.h"
#include <string>
#include <vector>

namespace minidb {
struct DatasetConfig {
    std::size_t count = 100000;
    std::uint64_t seed = 3530;
    InsertionOrder order = InsertionOrder::Sequential;
    TradeId first_id = 1; // Use a disjoint range for additional insertion records.
    TradeId id_stride = 1; // Gap between consecutive IDs; 2 leaves absent IDs between stored keys.
};

class DatasetGenerator {
public:
    static std::vector<TradeRecord> generate(const DatasetConfig& config);
    static std::size_t writeCsv(const std::string& path,
                                const DatasetConfig& config);
};
} // namespace minidb
```

### include/minidb/Benchmark.h

```cpp
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
    // Generates required datasets (stride-2 IDs, so absent IDs lie between stored
    // keys); warmups excluded from returned results. If progress is non-null, a
    // line is written after each dataset finishes.
    BenchmarkReport runSuite(const BenchmarkConfig& config,
                             std::ostream* progress = nullptr) const;
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
    // One trial per index: build (timed), then both lookup workloads on that
    // index, then the incremental inserts (timed, mutates the index). All
    // validation happens outside the timed regions.
    TrialResult timeBuild(IIndex& index, IndexKind kind,
                           const std::vector<IndexEntry>& entries,
                           InsertionOrder order, const BenchmarkConfig& config,
                           std::size_t trial) const;
    TrialResult timeInsert(IIndex& index, IndexKind kind,
                            std::size_t initial_size,
                            const std::vector<IndexEntry>& additions,
                            InsertionOrder order, const BenchmarkConfig& config,
                            std::size_t trial) const;
    TrialResult timeLookup(const IIndex& index, IndexKind kind,
                            std::size_t dataset_size, const LookupBatch& queries,
                            Workload workload, InsertionOrder order,
                            const BenchmarkConfig& config, std::size_t trial) const;
    void verifyMappings(const IIndex& index,
                         const std::vector<IndexEntry>& entries) const;
    void verifyQueries(const IIndex& index, const LookupBatch& queries) const;
    static double median(std::vector<std::int64_t> durations);
};
} // namespace minidb
```

### include/minidb/Cli.h

```cpp
#pragma once
#include "Benchmark.h"
#include "Database.h"
#include <iosfwd>
#include <optional>
#include <string>

namespace minidb {
class Cli {
public:
    Cli(Database& database, std::istream& input, std::ostream& output);
    // Returns an exit code; EOF and option 0 exit normally, without autosaving.
    int run();

private:
    void printMenu() const;
    std::optional<std::string> readLine(const std::string& prompt);
    void handleLoad();
    void handleInsert();
    void handleFind();
    void handleBenchmark();
    void handleSave();
    void printRecord(const TradeRecord& record) const;
    void printError(const std::string& message) const;
    static std::optional<IndexKind> parseIndexKind(const std::string& text);

    Database& database_;
    std::istream& input_;
    std::ostream& output_;
    Benchmark benchmark_;
};
} // namespace minidb
```

