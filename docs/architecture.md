# ForgeDB Architecture

## Layers

### 1. Common
Strong identifiers and compile-time constants.

### 2. Storage
`DiskManager` owns the database file and 4 KiB pages. `HeapPage` implements a slotted-page record layout. `HeapFile` owns a table's page set and provides CRUD/scan operations.

### 3. Buffer pool
`BufferPoolManager` maps database page ids to memory frames. Pinned pages cannot be evicted. Dirty pages are written by flush/checkpoint. `LRUReplacer` selects unpinned victims.

### 4. Records/schema
`Schema`, `Column`, `Value`, `Tuple`, and `TupleSerializer` provide typed records and nullable fields.

### 5. Indexes
The index abstraction supports point lookup, duplicate entries and range scans. The in-memory B+ Tree provides algorithmic behavior. `PersistentBPlusTree` maps the same concept onto disk pages through the buffer pool. `BitmapIndex`, `UniqueIndex` and `CachedIndex` cover alternate indexing/decorator strategies.

### 6. Query
`Query` represents predicates and limits. `QueryEngine` evaluates predicates over table tuples. Indexed execution is exposed through the index interfaces and can be integrated into a planner without changing storage code.

### 7. Concurrency/transactions
`ReadWriteLock` provides writer-priority collection/catalog synchronization. `Transaction` tracks undo actions. `TransactionManager` assigns ids and emits WAL lifecycle records.

### 8. Database facade
`Database` owns the disk manager, buffer pool, catalog, tables, logging, write queue and transaction manager. Catalog metadata lives on page 0; each table gets a metadata page describing its heap pages.
