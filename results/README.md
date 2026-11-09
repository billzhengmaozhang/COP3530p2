# Benchmark results

[submission-benchmark.csv](submission-benchmark.csv) contains the 240 measured
trials used for the report’s timing table. It includes 100,000, 250,000, and
500,000 records in sequential and shuffled insertion order, using seed 3530
and five measured trials per index and workload. The run used a Release build
(Apple clang 21.0.0, `-O3 -DNDEBUG`) on an Apple M5 Mac running macOS. Stored
IDs are odd numbers, and missing-key lookups use even IDs between them. (The
first revision of this file came from a Windows/GCC 13.1.0 run in which
missing IDs all lay above the largest stored ID.)

To repeat the suite, use a Release build, choose option **4**, and accept the
[benchmark defaults](../README.md#run-the-benchmarks). New runs write to
`results/benchmark.csv` by default. Timings vary by machine and run.
