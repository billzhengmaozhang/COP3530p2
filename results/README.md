# Benchmark results

[submission-benchmark.csv](submission-benchmark.csv) contains the 240 measured
trials used for the report’s timing table. It includes 100,000, 250,000, and
500,000 records in sequential and shuffled insertion order, using seed 3530
and five measured trials per index and workload. The original run used an
optimized GCC 13.1.0 build on Windows.

To repeat the suite, use a Release build, choose option **4**, and accept the
[benchmark defaults](../README.md#run-the-benchmarks). New runs write to
`results/benchmark.csv` by default. Timings vary by machine and run.
