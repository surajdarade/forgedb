> **Update (2026-10-17):** This report records the first optimized-only latency sweep. A matched baseline/optimized run using identical latency instrumentation has since been completed. Use [`../benchmark-results/latency-ab/report.md`](../benchmark-results/latency-ab/report.md) for before/after P50/P95/P99 comparisons. The values below are historical single-version measurements, not evidence of improvement by themselves.

# ForgeDB Per-Operation Latency Benchmark Report

## Summary

The latest uploaded ForgeDB ZIP was built and benchmarked in a Linux x86-64 C++20 Release environment. The original benchmark reported aggregate phase elapsed times, but did not capture per-operation latency. The benchmark harness was extended to sample individual persistent B+ tree point lookups and range scans.

### Pooled percentiles across five trials

Each of five fresh process invocations captured 10,000 measured point lookups and 10,000 measured range scans (50,000 samples per operation in total). Percentiles use the nearest-rank method over the pooled samples. Latencies are measured with `std::chrono::steady_clock` and reported in microseconds.

| Operation | Samples | P50 | P95 | P99 | Maximum observed |
|---|---:|---:|---:|---:|---:|
| B+ tree point lookup | 50,000 | 8.005 µs | 11.661 µs | 44.943 µs | 2,056.774 µs |
| B+ tree range scan | 50,000 | 12.283 µs | 19.867 µs | 87.944 µs | 1,568.746 µs |

### Median of the five trial-level percentiles

The following table reports the median of each trial’s P50/P95/P99 estimate. It is not the same statistic as taking the percentile of all 50,000 observations; both views are provided to make between-run variation visible.

| Operation | Median trial P50 | Median trial P95 | Median trial P99 | Median trial maximum | P95 across trials (min–max) | P99 across trials (min–max) |
|---|---:|---:|---:|---:|---:|---:|
| B+ tree point lookup | 7.865 µs | 10.941 µs | 35.065 µs | 653.816 µs | 10.570–13.976 µs | 31.709–83.004 µs |
| B+ tree range scan | 11.973 µs | 16.110 µs | 82.383 µs | 934.206 µs | 14.928–50.925 µs | 44.182–119.833 µs |

### Per-trial results

| Trial | Operation | Samples | P50 | P95 | P99 | Maximum | Whole-process elapsed |
|---:|---|---:|---:|---:|---:|---:|---:|
| 1 | B+ tree point lookup | 10000 | 7.804 µs | 10.620 µs | 33.141 µs | 277.185 µs | 1.86 s |
| 1 | B+ tree range scan | 10000 | 11.832 µs | 14.928 µs | 44.182 µs | 696.515 µs | 1.86 s |
| 2 | B+ tree point lookup | 10000 | 7.864 µs | 10.570 µs | 31.709 µs | 362.514 µs | 1.97 s |
| 2 | B+ tree range scan | 10000 | 11.973 µs | 15.258 µs | 45.946 µs | 321.398 µs | 1.97 s |
| 3 | B+ tree point lookup | 10000 | 7.865 µs | 10.941 µs | 35.065 µs | 1267.076 µs | 1.98 s |
| 3 | B+ tree range scan | 10000 | 11.872 µs | 16.110 µs | 82.383 µs | 934.206 µs | 1.98 s |
| 4 | B+ tree point lookup | 10000 | 8.225 µs | 12.593 µs | 46.186 µs | 2056.774 µs | 2.22 s |
| 4 | B+ tree range scan | 10000 | 13.325 µs | 50.925 µs | 119.833 µs | 1568.746 µs | 2.22 s |
| 5 | B+ tree point lookup | 10000 | 8.335 µs | 13.976 µs | 83.004 µs | 653.816 µs | 2.15 s |
| 5 | B+ tree range scan | 10000 | 12.843 µs | 23.534 µs | 113.341 µs | 1144.628 µs | 2.15 s |

## Workload and measurement procedure

- Dataset: 1,000,000 integer keys inserted in ascending order into the persistent B+ tree; 4,096 buffer-pool pages.
- Benchmark seed: 42 for the original deterministic workload; percentile probes use a separate deterministic generator seeded with 43.
- Point lookup distribution: 10,000 uniformly sampled keys over the full key domain per trial.
- Range lookup distribution: 10,000 uniformly sampled valid range starts over the key domain; each range returns up to 100 inclusive keys.
- Warm-up: up to 1,000 point lookups and 1,000 range scans per trial, excluded from the sample distribution.
- Samples are taken after the original aggregate point-lookup and range-scan phases; therefore they represent a warmed/steady-state, single-threaded workload, not cold-start latency.
- Two steady-clock reads bracket each API call. The reported latency is elapsed time from immediately before the API call until immediately after the call returns; result-count verification is outside the timed interval.
- All five runs passed built-in validations: 1,000,000 full-scan rows, 10,000/10,000 original point lookup hits, 5,000/5,000 original range-result rows, 1,000,000 B+ tree entries, and validation of every sampled lookup/range result.

## Environment

- Linux x86-64; kernel 6.18.55.
- AMD EPYC 7763 host processor; 5 logical CPUs visible to the container, with a 4-CPU cgroup quota.
- 4 GiB container memory limit.
- GCC 14.2.0; CMake 3.31.6; C++20 Release build.
- Five separate process runs; whole benchmark process elapsed time ranged from 1.86 to 2.22 seconds, inclusive of data creation and both latency-sampling phases.

## Interpretation and limitations

- These are measurements of the latest optimized source only. The uploaded archive has no matching historical baseline source/executable with individual latency sampling enabled, so no before/after P50/P95/P99 improvement can be claimed from these data.
- Point lookup P99 varied from 31.709 to 83.004 µs across the five runs. Range-scan P95 varied from 14.928 to 50.925 µs, and range-scan P99 from 44.182 to 119.833 µs. This shows noticeable tail variance in the shared container.
- Maximum values include scheduling and background-system effects, and should not be interpreted as stable service-level latency.
- The experiment is single-threaded. It does not measure concurrency scaling, client/server network latency, production I/O patterns, cold-cache performance, or transaction durability.
- These per-operation percentiles are distinct from the historical aggregate benchmark phase timings and their five-run medians.

## Artifacts

- `combined-latency-samples.csv`: all 100,000 raw samples with trial identifiers.
- `trial1/` through `trial5/`: raw per-run samples (`forgedb_latency_samples.csv`), stdout, stderr, process timing, and exit code.
- `per-trial-percentiles.csv`: per-run latency summaries.
- `percentile-summary.csv`: pooled percentiles and trial-level summary statistics.
- `per-trial-percentiles.json`: machine-readable workload, per-trial summaries, and pooled summary.
