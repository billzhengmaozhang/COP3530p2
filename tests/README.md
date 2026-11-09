# Tests

The Catch2 suite covers:

- **B+ tree:** configuration validation, empty lookups, row zero, duplicate keys,
  leaf splits and separator lookups, cascading internal splits, root growth, and
  mappings after sequential and shuffled inserts.
- **Hash index:** configuration validation, growth at the load-factor
  boundary, a single-bucket chain, row zero, duplicates, and extreme keys.
- **Both indexes:** randomized differential tests against a reference `std::map`
  for several B+ tree node sizes.
- **Record validation, CSV I/O, and the database:** field and symbol rules,
  overflow, CRLF/blank lines/BOM, error line numbers, atomic save, load replacement
  and rollback, duplicate-ID line reporting, and save/load round trips.
- **Dataset generator, benchmark, and CLI:** seed determinism, ID strides and
  range errors, verified benchmark trials, CSV output, and scripted menu sessions.

Use the [build and CTest commands](../README.md#run-the-tests) from the
repository root. The first configure fetches Catch2 v3.5.4 from GitHub unless Catch2 v3 is installed.
