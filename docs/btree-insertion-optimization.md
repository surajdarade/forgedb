# Persistent B+ Tree Insertion Optimization

Date: 2026-10-09

## Problem

The benchmark's B+ tree workload uses `PersistentBPlusTree`. Before this change, every insertion traversed from the root to a leaf, materialized all leaf entries into a vector, found the insertion position, inserted into the vector, and rewrote the serialized page. The benchmark inserts monotonically increasing integer keys, so this repeated work was unnecessary for the common append-to-rightmost-leaf case.

## Changes

- Added a serialized-tail scan to `BPlusTreeLeafPage` that locates the last entry and payload end without constructing a vector of all entries.
- Added an in-place ordered append path that serializes only the new entry and increments the page entry count. Out-of-order inserts still use the existing general insertion path.
- Added a cached rightmost leaf page and maximum key to `PersistentBPlusTree`; monotonically increasing keys can bypass repeated root-to-leaf traversal.
- The cache is updated when the rightmost leaf splits and invalidated after successful deletes, since rebalancing may move or remove the former rightmost leaf.
- Added a regression test covering 50,000 ascending integer keys, leaf/internal splits, buffer-pool eviction with 16 frames, exact duplicate rejection, out-of-order inserts, deletion/reinsertion of the maximum key, varchar append handling, and close/reopen verification.

## Verification

Release build succeeded. CTest passed 3/3:

- `dirty_page_eviction_persistence`
- `heap_free_space_index_variable_rows`
- `persistent_b_plus_tree_insert_reopen`

Five 50,000-row / 256-page benchmark trials all passed full-scan, point-lookup, range-scan, and B+ tree entry-count validation.

| Operation | Five-run median |
|---|---:|
| Table insert | 60.57 ms |
| Heap full scan | 73.20 ms |
| B+ tree insert | 80.28 ms |
| B+ tree point lookup | 258.08 ms |
| B+ tree range scan | 25.87 ms |

The earlier same-environment 50,000-row B+ tree insertion median was 1,570.23 ms across three trials. The new five-trial median of 80.28 ms is about 19.6x faster (94.9% lower elapsed time). This is a useful benchmark signal, not a universal production guarantee; the trial environment showed substantial timing variation for scans and lookups.

## Caveats

- The rightmost-leaf cache is in-memory state only and is reconstructed lazily after opening an existing tree.
- This is an insertion-path optimization, not a change to the persistent page format or metadata format.
- Validate the updated build on the target Windows machine using the same 50,000-row / 256-page workload before making Windows-specific performance claims.

## Follow-up: Point Lookup Optimization

Date: 2026-10-09

### Problem

`BPlusTreeLeafPage::lookup` previously called `readEntries()`, allocating and deserializing every entry in a leaf before performing a binary search. For point queries this paid the allocation and full-page decode cost even when the requested key appeared early in the serialized entry stream. The persistent-tree lookup also decoded the entire leaf again via `keyAt(size - 1)` to decide whether it should continue to the next leaf.

### Changes

- Point lookup now scans the serialized leaf payload directly and decodes entries one at a time, avoiding the temporary vector of every leaf entry.
- The scan stops as soon as it sees a key greater than the requested key.
- The leaf lookup reports whether a later leaf could still contain the requested key, preserving duplicate-key retrieval when duplicates span leaf splits and avoiding redundant sibling-page visits after a greater key has been found.
- Regression coverage now includes missing keys below, between, and above populated integer ranges, plus a missing variable-length string key after reopening the persistent tree.

### Verification and measurements

Release build succeeded and CTest passed 3/3. Five 50,000-row / 256-page benchmark trials all passed row counts, point-lookup hits, range-scan counts, and B+ tree entry-count validation.

Same Linux environment, same benchmark executable configuration, five trials before and after the direct serialized lookup change:

| Operation | Before median | After median | Change |
|---|---:|---:|---:|
| Table insert | 37.59 ms | 36.32 ms | 3.4% faster |
| Heap full scan | 49.55 ms | 54.05 ms | 9.1% slower |
| B+ tree insert | 61.95 ms | 67.08 ms | 8.3% slower |
| B+ tree point lookup | 165.98 ms | 81.95 ms | 50.6% faster (2.03x) |
| B+ tree range scan | 20.96 ms | 22.93 ms | 9.4% slower |

The lookup improvement is the primary intended result. Other-operation differences are small enough to be affected by timing variance, especially the slower fifth after-change trial; they should be monitored rather than attributed to this change. No persistent page format or metadata format changed.
