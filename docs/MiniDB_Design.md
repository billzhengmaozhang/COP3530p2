# MiniDB: Component Design and Interface Specification

**Course:** COP3530 — Project 2  
**Status:** Proposed implementation design  
**Basis:** MiniDB proposal dated October 8, 2026  
**Implementation:** C++17, CMake, Catch2

## 1. Purpose and scope

MiniDB is a single-table, single-user, in-memory database for synthetic trade records. Its main purpose is to compare two indexes implemented from scratch: a separate-chaining hash table and a B+ tree. Both indexes support the same insertion and exact-ID lookup operations over the same records.

The required features are CSV loading and saving, inserting records, finding records through either index, generating reproducible datasets, and benchmarking at least 100,000 records. Dataset sizes are 100,000, 250,000, and 500,000 records, with sequential and shuffled insertion orders.

Deletion, updates, SQL parsing, transactions, concurrency, and disk-page management are outside the initial scope. B+ tree range queries are an optional extension. The detailed API names, validation rules, capacities, and benchmark defaults below are proposed implementation choices, rather than additional course requirements.

## 2. Architecture and ownership

| Component | Responsibility | Main input | Main output |
|---|---|---|---|
| Record model and validator | Define valid records | Typed fields or parsed CSV fields | Valid record or validation error |
| Record store | Own full trade records | Valid record or row position | Stable row position or record copy |
| Hash index | Map trade IDs to row positions | ID and optional row position | Insertion status or matching row position |
| B+ tree index | Provide the same mapping using a balanced tree | ID and optional row position | Insertion status or matching row position |
| Database coordinator | Keep records and both indexes consistent | Validated database operation | Result, record, count, or error |
| CSV reader/writer | Convert between CSV and records | File path or records | Parsed records, saved file, or diagnostic |
| Dataset generator | Produce repeatable synthetic input | Size, seed, insertion order | Valid unique records and CSV dataset |
| Benchmark runner | Compare the two index implementations | Dataset and benchmark configuration | Console summary and results CSV |
| Command-line interface | Collect user requests and format results | Menu choices and entered fields | Human-readable success, data, or error |

The interface calls the database coordinator. The coordinator owns one record store and both indexes. CSV loading builds a temporary database and replaces the active one only after success. Benchmarks create separate index instances and do not modify the active database.

The record store holds each full record once. Each index stores only a `(trade_id, row_position)` pair. Standard vectors and smart pointers may implement storage and node ownership; neither compared index may delegate its mapping to `std::map`, `std::unordered_map`, or an external index library.

## 3. Shared data types and contracts

```cpp
using TradeId = std::uint64_t;
using RowId = std::size_t;

struct TradeRecord {
    TradeId trade_id;
    std::string symbol;
    std::int64_t price_cents;
    std::uint32_t quantity;
    std::int64_t timestamp;
};

enum class InsertStatus { Inserted, DuplicateKey };
enum class IndexKind { Hash, BPlusTree };

class IIndex {
public:
    virtual ~IIndex() = default;
    virtual InsertStatus insert(TradeId key, RowId row) = 0;
    virtual std::optional<RowId> find(TradeId key) const = 0;
    virtual std::size_t size() const = 0;
    virtual bool validateStructure() const = 0;       // untimed self-check
    virtual std::size_t bucketCount() const { return 0; }  // hash only
    virtual std::size_t maxChildren() const { return 0; }  // B+ tree only
};
```

`find` returns `std::nullopt` for a missing key; row position zero is a valid result. Finding a key never inserts it. A duplicate insertion leaves the original mapping and index size unchanged. The shared interface is used by the benchmark runner as well as the database.

### Record model and validator

| Field | Accepted input | Behavior/output |
|---|---|---|
| `trade_id` | Integer from 1 through `UINT64_MAX` | Unique primary key; duplicate detection belongs to the database/index |
| `symbol` | 1–10 ASCII characters; first character A–Z; remaining characters A–Z, 0–9, `.` or `-` | Store exactly as entered; reject unsupported characters |
| `price_cents` | Positive integer representable by `int64_t` | Store cents exactly; no floating-point conversion |
| `quantity` | Integer from 1 through `UINT32_MAX` | Store positive number of shares |
| `timestamp` | Nonnegative integer representable by `int64_t` | Interpret as Unix seconds in UTC |

