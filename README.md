# MiniDB — COP3530 Project 2

A proposed C++17 in-memory trade database comparing a **custom hash table** with
a **custom B+ tree** for insertion and exact trade-ID lookup.

**Current status: repository scaffold.** Headers contain function declarations;
matching `.cpp` files contain TODO comments. Database behavior, the menu, and
benchmarks still need implementation. No working application or measured
performance is claimed.

## File structure

| Path | Contents |
|---|---|
| `include/minidb/` | 12 headers: shared types, errors, validation, indexes, storage, database, CSV, generator, benchmark, CLI |
| `src/` | Matching implementation placeholders plus `main.cpp` |
| `tests/` | Test plan; future Catch2 tests |
| `data/` | Small synthetic sample CSV; future generated datasets |
| `results/` | Future benchmark output |
| `docs/MiniDB_Design.md` | Component behavior, inputs, outputs, and acceptance criteria |
| `docs/Header_Reference.md` | Function declarations and proposed implementation mapping |
| `CMakeLists.txt` | C++17 scaffold compilation target |
| `.gitignore` | Build, IDE, generated data, and result exclusions |
| `.gitattributes` | Cross-platform text handling |

## Build the scaffold

Requirements: a C++17 compiler and CMake 3.16 or newer.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

This compiles each header independently and the placeholder `.cpp` files into
objects. It does not link an executable or test unimplemented functions. After
implementing the modules, add a library target, an executable using `main.cpp`,
and Catch2/CTest targets.

## Implementation plan

1. Agree on shared types and the `IIndex` contract.
2. Implement and test the hash table and B+ tree independently.
3. Connect the record store, coordinator, and CSV I/O.
4. Implement dataset generation and the menu.
5. Benchmark 100,000, 250,000, and 500,000 records using identical workloads.

Full records live in one append-only vector; both indexes map trade IDs to row
positions. Every successful database insertion updates both indexes. The initial
scope excludes deletion, updates, SQL parsing, transactions, concurrency, and
disk-page management.

## Upload this ZIP to GitHub

Extract the ZIP, then open a terminal **inside the extracted `COP3530p2` folder**.
Create an empty GitHub repository named `COP3530p2` without adding a README,
license, or gitignore. Choose its visibility when creating it.

Run these commands, replacing `YOUR_USERNAME` with your GitHub username:

```sh
git init -b main
git add .
git commit -m "Add MiniDB project scaffold"
git remote add origin https://github.com/YOUR_USERNAME/COP3530p2.git
git push -u origin main
```

Authenticate using your usual GitHub sign-in or credential manager. If Git asks
for your author name/email, configure your own Git identity and retry the commit.
The source ZIP excludes local Git history and build artifacts, so these commands
start your own history. The local prepared copy already has an initial commit.

Alternatively, use GitHub's **Add file → Upload files** to upload the extracted
folder's contents. Upload the files themselves, not the ZIP, and ensure hidden
configuration files are included.

## Validation performed

The headers and placeholder `.cpp` files were compiled independently with GCC in
C++17 mode. The source archive was checked against the committed file list.
CMake configuration was provided but was not executed in the preparation
environment because CMake was unavailable. Behavioral tests and runtime
benchmarks await implementation.
