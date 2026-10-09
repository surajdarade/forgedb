# ForgeDB Benchmark Follow-up — Heap Insertion Scaling

Date: 2026-10-09

## Summary

The user's Windows Release build passed the registered regression test and completed five repetitions each for 5,000 rows / 64 pages, 10,000 rows / 256 pages, and 50,000 rows / 256 pages. All printed validation checks passed in all 15 runs.

The Windows medians supplied by the user were:

| Operation | 5,000 / 64 | 10,000 / 256 | 50,000 / 256 |
|---|---:|---:|---:|
| Table insert | 11.11 ms | 31.33 ms | 3,315.82 ms |
| Heap full scan | 110.81 ms | 223.24 ms | 1,099.42 ms |
| B+ tree insert | 432.89 ms | 913.28 ms | 4,698.77 ms |
| Point lookup | 52.23 ms | 120.72 ms | 438.13 ms |
| Range scan | 19.93 ms | 20.76 ms | 21.99 ms |

These Windows measurements are user-provided and were collected on a different machine and operating system from the Linux measurements below. They must not be compared directly as a before/after pair.

## Root cause from source inspection

`HeapFile::insert` first tries the newest page. If it is full, the previous implementation walked older pages in reverse order, fetching each one and attempting an insert until it found space. For append-heavy fixed-size rows, older pages were usually full, so each new insert repeatedly performed unsuccessful fetch/insert attempts across many pages. This explains the sharp insertion scaling degradation at 50,000 rows.

The earlier code also returned a copy of the page-directory vector on each insert. That copy is still present; the new change removes the much more expensive repeated page-fetch/overflow path for pages known not to have sufficient space.

## Follow-up implementation

- Add an in-memory free-space index keyed by each heap page's available byte count.
- Initialize the index from existing data pages on first insert after opening the heap file.
- Continue trying the newest page first for append-heavy workloads.
- If the newest page cannot fit a row, use `lower_bound(record bytes + slot bytes)` to find an older page that can fit it, rather than trying all older pages.
- Update the index after inserts and record updates; keep it in memory only, so the persisted on-disk format does not change.
- Keep the change isolated to the extracted working copy; the original uploaded archive remains unchanged.

## Same-environment Linux comparison

Release builds of the package before this follow-up and the follow-up working copy were benchmarked in the same Linux container, with five trials for each workload and configuration. Medians:

| Rows / pool | Metric | Before follow-up | After follow-up | Change |
|---|---|---:|---:|---:|
| 5,000 / 64 | Table insert | 4.50 ms | 2.46 ms | 45.3% less time |
| 10,000 / 256 | Table insert | 14.50 ms | 4.92 ms | 66.1% less time |
| 50,000 / 256 | Table insert | 540.50 ms | 42.28 ms | 92.2% less time |

All five trials in each comparison exited successfully. Timing variance on the shared container was noticeable, so these results should be treated as strong evidence of improvement in the table-insertion path, not as a full characterization of every operation. Other operation medians varied between builds and are included in the raw benchmark logs rather than claimed as regressions or improvements.

The updated 50,000-row / 64-page run also completed and passed all benchmark validation checks. A second CTest regression exercises variable-size inserts, a record update that fits, and close/reopen validation.

## Validation performed

- Release CMake build: passed.
- CTest: 2/2 passed (`dirty_page_eviction_persistence`, `heap_free_space_index_variable_rows`).
- 5,000 / 64: five before and five after runs passed validation.
- 10,000 / 256: five before and five after runs passed validation.
- 50,000 / 256: five before and five after runs passed validation.
- 50,000 / 64: one follow-up run passed validation.

The benchmark validates full table scan row counts, deterministic point lookup hits, sampled range-result counts, and B+ tree entry counts. It does not replace broader CRUD, variable-size record, crash-recovery, concurrency, or p95/p99 latency tests.

## Next validation on Windows

Build the updated source in Release mode, run CTest with `-C Release`, then repeat the 5,000 / 64, 10,000 / 256, and 50,000 / 256 workloads in fresh working directories. Compare Windows results only with Windows results from the same machine and build configuration.
