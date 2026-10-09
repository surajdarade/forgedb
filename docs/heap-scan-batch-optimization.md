# Heap scan batching optimization

## Problem

`Table::scan()` previously called `HeapFile::scan()` to obtain every live `RecordId`, then called `Table::get()` for each ID. Each `get()` fetched and unpinned the same heap page again and copied the record bytes into a new vector before deserializing the tuple. For large tables, this caused repeated buffer-pool lookups and record-copy overhead during a sequential scan.

## Change

- Added `HeapPage::readView(slot)`, which exposes a bounds-checked `std::span<const Byte>` over a live record's serialized bytes without allocating a temporary byte vector.
- Added `HeapFile::forEachRecord(visitor)`, which fetches each heap page once, visits its live records while the page is pinned, and unpins the page after all records on that page have been consumed.
- Changed `Table::scan()` to deserialize each tuple directly from the pinned-page view, set its `RecordId`, and preserve schema validation.
- The existing `HeapFile::scan()` API remains available for callers that only need record IDs.
- Added a regression test for deleted rows, row payloads, uniqueness, and close/reopen behavior.

The record view is intentionally valid only during the visitor call; callers must not retain the span after the callback returns.

## Correctness

Release build succeeded. All six CTest tests passed, including the new `heap_batched_scan_deleted_rows_reopen` test. The 1,000,000-row benchmark passed its row-count, point-lookup, range-scan, and index-entry validation checks.

## Linux benchmark comparison

Five independent trials were run per version and workload using the same extracted source baseline, compiler configuration, and machine. Each trial used a fresh working directory. Timings are medians; this is a single-machine microbenchmark and should not be compared directly with Windows timings.

| Rows | Operation | Before median | After median | Change |
|---:|---|---:|---:|---:|
| 100,000 | Heap full scan | 107.48 ms | 30.38 ms | 71.7% lower |
| 500,000 | Heap full scan | 690.15 ms | 203.62 ms | 70.5% lower |

Insertion and B+ tree timings varied modestly between versions; the change is specifically intended to reduce the repeated fetch/copy work in table scans. No broad performance claim is made for unrelated operations.
