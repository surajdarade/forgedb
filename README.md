# ForgeDB

ForgeDB is a C++20 embedded database engine built from first principles.

It is designed as a compact, disk-backed relational storage engine with explicit control over pages, buffering, records, indexing, querying, persistence, concurrency, and transaction infrastructure.

## Implemented Engine

### Storage

- Fixed-size 4 KiB pages.
- File-backed `DiskManager` with page allocation and flush semantics.
- Slotted `HeapPage` storage with stable `RecordId`s.
- Heap-file page metadata for table-local page ownership.
- Variable-length tuple records.
- Record deletion.
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
- Tuple serialization and deserialization.
- Stable record identifiers.
- Schema validation.

### Indexing

#### B+ Tree

- In-memory multi-level B+ Tree.
- Duplicate-key support.
- Point lookup.
- Inclusive range scans.
- Linked leaf pages for sequential scans.
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

### Query Layer

- Predicate-based queries.
- Equality and inequality comparisons:
  - `EQ`
  - `NE`
  - `LT`
  - `LTE`
  - `GT`
  - `GTE`
- NULL-aware equality handling.
- Query limits.
- Table scans.

### Concurrency and Transactions

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
        │              │             WAL
        │              │
        │            Table
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
