# Planned correctness tests

Add Catch2 tests as each component is implemented. Catch2 is not downloaded or
configured by this scaffold. Proposed files and meaningful checks:

| File | Checks |
|---|---|
| RecordValidatorTests.cpp | Invalid values, overflow, complete numeric parsing |
| HashIndexTests.cpp | Collisions, duplicates, repeated resizing |
| BPlusTreeIndexTests.cpp | Leaf/internal/root splits, ordering, occupancy, equal leaf depth |
| DatabaseTests.cpp | Both indexes agree, duplicate rejection preserves state |
| CsvIOTests.cpp | Failed load preserves data, duplicate line diagnostics, save/load round trip |
| BenchmarkTests.cpp | Identical workloads and independent trial state |

The current build checks compilation of declarations, not database behavior.