**Input:** Five fields from the menu or CSV reader.  
**Output:** A `TradeRecord`, or an error identifying the field and reason.

Numeric input must consume the entire field after trimming surrounding ASCII whitespace. Reject overflow, unsigned negative values, fractional values, and trailing characters. For example, `price_cents=18925` is accepted; `price_cents=189.25` is rejected. Prices display as `$189.25` in the menu but remain integer cents in CSV.

## 4. Record store

**Proposed file:** `RecordStore.h/.cpp`

**Input:** A validated `TradeRecord`, or an existing `RowId`.  
**Output:** The new row position, a record copy, or an invalid-position error.

The store uses `std::vector<TradeRecord>`. Appending a record returns its zero-based vector position. Records are append-only within an active database: no deletion, sorting, or movement to a different position is allowed. Vector reallocation changes addresses but preserves the row positions stored in the indexes.

```cpp
RowId append(const TradeRecord& record);
TradeRecord get(RowId row) const;
std::size_t size() const;
```

Only the coordinator calls `append`; users cannot bypass index maintenance. A full CSV reload replaces the store and both indexes together. Lookup returns a record copy so callers do not retain references invalidated by later insertions.

**Example:** Appending the first trade returns `RowId{0}`; both indexes then map that trade ID to `0`.

## 5. Hash table index

**Proposed file:** `HashIndex.h/.cpp`

**Input:** `insert(trade_id, row_position)` or `find(trade_id)`.  
**Output:** `Inserted` / `DuplicateKey`, or an optional row position.

### Representation and behavior

Use `std::vector<std::vector<Entry>>` for separate chaining, where each entry contains a key and row position. Start with 16 buckets. Apply a documented deterministic 64-bit integer mixing function, then compute `bucket = hash(key) % bucket_count`. Arithmetic in the hash function uses unsigned 64-bit values.

For insertion:

1. Search the appropriate bucket for the exact key.
2. If it exists, return `DuplicateKey` without replacing the stored row.
3. If adding the key would make the load factor exceed 0.75, double the bucket count and rehash every entry.
4. Insert the new mapping into its bucket and increment the entry count.

For lookup, hash the key and compare keys within the selected bucket. A collision means different keys share a bucket; it is not a duplicate. Rehashing changes bucket placement but never changes stored row positions.

**Example:** After `insert(1001, 0)`, `find(1001)` returns `0`; a second `insert(1001, 7)` returns `DuplicateKey`, and `find(1001)` still returns `0`.

**Expected cost:** Lookup is expected O(1) with well-distributed hashes and the controlled load factor; worst-case lookup is O(n). Insertion is expected amortized O(1), with O(n) work when a rehash occurs.

**Required checks:** Empty lookup, multiple colliding keys, duplicate rejection, and preservation of every mapping across several resizes.

## 6. B+ tree index

**Proposed file:** `BPlusTreeIndex.h/.cpp`

**Input:** The same insertion and exact-ID lookup arguments as the hash index.  
**Output:** The same statuses and optional row positions.

### Representation

Use a configurable maximum of `M` children per internal node; default `M=32`, with `M>=3`. Internal nodes hold at most `M-1` sorted separator keys and `M` owned children. Leaves hold at most `M-1` sorted `(key, row_position)` entries. Full trade records remain in the record store.

Each separator equals the smallest key in its right child subtree. When a search key equals a separator, descend into the child to its right; this can be implemented with `std::upper_bound`. Leaf lookup uses binary search. Leaves have a non-owning `next` link; ownership remains with the tree so leaf links do not create ownership cycles.

### Search and insertion behavior

