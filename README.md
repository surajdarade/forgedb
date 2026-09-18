# ForgeDB

ForgeDB is a C++20 embedded database engine built from first principles.

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
                            │
             ┌──────────────┼──────────────┐
             │              │              │
          Catalog      Query Engine    Transactions
             │              │              │
             │            Table           WAL
             │              │
             │          HeapFile
             │              │
             │          HeapPage
             │
             └──────────────┬──────────────┐
                            │              │
                    Persistent B+ Tree   Bitmap Index
                            │
                     BufferPoolManager
                            │
                       LRUReplacer
                            │
                        DiskManager
```

## Build

ForgeDB uses **CMake and C++20**.

### Visual Studio

```bash
cmake -S . -B build -G "Visual Studio 18 2026" -A x64
cmake --build build --config Release
```

### Run the Examples

```bash
.\build\Release\forgedb_basic_example.exe
```

```bash
.\build\Release\forgedb_persistent_index_example.exe
```

---

## Benchmark

ForgeDB includes a dependency-free benchmark executable covering core storage and indexing workloads.

The benchmark accepts the dataset size and buffer-pool capacity from the command line:

```bash
.\build\Release\forgedb_benchmark.exe <rows> <buffer_pool_pages>
```

### Examples

```bash
.\build\Release\forgedb_benchmark.exe 1000 64
```

```bash
.\build\Release\forgedb_benchmark.exe 5000 64
```

```bash
.\build\Release\forgedb_benchmark.exe 50000 256
```
