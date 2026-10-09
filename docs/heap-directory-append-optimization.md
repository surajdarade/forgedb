# Heap Directory Append Optimization

## Problem

Large insert workloads exposed two avoidable costs in `HeapFile`:

1. `dataPages()` returned `std::vector<PageId>` by value, and `HeapFile::insert` copied that vector on every insert even though the persisted heap's directory was already cached. As the heap grew, this made the append path perform repeated work proportional to the number of allocated pages.
2. `persistDataPages()` rewrote every page in the chained metadata directory whenever a new data page was allocated, even though appending one page normally changes only the current directory tail.

## Changes

- `dataPages()` returns a const reference to its cached page list for metadata-backed heaps. Legacy heaps without a metadata page refresh their temporary cache each call, preserving their previous discovery semantics.
- Inserts append to the cached data-page directory directly. If metadata persistence throws, the in-memory directory entry is rolled back.
- V2 directory persistence updates only the current tail metadata page on ordinary appends.
- When a new metadata overflow page is required, the new tail is written first and its predecessor is updated to link to it.
- Legacy V1 metadata is still read. Its first write performs a one-time full V1-to-V2 migration; subsequent appends use incremental persistence.
- Existing heap free-space index, B+ tree insertion cache, direct point lookup, and range-scan behavior are retained.

## Validation

- Release build succeeded.
- CTest: 5/5 tests passed, including dirty-page eviction, free-space index, metadata overflow/reopen and V1 migration, B+ tree insertion/reopen, and B+ tree range scans.
- Five trials each at 100,000 rows / 512 buffer pages and 500,000 rows / 2,048 buffer pages passed all benchmark validations.
- An additional 1,000,000-row / 4,096-page run passed: full scan 1,000,000/1,000,000; point lookups 10,000/10,000; range results 5,000/5,000; B+ tree entries 1,000,000/1,000,000.

## Linux benchmark medians

Five-run medians in one Linux test environment. These numbers should not be compared directly to Windows timings. The before/after comparison below isolates the incremental metadata-write change against the immediately preceding version, which already had the page-directory vector-copy removal.

| Rows / pool | Metric | Before incremental writes | After incremental writes |
|---|---|---:|---:|
| 100,000 / 512 | Table insert | 55.53 ms | 53.60 ms |
| 100,000 / 512 | Heap full scan | 103.33 ms | 109.85 ms |
| 100,000 / 512 | B+ tree insert | 122.11 ms | 130.57 ms |
| 100,000 / 512 | Point lookups | 73.40 ms | 79.87 ms |
| 100,000 / 512 | Range scans | 0.43 ms | 0.57 ms |
| 500,000 / 2,048 | Table insert | 558.46 ms | 384.08 ms |
| 500,000 / 2,048 | Heap full scan | 585.94 ms | 701.57 ms |
| 500,000 / 2,048 | B+ tree insert | 663.27 ms | 754.95 ms |
| 500,000 / 2,048 | Point lookups | 149.28 ms | 200.38 ms |
| 500,000 / 2,048 | Range scans | 0.74 ms | 0.82 ms |

At 500,000 rows, the table-insert median fell by approximately 31.2% versus the immediately preceding five-run set. Other operation timings varied and are not claimed as improvements or regressions caused by this change.

The earlier pre-optimization single-run 500,000-row baseline was 1,418.10 ms for table insertion; the new five-run median is 384.08 ms. This is a useful directional comparison, not a strictly matched repeated-run experiment.
