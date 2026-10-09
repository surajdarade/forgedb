# ForgeDB Performance Audit and Baseline/Verification Report

Date: 2026-10-09

## Scope and safety

The original uploaded archive was left unchanged. Work was performed on an extracted working copy. A pre-change checkpoint was preserved separately during the audit. This report distinguishes measurements from source-code findings and hypotheses.

## Environment

- Linux x86-64, kernel 6.18.44
- Intel Xeon Platinum 8370C, 2.80 GHz; four-CPU cgroup quota, five logical CPUs visible
- 4 GiB cgroup memory limit
- GCC 14.2.0, CMake 3.31.6, C++20, Release build
- Overlay filesystem
- Benchmark seed 42; lookup count `min(rows, 10000)`; up to 50 range scans of 100 keys

## Baseline before changes

Five trials were run at 5,000 rows and a 64-page buffer pool. Median timings:

| Operation | Baseline median |
|---|---:|
| Table insert | 226.56 ms |
| Heap full scan | 5.33 ms |
| B+ tree insert | 148.48 ms |
| B+ tree point lookup | 51.62 ms |
| B+ tree range scan | 22.41 ms |

At 6,500 and 7,000 rows with a 64-page pool, full-scan validation failed. Larger 50,000-row runs did not complete within the original timeout.

## Root causes found

### 1. Dirty pages were discarded on buffer-frame eviction

`BufferPoolManager::fetchPage` and `newPage` selected an evictable frame and removed its old page-table entry without first writing a dirty victim to disk. Reusing the frame therefore discarded modifications. This explains why the row-count check failed when the working set exceeded the buffer pool. It also undermined the reliability of benchmark results at larger workloads.

### 2. Heap insertion repeatedly searched the entire page directory

Every `HeapFile::insert` reloaded the persisted list of data pages and tried pages from the beginning. For append-heavy workloads, most earlier pages were already full, so successful inserts repeatedly revisited them. This caused unnecessary directory decoding and page fetches, with work growing sharply as the number of pages increased.

### 3. The suspected B+ tree lookup hang was not confirmed

Initial redirected benchmark output was buffered, so the timed-out 50,000-row run did not clearly expose the active phase. After the dirty-eviction fix alone, the 50,000-row run still spent excessive time in table insertion. After the insertion-path improvement, the 50,000-row benchmark completed and validated all 10,000 point lookups. The earlier lookup-hang diagnosis should therefore be treated as unconfirmed and superseded by the measured insertion-scaling issue.

## Changes implemented in the working copy

1. **Write back dirty victims before frame reuse.** Both `fetchPage` and `newPage` now write an occupied dirty victim to disk before removing/replacing it. If the write fails, the victim is returned to the replacer and the exception is propagated.
2. **Cache the heap page directory.** Metadata-backed `HeapFile` instances cache their page-ID list after loading and refresh it after successful persistence.
3. **Try the newest heap page first.** Inserts check the most recently allocated page first and search older pages only when necessary. This preserves the prior behavior of finding space on older pages for variable-size records while avoiding a full scan for most successful appends.
4. **Add a CTest regression test.** A test inserts 7,000 rows using a 16-frame buffer pool, closes the database, reopens it, and verifies all rows remain available.

The original ZIP was not overwritten. Changes exist in this updated project copy only.

## Before/after benchmark comparison

Five post-change trials were run at 5,000 rows / 64 pages. Median values:

| Operation | Before | After | Absolute change | Relative change |
|---|---:|---:|---:|---:|
| Table insert | 226.56 ms | 4.48 ms | -222.08 ms | 98.0% lower time |
| Heap full scan | 5.33 ms | 4.41 ms | -0.92 ms | 17.3% lower time |
| B+ tree insert | 148.48 ms | 129.56 ms | -18.92 ms | 12.7% lower time |
| Point lookup | 51.62 ms | 47.34 ms | -4.28 ms | 8.3% lower time |
| Range scan | 22.41 ms | 18.88 ms | -3.53 ms | 15.7% lower time |

Table insertion throughput derived from the median duration rose from approximately 22,069 rows/s to 1.116 million rows/s (about 50.6x). This is a combined-change comparison, not an isolated attribution to any single patch. The other timing changes are less dramatic and should be treated as indicative because this was a small benchmark suite without a dedicated statistical harness.

## Larger-workload verification

| Rows | Buffer pool | Trials | Result |
|---:|---:|---:|---|
| 6,500 | 64 | 5 | All trials passed full-scan, point-lookup, range-scan, and index-entry validation |
| 20,000 | 256 | 1 | Passed all validation |
| 50,000 | 256 | 3 | All trials passed; median table insert 505.80 ms, full scan 45.47 ms, B+ tree insert 1,570.23 ms, point lookup 168.68 ms, range scan 20.16 ms |
| 50,000 | 64 | 1 | Passed all validation; table insert 508.37 ms, full scan 43.20 ms, B+ tree insert 1,465.72 ms, point lookup 172.64 ms, range scan 20.76 ms |

The 50,000-row benchmark checks 10,000 deterministic point lookups and 5,000 range-result rows across 50 ranges; it does not perform 50,000 point lookups or a full-index range sweep.

## Test/build results

- Release build: passed.
- CTest: 1/1 passed (`dirty_page_eviction_persistence`).
- Standalone reopen test: 7,000 rows remained scannable after close/reopen with a 16-page pool.
- Benchmark validation: 6,500 rows / 64 pages passed in five consecutive trials; 50,000 rows passed in three consecutive 256-page trials and one 64-page trial.
- Remaining compiler warnings: two pre-existing unused variables in `examples/basic_database.cpp` (`surajId`, `rahulId`). No warning is reported as a runtime failure.

## Limitations and remaining work

- The benchmark does not measure per-operation p95/p99 latency, concurrency scalability, CPU utilization, or I/O wait directly.
- No original unit-test suite existed; one targeted CTest regression was added. Broader CRUD, recovery, index split/merge, and concurrent-access tests remain recommended.
- The performance results are environment-specific. Repeat on the target deployment filesystem and hardware before making production capacity claims.
- The B+ tree implementation was not changed because the initial apparent lookup hang was not reproduced after fixing table insertion and dirty-page eviction.

## Reproduction

From the project root:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DFORGEDB_BUILD_EXAMPLES=ON \
  -DFORGEDB_BUILD_BENCHMARKS=ON \
  -DBUILD_TESTING=ON
cmake --build build -j2
ctest --test-dir build --output-on-failure

mkdir -p /tmp/forgedb_run_50000
cd /tmp/forgedb_run_50000
timeout -k 2s 60s /usr/bin/time -v \
  /path/to/project/build/forgedb_benchmark 50000 256
```

Always run the benchmark in a fresh dedicated working directory: the benchmark intentionally removes its fixed database, WAL, and index filenames at startup.