1. Descend from the root to the appropriate leaf.
2. Search the leaf for the key. Reject duplicates without modifying the tree.
3. Insert the mapping into sorted position.
4. If the leaf now contains `M` entries, split it into left and right leaves, retaining the entries in the leaves. Copy the right leaf's smallest key into the parent as a separator and update the leaf links.
5. If an internal node overflows to `M+1` children, split its child list near the midpoint. Promote the separator between the two resulting child groups to the parent; that separator is removed from the split internal node's key lists.
6. Continue splitting upward as needed. If the root splits, create a new root with two children. An empty tree starts with a single empty leaf root.

All leaves must remain at the same depth. A non-root internal node has at least `ceil(M/2)` children; a non-root leaf has at least `ceil((M-1)/2)` entries. Internal separator counts always equal child counts minus one. These are checked in tests using a small `M` to force splits.

**Example:** With `M=4`, inserting keys `10, 20, 30, 40` into an empty tree splits the leaf into `[10,20]` and `[30,40]`, with root separator `30`. A lookup for `30` follows the right child.

**Expected cost:** For fixed node capacity, exact lookup and insertion take O(log n); an insertion may split nodes along the search path. Both indexes use O(n) space, with different constant overheads.

**Required checks:** Sorted and shuffled insertion, duplicate rejection, leaf splits, internal splits, root growth, separator equality cases, and agreement with known mappings after every split-heavy sequence.

## 7. Database coordinator

**Proposed file:** `Database.h/.cpp`

**Input:** Insert requests, lookup requests with an index choice, or a complete imported dataset.  
**Output:** A success/error result, a full record or not-found result, or an imported row count.

| Operation | Proposed behavior | Output |
|---|---|---|
| `insert(record)` | Validate, reject an existing ID, append once, and add its position to both indexes | Success and ID, or validation/duplicate error |
| `find(id, kind)` | Query the selected index; resolve its row position in the store | Record copy or not found |
| `replaceAll(records)` | Build a fresh store and both indexes; publish them together after success | Loaded count, or error with active database unchanged |
| `size()` | Return the number of stored records | Nonnegative integer |
| `save(path)` | Export the current store through the CSV writer | Saved count or file error |

### Consistency rules

- Store size, hash-index size, and B+ tree size must always agree after a completed operation.
- Every stored ID appears exactly once in each index and resolves to the correct row.
- Invalid records and duplicate requests change neither records nor mappings.
- Selecting an index affects lookup only. Every successful insertion updates both indexes.
- Duplicate prechecks prevent normal rejection midway through insertion. If an insertion fails part-way for an unexpected reason (for example an allocation failure), `Database::insert` rebuilds the store and both indexes from the previously committed records before rethrowing, so they never disagree. Recoverable errors are malformed input, duplicates, missing IDs, and file I/O failures.

This is application-level consistency within one process; the project does not implement transaction recovery or crash durability.

## 8. CSV reader and writer

**Proposed file:** `CsvIO.h/.cpp`

### File format

```csv
trade_id,symbol,price_cents,quantity,timestamp
1001,AAPL,18925,100,1793797200
1002,MSFT,42150,50,1793797260
```

Require the header names in this order, exactly five fields per data row, and one record per line. Trim surrounding ASCII whitespace in fields, accept LF and CRLF, and ignore blank lines. This deliberately restricted CSV format does not support quoting, embedded commas, or embedded newlines; the symbol rules make those features unnecessary.

### Load

**Input:** Readable file path.  
**Output:** Loaded record count on success, or an error containing the physical line number, field where applicable, and reason.

Read and validate all records, then build a temporary database using both indexes. A duplicate ID anywhere in the file rejects the entire load. Successful loading **replaces** the active dataset. Missing files, missing/incorrect headers, invalid rows, duplicates, or read failures leave the active database unchanged. A header-only file is a valid empty dataset; a completely empty file is invalid.

**Example error:** `Load failed at line 8: quantity must be greater than zero. Active database unchanged.`

### Save

**Input:** Destination path and current records.  
**Output:** CSV file and saved count, or file error.

Write the header and rows in record-store order. Write to a temporary sibling file, check stream errors and close success, then replace the destination using a supported rename/replace operation. Do not delete an existing destination as a fallback if replacement fails. Report success only when the destination replacement succeeds. This prevents ordinary partial-write failures from publishing a truncated CSV; crash durability is outside scope.

