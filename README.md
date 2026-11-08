# MiniDB | COP3530 Project 2

MiniDB is a C++17 command-line database for synthetic stock trades. It compares
a custom separate-chaining hash table with a custom B+ tree for building an
index, inserting trades, and looking up exact trade IDs. The database stores
each record once and updates both indexes on insertion.

## Build and run

Use a C++17 compiler and CMake 3.16 or newer. On Windows, open a terminal where
your compiler is on the PATH, such as a Visual Studio Developer PowerShell or a
MinGW terminal. Run these commands from the repository root.

~~~sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
cmake --build build --config Release
~~~

Launch the program from the repository root so the sample data and default
benchmark output path resolve correctly.

| Build | Run |
| --- | --- |
| Linux or macOS, Makefiles or Ninja | `./build/minidb_cli` |
| Windows PowerShell, Visual Studio | `.\build\Release\minidb_cli.exe` |
| Windows PowerShell, Ninja or MinGW Makefiles | `.\build\minidb_cli.exe` |

## Try the CLI

1. Choose **1** and enter **data/sample_trades.csv** to load the sample.
2. Choose **3**, enter **1001**, and select **hash**. Repeat with **bplus** to
   retrieve the same trade through the B+ tree.
3. Choose **2** to insert a trade. The prompts accept a trade ID, symbol,
   integer price in cents, quantity, and Unix timestamp in seconds.
4. Choose **5** and enter a writable path such as **results/trades.csv** to
   export the active records.
5. Choose **0** to exit.

Trade CSVs use this header and field order.

~~~csv
trade_id,symbol,price_cents,quantity,timestamp
1001,AAPL,18925,100,1793797200
~~~

Use one trade per line and unquoted fields. Duplicate IDs and invalid rows
are rejected. A successful import replaces the active dataset.

## Run the benchmarks

Choose **4** and press Enter at all four prompts for the default suite. The
program compares both indexes on 100,000, 250,000, and 500,000 generated rows in
sequential and shuffled order, with seed 3530. Use a Release build when
comparing timings. For a quick check, enter **1000** at the dataset-size prompt
and accept the other defaults.

The console shows median times for build, incremental insert, successful
lookup, and missing lookup. The CSV at **results/benchmark.csv** contains
individual measured trials. The menu also accepts a different output path
whose parent directory already exists. The [archived benchmark results](results/README.md)
include the raw trials used in the report.

## Run the tests

Configuring with tests enabled fetches Catch2 v3.5.4 from GitHub, which requires
Git and network access on the first configuration.

~~~sh
cmake -S . -B build-tests -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build-tests --config Release
ctest --test-dir build-tests -C Release --output-on-failure
~~~

