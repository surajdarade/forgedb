# Reference Scope

ForgeDB was scoped against the public roadmap of `sepgh/testudo`.

Covered concepts:

- B+ Tree indexes
- Cluster/persistent index concepts
- Disk/page storage
- Buffer pages and LRU caching
- Nullable serialization
- Primitive serialization
- Unique and non-unique index behavior
- Bitmap indexing with sparse 64-bit record/page addressing
- Query predicates
- Select/insert/update/delete table operations
- Reader/writer locking
- Transaction lifecycle and WAL records
- Async write queue
- Exception-safe page handling
- Logging
- Graceful shutdown/checkpoint
- Binary-search based lookup paths

The reference repository's Java source is not copied into ForgeDB. The C++ implementation uses independent types, page layouts, ownership rules and APIs.

The reference project's documented open problems around non-indexed queries, large bitmap addressing and shared database/index storage were used as design inputs. ForgeDB addresses these with table-local scans, sparse 64-bit bitmap addressing, and a single database-owned buffer/storage stack.
