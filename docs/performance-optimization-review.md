# ForgeDB Remaining-Bottleneck Optimization Review

Date: 2026-10-09  
Source checkpoint: `forgedb-btree-tail-cache-optimized.zip`  
Work performed on an extracted copy; the input archive was not overwritten.

## Executive summary

Three low-level overheads were reduced in this pass:

1. **Disk-file length metadata cache.** `DiskManager` previously called `std::filesystem::file_size` in the page-count / page-validation paths. The current version reads the file length once at open and maintains it as pages are written. This avoids repeated filesystem metadata queries on hot paths.
2. **Amortized free-frame selection.** `BufferPoolManager::findFreeFrame()` previously rescanned frames from index zero for each allocation. A cursor now advances over the occupied prefix and moves backward when `deletePage()` creates an earlier hole. Sequential allocation therefore no longer repeatedly scans all previously occupied frames.
3. **Batch stream flushing.** `DiskManager::writePage()` no longer flushes the C++ file stream on every page write. Explicit `BufferPoolManager::flushPage()` still calls `DiskManager::flush()`, and `flushAllPages()` writes dirty pages then flushes once at the end. The disk manager destructor still flushes. This avoids repeated stream flushes while retaining explicit flush points.

These changes were measured as a **combined patch**. The experiment does not isolate the contribution of each individual change.

## Environment

- Linux x86-64 container; kernel `6.18.44`
- Intel Xeon Platinum 8370C at 2.80 GHz; 5 logical CPUs visible
- Container memory limit: 4 GiB
- GCC 14.2.0; CMake 3.31.6
- C++20, CMake Release build
- Benchmark: 1,000,000 rows; 4,096 buffer-pool pages; seed 42; 10,000 point lookups; 50 range scans of 100 keys each
- Baseline and optimized versions were run as fresh benchmark invocations. The benchmark reported successful row-count, lookup, range-scan and index-entry validation on every recorded run.

These results are from this Linux container and should not be presented as Windows-machine measurements.

## Five-trial medians

Lower time is better. Percentage is `(baseline median - optimized median) / baseline median * 100`.

| Operation | Baseline median | Optimized median | Time reduction |
|---|---:|---:|---:|
| Table insert | 706.40 ms | 504.70 ms | 28.6% |
| Heap table full scan | 537.06 ms | 238.35 ms | 55.6% |
| B+ tree insert | 928.15 ms | 774.55 ms | 16.5% |
| B+ tree point lookup | 248.81 ms | 92.54 ms | 62.8% |
| B+ tree range scan | 1.43 ms | 0.57 ms | 60.1% |

## Raw trial measurements

The full trial-by-trial values are in `benchmark-results/remaining-bottleneck-ab-raw.csv`; median calculations and trial arrays are in `benchmark-results/remaining-bottleneck-ab-medians.csv`.

| Operation | Baseline trials (ms) | Optimized trials (ms) |
|---|---|---|
| Table insert | 692.76, 682.72, 852.17, 732.03, 706.40 | 497.85, 499.41, 504.70, 505.91, 537.91 |
| Heap table full scan | 513.82, 526.87, 655.77, 571.28, 537.06 | 234.24, 237.19, 238.35, 247.88, 239.37 |
| B+ tree insert | 928.15, 1151.23, 909.60, 1004.81, 927.24 | 772.55, 774.55, 771.47, 815.09, 907.31 |
| B+ tree point lookup | 248.81, 295.56, 245.25, 255.03, 245.46 | 94.55, 88.84, 92.54, 92.36, 179.32 |
| B+ tree range scan | 1.19, 2.79, 1.59, 1.43, 1.24 | 0.67, 0.49, 0.61, 0.57, 0.50 |

## Validation

- Release build: succeeded.
- Existing CTest regression suite: **6/6 passed** after the changes.
- All five optimized 1M-row benchmark trials passed all internal validation checks: full scan `1,000,000 / 1,000,000`, point lookup hits `10,000 / 10,000`, range rows `5,000 / 5,000`, B+ tree entries `1,000,000 / 1,000,000`.
- An AddressSanitizer/UndefinedBehaviorSanitizer test build was attempted. It did not complete within the available command timeout while running `persistent_b_plus_tree_insert_reopen`; therefore no sanitizer-pass claim is made.

## Interpretation and caveats

- The combined patch improved the median for all five benchmark phases in this experiment. The strongest observed changes were point lookups and range scans; those phases are relatively short and one optimized point-lookup trial was a clear outlier (179.32 ms versus the other four at 88.84–94.55 ms). The median is reported rather than the best run.
- Table insertion improved by about 28.6%, full scan by 55.6%, B+ tree insertion by 16.5%, point lookup by 62.8%, and range scan by 60.1% in this particular environment and workload. These are experimental results, not universal performance guarantees.
- This benchmark has five coarse aggregate timings, not per-operation p95/p99 latency, CPU profiles, hardware counters, I/O wait measurements, or isolated attribution per code change. More detailed profiling and randomized A/B ordering on the target Windows machine would further strengthen the evidence.
- Removing per-write stream flushes changes the flushing strategy: writes are buffered and explicit flush operations / finalization flush the stream. The existing regression suite passed, but this does not provide crash-injection or power-loss durability proof. Do not interpret `flush()` as an OS-level `fsync` guarantee.
- The source remains a compact embedded-engine project; these results do not establish production readiness, concurrent scalability, transaction recovery guarantees, or crash safety.

## Files changed

- `include/forgedb/storage/disk_manager.h`
- `src/storage/disk_manager.cpp`
- `include/forgedb/buffer/buffer_pool_manager.h`
- `src/buffer/buffer_pool_manager.cpp`
- Added raw and median CSV benchmark artifacts under `benchmark-results/`
- Added this review under `docs/`

## Reproduction

From the project root on a machine with CMake and a C++20 compiler:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DFORGEDB_BUILD_BENCHMARKS=ON -DFORGEDB_BUILD_EXAMPLES=OFF
cmake --build build -j4
ctest --test-dir build --output-on-failure
./build/forgedb_benchmark 1000000 4096
```

Run each benchmark in an isolated working directory if running multiple copies in parallel; the benchmark uses fixed database/index filenames and cleans them at startup.
