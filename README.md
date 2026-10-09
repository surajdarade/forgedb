# ForgeDB

ForgeDB is a C++ 20 embedded database engine built from first principles.

It provides a disk-backed storage engine with paged storage, buffer-pool management, heap files, persistent indexing, predicate-based queries, transaction infrastructure, concurrency control, logging, and database catalog persistence.

## Features

### Storage

- Fixed-size 4 KiB pages.
- File-backed `DiskManager` with page allocation and flush semantics.
- Slotted `HeapPage` storage with stable `RecordId`s.
- Heap-file page metadata for table-local page ownership.
- Variable-length tuple records.
- Record insertion, retrieval, update, and deletion.
- Page compaction during larger updates.
- Little-endian binary serialization for primitive values and strings.

### Buffering and Caching

- `BufferPoolManager` with pin/unpin semantics.
- LRU page replacement.
- Dirty-page tracking.
- Page eviction and flushing.
- Asynchronous `WriteQueue` for background write work.

### Schema and Records

Supported data types:

- `Boolean`
- `Int32`
- `Int64`
- `Float`
- `Double`
- `Varchar`

Additional capabilities:

- Nullable values.
- Typed schemas and columns.
- Schema validation.
- Tuple serialization and deserialization.
- Stable record identifiers.

### Indexing

#### B+ Tree

- In-memory multi-level B+ Tree.
- Duplicate-key support.
- Point lookup.
- Inclusive range scans.
- Linked leaf nodes for sequential scans.
- Leaf-node splitting.
- Internal-node splitting.
- Deletion with redistribution.
- Node merging.
- Root contraction.

#### Persistent B+ Tree

- Disk-backed B+ Tree pages.
- Persistent tree metadata.
- Persistent entry count.
- Persistent insert.
- Persistent point lookup.
- Persistent range scan.
- Persistent deletion and rebalancing.

#### Additional Indexes

- Sparse 64-bit bitmap index.
- Unique-index decorator.
- LRU cached-index decorator.

### Query Engine

ForgeDB provides a programmatic predicate-based query API.

Supported comparison operators:

- `EQ`
- `NE`
- `LT`
- `LTE`
- `GT`
- `GTE`

Additional capabilities:

- Multiple predicates.
- NULL-aware equality handling.
- Query limits.
- Table scans.
- Database-level query execution.

ForgeDB does not currently include a SQL parser or SQL statement language.

### Record Operations

The table layer supports record-level CRUD operations:

- Insert tuples.
- Read records by `RecordId`.
- Scan tables.
- Update records.
- Delete records.

These are exposed through the C++ API rather than SQL statements.

### Transactions and Concurrency

- Writer-priority reader/writer lock.
- RAII shared and exclusive lock guards.
- Transaction lifecycle:
  - Begin
  - Commit
  - Abort
- Undo actions for application-level rollback.
- Append-only write-ahead log records.

### Database and Catalog

- Database open/close/checkpoint lifecycle.
- Persistent catalog stored on page 0.
- Persistent table schemas.
- Separate heap metadata for each table.
- Create/open/drop table operations.
- Database-level query execution.
- Logging.
- Graceful shutdown.

## Architecture

```text
                         Database
                            |
             +--------------+--------------+
             |              |              |
          Catalog      Query Engine   Transactions
             |              |              |
             |            Table            WAL
             |              |
             |           HeapFile
             |              |
             |           HeapPage
             |
             +--------------+--------------+
                            |              |
                    Persistent B+ Tree  Bitmap Index
                            |
                    BufferPoolManager
                            |
                       LRUReplacer
                            |
                        DiskManager
```

## Build

ForgeDB uses **CMake and C++20**.

### Visual Studio

```powershell
cmake -S . -B build -G "Visual Studio 18 2026" -A x64
cmake --build build --config Release
```

### Run the Examples

```powershell
.\build\Release\forgedb_basic_example.exe
```