Saving does not modify the in-memory database. Loading a saved file must reproduce all field values and both lookup results, even if the index layouts differ after rebuilding.

## 9. Synthetic dataset generator

**Proposed file:** `DatasetGenerator.h/.cpp`

**Input:** Record count `n`, unsigned random seed, and order `sequential` or `shuffled`.  
**Output:** Vector of valid records and, when requested, a CSV file.

Generate IDs `1` through `n`, symbols from a fixed documented list, positive integer prices and quantities, and nonnegative timestamps. Use `std::mt19937_64` with seed `3530` by default. Document the field ranges and generation procedure. A shuffled dataset contains the same records as its sequential counterpart in a different order. Identical configuration and build environment reproduce the dataset; sharing the generated CSV guarantees identical input across environments.

Generate insertion-test records with IDs above the initial dataset range. The benchmark datasets use an ID stride of 2 (odd IDs), and missing-query IDs are drawn from the absent IDs interleaved with stored keys, so unsuccessful lookups reach every part of a tree rather than only its rightmost path. Datasets without gaps fall back to a disjoint range above all stored and inserted IDs. Every query result is verified against its expected outcome. Reject configurations whose ID ranges would overflow.

## 10. Benchmark runner

**Proposed file:** `Benchmark.h/.cpp`

**Input:** Validated records, fixed query lists, index configurations, seed, repeat count, and output path. Default to five measured trials after one warm-up trial.  
**Output:** A console comparison and a results CSV. Measurements are produced by running the implementation; this document proposes no performance numbers.

### Workloads

| Workload | Input and initial state | Timed work | Recorded result |
|---|---|---|---|
| Index build | Fresh empty index; `n` known `(ID, RowId)` pairs | Insert all pairs with normal resizing/splitting | Batch duration and average time per inserted entry |
| New-record insertion | The index built for this trial (after the lookup workloads); disjoint new IDs above all stored IDs | Insert `m=max(1,n/10)` new mappings | Batch duration and average time per insertion |
| Successful lookup | Prebuilt index; 100,000 fixed existing-ID queries | Call `find` for each query and consume result | Batch duration, average time per query, throughput |
| Unsuccessful lookup | Prebuilt index; 100,000 fixed absent-ID queries | Call `find` for each query and consume result | Same measures plus zero expected matches |

Each trial builds one index per implementation, times the build, runs both lookup workloads on that index, then times the incremental inserts; structure and mappings are verified afterward, outside timing. Repeat for all three dataset sizes and both insertion orders. Both implementations receive identical mappings and query sequences. The insertion benchmark measures index maintenance, not full-record parsing or database insertion into both indexes.

### Measurement rules

1. Generate/parse records and queries before timing. Prepopulate insertion-test indexes outside the timed region. Use fresh instances for every measured trial.
2. Use `std::chrono::steady_clock` in a Release build with the same compiler settings for both implementations. Exclude console output, file I/O, and correctness checks from timed sections.
3. Alternate which implementation runs first between trials. Keep bucket and node capacities fixed across the main comparison, record them, and include resizing/splitting in the relevant timed operation.
4. Consume lookup results with a checksum that encodes both found status and row position, and retain/report it after timing. Validate every result against known expected mappings outside timing; a matching checksum alone is not a correctness proof.
5. Report the median batch duration across trials. Derive average operation time as batch duration divided by operation count; this is not a measurement of individual-query latency or tail latency. Derive throughput from operations divided by batch seconds.
6. If any result is incorrect, mark the run invalid and do not present it as a successful performance comparison. Keep the user's active database unchanged.

Suggested results columns:

```text
dataset_size,order,seed,index,workload,trial,operation_count,
elapsed_ns,checksum,bucket_count,max_children
```

Store one raw row per trial and produce medians in the summary. Record the compiler, optimization settings, and machine description alongside the results. Use a Catch2/reference-map oracle only in correctness checks, never as either compared implementation. Memory profiling and parameter sweeps are optional and must be labeled separately.

## 11. Command-line interface

**Proposed file:** `main.cpp`

