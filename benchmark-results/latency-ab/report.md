# ForgeDB matched P50/P95/P99 before-and-after benchmark

## Result

**P95 and P99 improved in the matched experiment for both point lookups and range scans.** The primary comparison below uses the median of the five per-trial percentile estimates. P50 point-lookup latency improved only modestly. The maximum-latency metric is noisy and is not claimed as an improvement.

## Compared versions

- **Baseline:** `forgedb-btree-tail-cache-optimized.zip`, the checkpoint immediately before the final low-level optimization pass.
- **Optimized:** latest uploaded `forgedb-prod(1).zip`.
- Production-code changes between checkpoints are limited to `DiskManager` and `BufferPoolManager` headers/implementations, plus README documentation. Changes cover cached file-length metadata, free-frame selection cursor, and batched stream flushing.
- The exact same instrumented `benchmarks/forgedb_benchmark.cpp` was applied to both versions; SHA-256: `30af8f6551125f140dd351f4bf6b5671061b47cac743a0c396cd295cab06be2b` for both.

## Workload and method

- Linux x86-64, C++20 Release, GCC 14.2.0, CMake 3.31.6, 4 GiB memory limit and 4-CPU cgroup quota.
- 1,000,000 integer keys, 4,096 buffer-pool pages; five baseline and five optimized process runs.
- Every run performed 10,000 measured point lookups and 10,000 measured range scans, with 1,000 warm-up calls per operation excluded from the sample set.
- Probe keys and range starts were generated deterministically with seed 43, spread over the keyspace. Each range requested up to 100 inclusive keys.
- Two `steady_clock` reads bracket each API call; result-count validation is outside the timed interval. The benchmark reports nearest-rank percentiles.
- Baseline/optimized runs alternated in order, each in a fresh working directory. Both versions built successfully and passed all 6 registered regression tests; all ten benchmark runs passed validation.

## Per-operation percentile comparison

Values are the **median of five run-level percentile estimates**. Percentile-reduction percentage = `(baseline - optimized) / baseline × 100`.

| Operation | Metric | Baseline | Optimized | Reduction | Baseline run range | Optimized run range |
|---|---|---:|---:|---:|---:|---:|
| B+ tree point lookup | P50 | 8.486 µs | 7.854 µs | **7.5%** | 8.396–8.716 µs | 7.765–8.175 µs |
| B+ tree point lookup | P95 | 41.337 µs | 10.971 µs | **73.5%** | 40.035–57.447 µs | 10.089–12.544 µs |
| B+ tree point lookup | P99 | 79.548 µs | 32.010 µs | **59.8%** | 65.872–85.770 µs | 24.876–49.142 µs |
| B+ tree range scan | P50 | 38.682 µs | 11.632 µs | **69.9%** | 37.089–39.083 µs | 11.502–12.002 µs |
| B+ tree range scan | P95 | 72.415 µs | 14.788 µs | **79.6%** | 68.958–89.477 µs | 14.447–15.118 µs |
| B+ tree range scan | P99 | 128.950 µs | 36.408 µs | **71.8%** | 120.364–138.258 µs | 33.843–54.982 µs |

## Pooled sample percentiles

These are calculated over all 50,000 observations for each operation/version. They are a second view, not the same statistic as the median run-level percentile above.

| Operation | Metric | Baseline pooled | Optimized pooled | Reduction |
|---|---|---:|---:|---:|
| B+ tree point lookup | P50 | 8.546 µs | 7.925 µs | 7.3% |
| B+ tree point lookup | P95 | 42.018 µs | 10.931 µs | 74.0% |
| B+ tree point lookup | P99 | 79.869 µs | 32.330 µs | 59.5% |
| B+ tree range scan | P50 | 38.372 µs | 11.722 µs | 69.5% |
| B+ tree range scan | P95 | 74.940 µs | 14.778 µs | 80.3% |
| B+ tree range scan | P99 | 130.904 µs | 37.059 µs | 71.7% |

## Maximum observed latency: do not overclaim

| Operation | Baseline maximum across all samples | Optimized maximum across all samples |
|---|---:|---:|
| B+ tree point lookup | 1276.628 µs | 4079.498 µs |
| B+ tree range scan | 7234.132 µs | 345.413 µs |

The maximum point-lookup sample was **4,079.498 µs** in an optimized trial versus **1,276.628 µs** in the baseline runs. This isolated outlier means the overall maximum did not improve, even though P95/P99 did. The range-scan maximum was 7,234.132 µs baseline and 345.413 µs optimized, driven by a baseline outlier. Maximum latency is highly sensitive to scheduling/background activity and is not a stable percentile-like statistic.

## Aggregate phase timings from the same runs

These are existing whole-phase timers from the same run set; they are not individual-operation latency percentiles.

| Operation | Baseline median | Optimized median | Elapsed-time reduction |
|---|---:|---:|---:|
| Table insertion | 772.54 ms | 499.24 ms | 35.38% |
| Heap full scan | 539.07 ms | 233.95 ms | 56.60% |
| B+ tree insertion | 990.15 ms | 768.80 ms | 22.36% |
| B+ tree point lookup | 256.41 ms | 92.89 ms | 63.77% |
| B+ tree range scan | 1.27 ms | 0.57 ms | 55.12% |

## Interpretation

1. **Point lookup:** median per-run P95 decreased 73.5% (41.337 to 10.971 µs); median per-run P99 decreased 59.8% (79.548 to 32.010 µs). The P50 improvement was 7.4% (8.486 to 7.854 µs). The baseline and optimized P95/P99 run ranges do not overlap in this set of five runs.
2. **Range scan:** median per-run P95 decreased 79.6% (72.415 to 14.788 µs); median per-run P99 decreased 71.8% (128.950 to 36.408 µs). The median per-run P50 decreased 69.9% (38.682 to 11.632 µs). The P95/P99 run-level ranges are fully separated in these five runs; see CSV for exact ranges.
3. The result is consistent with the final patch reducing overhead on page access paths. The experiment compares the combined patch and does not isolate which individual code change caused each percentile improvement.
4. The benchmark is warmed and single-threaded. It does not establish concurrency scaling, cold-cache latency, server/network latency, crash-recovery behavior, or a production SLO.
5. No synthetic or fabricated samples were used. All percentiles were calculated from raw API-call timings captured in this experiment.

## Files

- `per-trial-percentiles.csv`: P50/P95/P99/maximum for every trial and operation.
- `percentile-summary.csv`: median run-level values, run ranges, pooled percentiles, and observed maxima.
- `combined-latency-samples.csv`: 100,000 raw observations with version and trial labels.
- `aggregate-phase-times.csv`: original benchmark-phase times for the same five runs per version.
- `baseline/trial1..trial5/` and `optimized/trial1..trial5/`: raw samples, stdout/stderr, process metadata, and exit status for each run.
- `environment.txt` and `experiment-manifest.json`: environment and source/build hashes.