```powershell
.\build\Release\forgedb_persistent_index_example.exe
```

## Benchmark

ForgeDB includes a dependency-free benchmark executable covering core storage and indexing workloads.

The benchmark accepts the dataset size and buffer-pool capacity from the command line:

```powershell
.\build\Release\forgedb_benchmark.exe <rows> <buffer_pool_pages>
```

### Examples

```powershell
.\build\Release\forgedb_benchmark.exe 1000 64
```

```powershell
.\build\Release\forgedb_benchmark.exe 5000 64
```

```powershell
.\build\Release\forgedb_benchmark.exe 50000 256
```

### Benchmark Results

Benchmark data and experiment artifacts are tracked in [`benchmark-results/`](benchmark-results/). The directory includes raw trial directories, CSV comparisons, median summaries, and experiment-specific reports. Consult the relevant CSV/report for each workload; results from different row counts, buffer-pool capacities, machines, and optimization checkpoints should not be treated as one directly comparable experiment.

#### Final low-level optimization pass: one million rows

The following results compare five-trial medians from a Linux x86-64 Release build before and after a combined patch that cached disk-file length metadata, amortized free-frame selection, and batched file-stream flushing.

**Workload:** 1,000,000 rows, 4,096 buffer-pool pages, 10,000 point lookups, 50 range scans of 100 keys each, seed 42.

| Operation | Before (median) | After (median) | Elapsed-time reduction |
|---|---:|---:|---:|
| Table insertion | 706.40 ms | 504.70 ms | 28.6% |
| Heap full scan | 537.06 ms | 238.35 ms | 55.6% |
| B+ tree insertion | 928.15 ms | 774.55 ms | 16.5% |
| B+ tree point lookup | 248.81 ms | 92.54 ms | 62.8% |
| B+ tree range scan | 1.43 ms | 0.57 ms | 60.1% |

All recorded runs passed the benchmark's row-count, lookup, range-result, and index-entry validation checks. These figures are from the Linux test environment, not a Windows benchmark. The patch was measured as a combined change; the table does not attribute the gains to any one change in isolation.

#### Additional targeted optimization experiments

Separate same-environment Linux experiments measured the following results:

| Optimization experiment | Workload | Before | After | Measured change |
|---|---|---:|---:|---:|
| Heap free-space index | Table insertion; 50,000 rows, 256-page pool | 540.50 ms | 42.28 ms | 92.2% less time |
| Persistent B+ tree insertion fast path | B+ tree insertion; 50,000 rows, 256-page pool | 1,570.23 ms | 80.28 ms | About 19.6x faster |
| B+ tree serialized range scan | Range scans; 50,000 rows, 256-page pool | 22.16 ms | 0.48 ms | About 46.2x faster |
| B+ tree point lookup | Point lookups; 50,000 rows, 256-page pool | 165.98 ms | 81.95 ms | 50.6% less time |

These are separate experiments and must not be combined with the one-million-row table as if they were a single end-to-end before/after run. The insertion, range-scan, and point-lookup results are workload-specific; consult the corresponding report and CSV for trial details.

#### Reproducing and interpreting results

- Build in **Release** mode for performance measurements.
- Run baseline and optimized versions with the same row count, buffer-pool size, compiler configuration, and machine when comparing them.
- Use repeated trials and compare medians rather than relying on one run.
- Run each benchmark in a fresh working directory if the benchmark uses fixed database, WAL, or index filenames.
- Preserve raw trial output alongside summary CSVs so the reported medians can be checked.
- Benchmark timings are environment-dependent. These measurements do not by themselves establish production performance, concurrent scalability, or crash-recovery guarantees.

## Current Scope

ForgeDB exposes a C++ API and does not currently provide SQL parsing. Its transaction, logging, and concurrency components should be understood according to the behavior implemented and covered by the project's tests; the feature list alone is not a claim of full ACID compliance or production readiness.
