# ForgeDB

ForgeDB is a C++20 embedded database engine built from first principles. It is an independent C++ implementation inspired by the engineering topics covered by the `sepgh/testudo` database-from-scratch project; it does **not** copy the Java implementation source.

## Implemented engine

### Storage
- Fixed-size 4 KiB pages.
- File-backed `DiskManager` with page allocation and flush semantics.
- Slotted `HeapPage` storage with stable `RecordId`s.
- Heap-file page metadata for table-local page ownership.
- Variable-length tuple records.
- Record deletion and page compaction during larger updates.
- Little-endian binary serialization for primitive values and strings.

### Buffering and caching
- `BufferPoolManager` with pin/unpin semantics.
- LRU replacement.
- Dirty-page tracking.
- Page eviction and flushing.
- Asynchronous `WriteQueue` for background write work.

### Schema and records
- `Boolean`, `Int32`, `Int64`, `Float`, `Double`, `Varchar`.
- Nullable values.
- Schemas and typed columns.
- Tuple serialization/deserialization.
- Stable record identifiers.

### Indexing
- In-memory multi-level B+ Tree.
- Duplicate-key support.
- Point lookup and inclusive range scans.
- Leaf links for sequential scans.
- Leaf/internal split logic.
- Deletion with redistribution, merging and root contraction.
- Disk-backed persistent B+ Tree pages.
- Persistent tree metadata and entry count.
- Persistent insert, lookup, range scan and delete/rebalancing.
- Sparse 64-bit bitmap index.
- Unique-index decorator.
- LRU cached-index decorator.

### Query layer
- Predicate queries.
- `EQ`, `NE`, `LT`, `LTE`, `GT`, `GTE` comparisons.
- NULL equality handling.
- Query limits.
- Table scans.

### Concurrency and transactions
- Writer-priority reader/writer lock.
- RAII shared/exclusive guards.
- Transaction lifecycle: begin/commit/abort.
- Undo actions for application-level rollback.
- Append-only write-ahead log records.

### Database/catalog
- Database open/close/checkpoint lifecycle.
- Persistent catalog on page 0.
- Persistent table schemas.
- Separate heap metadata for each table.
- Create/open/drop table operations.
- Query execution through the database facade.
- Logging and graceful shutdown.

## Architecture

```text
Database
  ├── Catalog
  ├── Query Engine
  ├── Transaction Manager / WAL
  ├── Table
  │    └── HeapFile
  │         └── HeapPage
  ├── Persistent B+ Tree / Bitmap Index
  └── BufferPoolManager
       └── LRUReplacer
            └── DiskManager
```

The core dependency direction is intentionally kept downward:

```text
Database → Query / Transaction / Index → Record / Buffer → Storage
```

## Build

ForgeDB uses CMake and C++20.

```powershell
cmake -S . -B build -G "Visual Studio 18 2026" -A x64
cmake --build build --config Debug
```

To build without tests:

```powershell
cmake -S . -B build -DFORGEDB_BUILD_TESTS=OFF
```

## Example

```cpp
#include "forgedb/database/database.h"

using namespace forgedb;

int main() {
    auto db = Database::open("example.db");

    auto& users = db->createTable(
        "users",
        Schema{{
            Column{"id", DataType::Int32},
            Column{"name", DataType::Varchar, 100}
        }}
    );

    users.insert(Tuple{{
        Value{std::int32_t{1}},
        Value{"Suraj"}
    }});

    Query query;
    query.where({
        0,
        ComparisonOperator::Equal,
        Value{std::int32_t{1}}
    });

    const auto rows = db->select("users", query);
    db->close();
}
```

## Design principles

- C++20.
- RAII and explicit ownership.
- Strong page/record identifiers.
- Persistent state separated from execution logic.
- Correctness before micro-optimization.
- Tests for storage, records, indexes and database behavior.
- No Java-to-C++ line-by-line translation of the reference project.

## Reference project

The functional scope was guided by the public development roadmap of `sepgh/testudo`, including B+ Trees, bitmap indexes, page storage, buffer/LRU caching, serialization, nullable values, query operations, CRUD, reader-writer locking, transactions, async writes, logging and performance improvements.