**Input:** Menu selection followed by prompted fields.  
**Output:** Readable results followed by the menu, unless the user exits.

| Choice | Required input | Proposed behavior and output |
|---|---|---|
| `1. Load CSV (replaces current records)` | Input path | Load atomically; print `Loaded N records.` or a line-specific error |
| `2. Insert record` | ID, symbol, price in cents, quantity, timestamp | Print `Inserted trade 1001.` or a validation/duplicate error |
| `3. Find trade by ID` | ID and `hash` / `bplus` | Display all five fields or `Trade 1001 not found.` |
| `4. Compare index performance` | Size(s), order(s), seed, results path; defaults available | Run isolated benchmarks; display medians and saved-results path |
| `5. Save CSV` | Destination path | Print `Saved N records.` or file error |
| `0. Exit` | None | Release owned memory and exit normally |

Read whole input lines so invalid numeric tokens cannot corrupt the input stream. An invalid menu selection or index name produces an error and re-prompts without changing data. An empty database permits lookup and save: lookup returns not found, and save writes a header-only file. Exit does not save automatically; end-of-input exits normally as well.

### Example interaction

```text
Choose an option: 2
Trade ID: 1001
Symbol: AAPL
Price in cents: 18925
Quantity: 100
Timestamp (Unix seconds): 1793797200
Inserted trade 1001.

Choose an option: 3
Trade ID: 1001
Index (hash/bplus): bplus
trade_id=1001 symbol=AAPL price=$189.25 quantity=100 timestamp=1793797200

Choose an option: 2
Trade ID: 1001
...
Insert failed: trade ID 1001 already exists.
```

## 12. Verification and integration criteria

| Area | Acceptance condition |
|---|---|
| Record validation | Reject zero price/quantity, negative unsigned input, overflow, unsupported symbols, and partially parsed numbers |
| Common index contract | Both indexes return the same expected positions, reject duplicates, and distinguish missing keys from row zero |
| Hash table | Collisions and multiple resizes preserve all mappings |
| B+ tree | All leaves have equal depth; separators, ordering, occupancy, and counts remain valid through root/internal/leaf splits |
| Database | Both indexes agree with all stored records after every successful insertion; rejected inserts preserve state |
| CSV load | Invalid rows and duplicate IDs reject the whole import and preserve the previous database |
| CSV round trip | Save then load reproduces every field and lookup result |
| Benchmark | Same workload for both indexes; fresh trial state; no timed parsing/output; no accepted results with incorrect mappings |
| Scale | All required operations complete correctly on at least 100,000 records, with comparison runs at 250,000 and 500,000 |

Suggested integration order: agree on shared types and `IIndex`; implement each index against the contract; connect the coordinator and CSV I/O; add the menu; then collect benchmark results and analyze them for the final presentation.

## 13. Proposed division of work

| Proposed owner | Deliverables | Integration boundary |
|---|---|---|
| Member 1 | Hash index, collision/rehash tests | Implements `IIndex`; supplies configuration/statistics for results |
| Member 2 | B+ tree index, structural/split tests | Implements the same `IIndex`; owns tree invariants |
| Member 3 | Record store, coordinator, CSV, generator, menu, benchmark harness | Uses the shared contract to integrate both indexes |

These are proposed roles, not confirmed assignments. All members review integration, validate benchmark methodology, analyze the measured results, and contribute to the final video. The third role covers more modules, so the team should redistribute integration and benchmark tasks to keep workloads balanced.

## 14. Optional extension: range queries

After the required operations pass, add `range(low, high)` with inclusive bounds on trade ID. Return matching records in ascending ID order; `low > high` produces an invalid-range error and no matches produces an empty result.

The B+ tree finds the first qualifying leaf entry and traverses linked leaves, taking O(log n + k) for fixed node capacity and `k` returned entries. A hash-table implementation scans all entries and sorts the matches if ordered output is required, taking O(n + k log k). Compare equivalent result ordering and bounds, and report this separately from exact-ID benchmarks. These are trade-ID ranges, not timestamp ranges; timestamp queries would require a different index or a scan.
