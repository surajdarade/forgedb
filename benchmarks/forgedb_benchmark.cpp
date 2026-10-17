#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

#include "forgedb/buffer/buffer_pool_manager.h"
#include "forgedb/database/database.h"
#include "forgedb/index/index_key.h"
#include "forgedb/index/persistent_b_plus_tree.h"
#include "forgedb/storage/disk_manager.h"

namespace {

using Clock = std::chrono::steady_clock;

constexpr std::size_t kDefaultRowCount = 5000;
constexpr std::size_t kDefaultBufferPoolPages = 64;

constexpr std::size_t kMaxLookupCount = 10'000;
constexpr std::size_t kRangeWidth = 100;
constexpr std::size_t kMaxRangeCount = 50;
constexpr std::size_t kDefaultLatencySamples = 10'000;
constexpr std::size_t kLatencyWarmupCount = 1'000;

constexpr std::uint32_t kRandomSeed = 42;

struct LatencySample {
    std::string operation;
    std::size_t sampleIndex{};
    std::int32_t lowerKey{};
    std::int32_t upperKey{};
    std::int64_t latencyNanoseconds{};
    std::size_t resultCount{};
};

struct LatencySummary {
    double p50Microseconds{};
    double p95Microseconds{};
    double p99Microseconds{};
    double maxMicroseconds{};
};

struct BenchmarkResult {
    std::string name;
    double milliseconds{};
    double operationsPerSecond{};
    double nanosecondsPerOperation{};
};

template <typename Function>
BenchmarkResult measure(
    std::string name,
    std::size_t operations,
    Function&& function
) {
    const auto start = Clock::now();

    function();

    const auto end = Clock::now();

    const double milliseconds =
        std::chrono::duration<double, std::milli>(
            end - start
        ).count();

    const double seconds =
        milliseconds / 1000.0;

    return {
        std::move(name),
        milliseconds,
        operations == 0 || seconds == 0.0
            ? 0.0
            : static_cast<double>(operations) / seconds,
        operations == 0
            ? 0.0
            : milliseconds * 1'000'000.0 /
                  static_cast<double>(operations)
    };
}


std::int64_t percentileNanoseconds(
    std::vector<std::int64_t> sortedLatencies,
    std::size_t percentile
) {
    if (sortedLatencies.empty()) {
        throw std::invalid_argument(
            "Cannot calculate latency percentiles from an empty sample."
        );
    }

    std::sort(
        sortedLatencies.begin(),
        sortedLatencies.end()
    );

    // Nearest-rank percentile: sorted[ceil(p * N) - 1].
    const std::size_t rank =
        (sortedLatencies.size() * percentile + 99) / 100;
    const std::size_t index = rank == 0 ? 0 : rank - 1;

    return sortedLatencies[
        std::min(index, sortedLatencies.size() - 1)
    ];
}

LatencySummary summarizeLatencies(
    const std::vector<LatencySample>& samples,
    const std::string& operation
) {
    std::vector<std::int64_t> latencies;
    latencies.reserve(samples.size());

    for (const auto& sample : samples) {
        if (sample.operation == operation) {
            latencies.push_back(sample.latencyNanoseconds);
        }
    }

    if (latencies.empty()) {
        throw std::invalid_argument(
            "No latency samples recorded for " + operation
        );
    }

    const auto p50 = percentileNanoseconds(latencies, 50);
    const auto p95 = percentileNanoseconds(latencies, 95);
    const auto p99 = percentileNanoseconds(latencies, 99);
    const auto maximum = *std::max_element(
        latencies.begin(),
        latencies.end()
    );

    constexpr double kNanosecondsPerMicrosecond = 1000.0;
    return {
        static_cast<double>(p50) / kNanosecondsPerMicrosecond,
        static_cast<double>(p95) / kNanosecondsPerMicrosecond,
        static_cast<double>(p99) / kNanosecondsPerMicrosecond,
        static_cast<double>(maximum) / kNanosecondsPerMicrosecond
    };
}

void printLatencySummary(
    const std::string& operation,
    const LatencySummary& summary,
    std::size_t sampleCount
) {
    std::cout << std::left << std::setw(30) << operation
              << std::right << " samples=" << sampleCount
              << std::fixed << std::setprecision(3)
              << " p50=" << summary.p50Microseconds << " us"
              << " p95=" << summary.p95Microseconds << " us"
              << " p99=" << summary.p99Microseconds << " us"
              << " max=" << summary.maxMicroseconds << " us\n";
}

void writeLatencySamples(
    const std::vector<LatencySample>& samples,
    std::size_t rowCount,
    std::size_t bufferPoolPages,
    const std::filesystem::path& path
) {
    std::ofstream output(path, std::ios::trunc);
    if (!output) {
        throw std::runtime_error(
            "Could not open latency sample CSV: " + path.string()
        );
    }

    output << "operation,sample_index,rows,buffer_pool_pages,lower_key,upper_key,latency_ns,result_count\n";
    for (const auto& sample : samples) {
        output << sample.operation << ','
               << sample.sampleIndex << ','
               << rowCount << ','
               << bufferPoolPages << ','
               << sample.lowerKey << ','
               << sample.upperKey << ','
               << sample.latencyNanoseconds << ','
               << sample.resultCount << '\n';
    }

    if (!output) {
        throw std::runtime_error(
            "Failed while writing latency sample CSV: " + path.string()
        );
    }
}

void printResult(const BenchmarkResult& result) {
    std::cout << std::left
              << std::setw(30)
              << result.name
              << std::right
              << std::setw(12)
              << std::fixed
              << std::setprecision(2)
              << result.milliseconds
              << " ms"
              << std::setw(18)
              << result.operationsPerSecond
              << " ops/s"
              << std::setw(18)
              << result.nanosecondsPerOperation
              << " ns/op\n";
}

std::size_t parsePositiveSize(
    const char* argument,
    const char* argumentName
) {
    const std::string value(argument);

    std::size_t parsedCharacters = 0;

    const unsigned long long parsed =
        std::stoull(
            value,
            &parsedCharacters
        );

    if (parsedCharacters != value.size()) {
        throw std::invalid_argument(
            std::string("Invalid ") +
            argumentName +
            ": " +
            value
        );
    }

    if (parsed == 0) {
        throw std::invalid_argument(
            std::string(argumentName) +
            " must be greater than zero."
        );
    }

    return static_cast<std::size_t>(parsed);
}

void printUsage(const char* executable) {
    std::cout
        << "Usage:\n"
        << "  " << executable
        << " [rows] [buffer_pool_pages] [latency_samples]\n\n"
        << "Arguments:\n"
        << "  rows                Number of records to benchmark.\n"
        << "  buffer_pool_pages   Number of pages in the buffer pool.\n"
        << "  latency_samples     Per-operation samples for lookup and range latency.\n\n"
        << "Examples:\n"
        << "  " << executable << " 1000 64\n"
        << "  " << executable << " 5000 64\n"
        << "  " << executable << " 50000 256\n"
        << "  " << executable << " 1000000 4096 10000\n\n"
        << "Defaults:\n"
        << "  rows                "
        << kDefaultRowCount
        << '\n'
        << "  buffer_pool_pages   "
        << kDefaultBufferPoolPages
        << "\n"
        << "  latency_samples     "
        << kDefaultLatencySamples
        << '\n';
}

} // namespace

int main(int argc, char* argv[]) {
    std::size_t rowCount = kDefaultRowCount;
    std::size_t bufferPoolPages = kDefaultBufferPoolPages;
    std::size_t requestedLatencySamples = kDefaultLatencySamples;

    try {
        if (argc > 4) {
            printUsage(argv[0]);
            return 1;
        }

        if (argc >= 2) {
            rowCount =
                parsePositiveSize(
                    argv[1],
                    "row count"
                );
        }

        if (argc >= 3) {
            bufferPoolPages =
                parsePositiveSize(
                    argv[2],
                    "buffer pool size"
                );
        }

        if (argc >= 4) {
            requestedLatencySamples =
                parsePositiveSize(
                    argv[3],
                    "latency sample count"
                );
        }
    }
    catch (const std::exception& error) {
        std::cerr
            << "Invalid benchmark arguments: "
            << error.what()
            << "\n\n";

        printUsage(argv[0]);

        return 1;
    }

    const std::size_t lookupCount =
        std::min(
            rowCount,
            kMaxLookupCount
        );

    const std::size_t rangeCount =
        std::min(
            kMaxRangeCount,
            (rowCount + kRangeWidth - 1) /
                kRangeWidth
        );

    const std::size_t latencySampleCount =
        requestedLatencySamples;
    const std::size_t latencyWarmupCount =
        std::min(kLatencyWarmupCount, latencySampleCount);
    std::vector<LatencySample> latencySamples;
    latencySamples.reserve(latencySampleCount * 2);

    const std::string tablePath =
        "forgedb_benchmark_table.db";

    const std::string indexPath =
        "forgedb_benchmark_index.db";

    const std::string walPath =
        tablePath + ".wal";

    // Remove files from any previous benchmark run.
    std::filesystem::remove(tablePath);
    std::filesystem::remove(walPath);
    std::filesystem::remove(indexPath);

    try {
        std::cout
            << "ForgeDB benchmark\n"
            << "=================\n\n"

            << "Configuration\n"
            << "  Rows: "
            << rowCount
            << '\n'

            << "  Buffer pool: "
            << bufferPoolPages
            << " pages\n"

            << "  Point lookups: "
            << lookupCount
            << '\n'

            << "  Range scans: "
            << rangeCount
            << '\n'

            << "  Range width: "
            << kRangeWidth
            << " rows\n"

            << "  Random seed: "
            << kRandomSeed
            << '\n'
            << "  Latency samples per operation: "
            << latencySampleCount
            << "\n\n";

        std::vector<forgedb::RecordId> recordIds;

        recordIds.reserve(rowCount);

        std::size_t scannedRows = 0;

        // --------------------------------------------------------
        // Heap table benchmark
        // --------------------------------------------------------

        {
            auto db =
                forgedb::Database::open(
                    tablePath,
                    bufferPoolPages
                );

            auto& table =
                db->createTable(
                    "benchmark",
                    forgedb::Schema{{
                        forgedb::Column{
                            "id",
                            forgedb::DataType::Int32
                        },
                        forgedb::Column{
                            "payload",
                            forgedb::DataType::Varchar,
                            64
                        }
                    }}
                );

            const auto insertResult =
                measure(
                    "Table insert",
                    rowCount,
                    [&] {
                        for (std::size_t i = 0;
                             i < rowCount;
                             ++i) {

                            const auto id =
                                static_cast<
                                    std::int32_t
                                >(i);

                            recordIds.push_back(
                                table.insert(
                                    forgedb::Tuple{{
                                        forgedb::Value{id},
                                        forgedb::Value{
                                            "forge-benchmark-payload"
                                        }
                                    }}
                                )
                            );
                        }
                    }
                );

            printResult(insertResult);

            const auto scanResult =
                measure(
                    "Heap table full scan",
                    rowCount,
                    [&] {
                        scannedRows =
                            table.scan().size();
                    }
                );

            printResult(scanResult);

            db->close();
        }

        // --------------------------------------------------------
        // Persistent B+ Tree benchmark
        // --------------------------------------------------------

        std::size_t pointLookupHits = 0;
        std::size_t rangeRows = 0;
        std::size_t indexEntryCount = 0;

        {
            forgedb::DiskManager indexDisk(
                indexPath
            );

            forgedb::BufferPoolManager indexBufferPool(
                bufferPoolPages,
                indexDisk
            );

            forgedb::PersistentBPlusTree index(
                indexBufferPool
            );

            const auto indexInsertResult =
                measure(
                    "B+ tree insert",
                    rowCount,
                    [&] {
                        for (std::size_t i = 0;
                             i < rowCount;
                             ++i) {

                            const auto key =
                                static_cast<
                                    std::int32_t
                                >(i);

                            if (!index.insert(
                                    forgedb::IndexKey{key},
                                    recordIds[i]
                                )) {

                                throw std::runtime_error(
                                    "Benchmark: duplicate "
                                    "B+ tree insertion"
                                );
                            }
                        }
                    }
                );

            printResult(indexInsertResult);

            // ----------------------------------------------------
            // Generate deterministic lookup workload.
            // ----------------------------------------------------

            std::mt19937 generator(
                kRandomSeed
            );

            std::uniform_int_distribution<
                std::int32_t
            > distribution(
                0,
                static_cast<
                    std::int32_t
                >(rowCount - 1)
            );

            std::vector<std::int32_t> lookupKeys;

            lookupKeys.reserve(
                lookupCount
            );

            for (std::size_t i = 0;
                 i < lookupCount;
                 ++i) {

                lookupKeys.push_back(
                    distribution(generator)
                );
            }

            // ----------------------------------------------------
            // Point lookup benchmark
            // ----------------------------------------------------

            const auto lookupResult =
                measure(
                    "B+ tree point lookup",
                    lookupCount,
                    [&] {
                        for (const auto key :
                             lookupKeys) {

                            const auto records =
                                index.lookup(
                                    forgedb::IndexKey{
                                        key
                                    }
                                );

                            if (records.size() == 1) {
                                ++pointLookupHits;
                            }
                        }
                    }
                );

            printResult(lookupResult);

            // ----------------------------------------------------
            // Range scan benchmark
            // ----------------------------------------------------

            const auto rangeResult =
                measure(
                    "B+ tree range scan",
                    rangeCount,
                    [&] {
                        for (
                            std::size_t start = 0;
                            start < rangeCount;
                            ++start
                        ) {
                            const auto lower =
                                static_cast<
                                    std::int32_t
                                >(
                                    start *
                                    kRangeWidth
                                );

                            const auto upper =
                                std::min<
                                    std::int32_t
                                >(
                                    static_cast<
                                        std::int32_t
                                    >(
                                        lower +
                                        static_cast<
                                            std::int32_t
                                        >(
                                            kRangeWidth - 1
                                        )
                                    ),
                                    static_cast<
                                        std::int32_t
                                    >(rowCount - 1)
                                );

                            rangeRows +=
                                index.scan(
                                    forgedb::IndexKey{
                                        lower
                                    },
                                    forgedb::IndexKey{
                                        upper
                                    }
                                ).size();
                        }
                    }
                );

            printResult(rangeResult);

            // ----------------------------------------------------
            // Per-operation latency sampling (separate from aggregate
            // timing so per-call clock reads do not affect above results).
            // The sample workload is deterministic and spread over the key
            // domain. Warm-up operations are excluded from the percentiles.
            // ----------------------------------------------------

            std::mt19937 latencyGenerator(
                kRandomSeed + 1U
            );
            std::uniform_int_distribution<std::int32_t> latencyKeyDistribution(
                0,
                static_cast<std::int32_t>(rowCount - 1)
            );

            std::vector<std::int32_t> latencyLookupKeys;
            latencyLookupKeys.reserve(latencySampleCount);
            for (std::size_t i = 0; i < latencySampleCount; ++i) {
                latencyLookupKeys.push_back(
                    latencyKeyDistribution(latencyGenerator)
                );
            }

            for (std::size_t i = 0; i < latencyWarmupCount; ++i) {
                const auto warmupRecords = index.lookup(
                    forgedb::IndexKey{latencyLookupKeys[i]}
                );
                if (warmupRecords.size() != 1) {
                    throw std::runtime_error(
                        "Latency warm-up point lookup validation failed"
                    );
                }
            }

            for (std::size_t i = 0; i < latencySampleCount; ++i) {
                const auto key = latencyLookupKeys[i];
                const auto start = Clock::now();
                const auto records = index.lookup(
                    forgedb::IndexKey{key}
                );
                const auto end = Clock::now();

                latencySamples.push_back({
                    "B+ tree point lookup",
                    i,
                    key,
                    key,
                    std::chrono::duration_cast<std::chrono::nanoseconds>(
                        end - start
                    ).count(),
                    records.size()
                });
            }

            const std::int32_t maxRangeStart =
                rowCount > kRangeWidth
                    ? static_cast<std::int32_t>(rowCount - kRangeWidth)
                    : 0;
            std::uniform_int_distribution<std::int32_t> rangeStartDistribution(
                0,
                maxRangeStart
            );

            std::vector<std::int32_t> latencyRangeStarts;
            latencyRangeStarts.reserve(latencySampleCount);
            for (std::size_t i = 0; i < latencySampleCount; ++i) {
                latencyRangeStarts.push_back(
                    rangeStartDistribution(latencyGenerator)
                );
            }

            const auto rangeUpper = [rowCount](std::int32_t lower) {
                return std::min<std::int32_t>(
                    static_cast<std::int32_t>(rowCount - 1),
                    lower + static_cast<std::int32_t>(kRangeWidth - 1)
                );
            };

            for (std::size_t i = 0; i < latencyWarmupCount; ++i) {
                const auto lower = latencyRangeStarts[i];
                const auto results = index.scan(
                    forgedb::IndexKey{lower},
                    forgedb::IndexKey{rangeUpper(lower)}
                );
                const auto expected = static_cast<std::size_t>(
                    rangeUpper(lower) - lower + 1
                );
                if (results.size() != expected) {
                    throw std::runtime_error(
                        "Latency warm-up range scan validation failed"
                    );
                }
            }

            for (std::size_t i = 0; i < latencySampleCount; ++i) {
                const auto lower = latencyRangeStarts[i];
                const auto upper = rangeUpper(lower);
                const auto start = Clock::now();
                const auto results = index.scan(
                    forgedb::IndexKey{lower},
                    forgedb::IndexKey{upper}
                );
                const auto end = Clock::now();

                latencySamples.push_back({
                    "B+ tree range scan",
                    i,
                    lower,
                    upper,
                    std::chrono::duration_cast<std::chrono::nanoseconds>(
                        end - start
                    ).count(),
                    results.size()
                });
            }

            indexEntryCount =
                index.size();

            indexBufferPool.flushAllPages();
            indexDisk.flush();
        }

        // --------------------------------------------------------
        // Percentile latency summaries and raw samples
        // --------------------------------------------------------

        const auto pointLatencySummary = summarizeLatencies(
            latencySamples,
            "B+ tree point lookup"
        );
        const auto rangeLatencySummary = summarizeLatencies(
            latencySamples,
            "B+ tree range scan"
        );

        std::cout << "\nPer-operation latency (steady-clock samples; nearest-rank percentiles)\n";
        printLatencySummary(
            "B+ tree point lookup",
            pointLatencySummary,
            latencySampleCount
        );
        printLatencySummary(
            "B+ tree range scan",
            rangeLatencySummary,
            latencySampleCount
        );
        std::cout << "Raw samples: forgedb_latency_samples.csv\n";

        writeLatencySamples(
            latencySamples,
            rowCount,
            bufferPoolPages,
            "forgedb_latency_samples.csv"
        );

        // --------------------------------------------------------
        // Validation
        // --------------------------------------------------------

        const std::size_t expectedRangeRows =
            std::min(
                rowCount,
                rangeCount * kRangeWidth
            );

        if (scannedRows != rowCount) {
            throw std::runtime_error(
                "Benchmark validation failed: "
                "full scan row count"
            );
        }

        if (pointLookupHits != lookupCount) {
            throw std::runtime_error(
                "Benchmark validation failed: "
                "point lookup hits"
            );
        }

        if (rangeRows != expectedRangeRows) {
            throw std::runtime_error(
                "Benchmark validation failed: "
                "range scan rows"
            );
        }

        if (indexEntryCount != rowCount) {
            throw std::runtime_error(
                "Benchmark validation failed: "
                "B+ tree entry count"
            );
        }

        if (latencySamples.size() != latencySampleCount * 2) {
            throw std::runtime_error(
                "Benchmark validation failed: latency sample count"
            );
        }
        for (const auto& sample : latencySamples) {
            if (sample.operation == "B+ tree point lookup") {
                if (sample.resultCount != 1) {
                    throw std::runtime_error(
                        "Benchmark validation failed: percentile point lookup result count"
                    );
                }
            }
            else {
                const auto expectedCount = static_cast<std::size_t>(
                    sample.upperKey - sample.lowerKey + 1
                );
                if (sample.resultCount != expectedCount) {
                    throw std::runtime_error(
                        "Benchmark validation failed: percentile range scan result count"
                    );
                }
            }
        }

        std::cout
            << "\nValidation\n"
            << "  Full scan rows: "
            << scannedRows
            << " / "
            << rowCount
            << '\n'

            << "  Point lookup hits: "
            << pointLookupHits
            << " / "
            << lookupCount
            << '\n'

            << "  Range scan rows: "
            << rangeRows
            << " / "
            << expectedRangeRows
            << '\n'

            << "  B+ tree entries: "
            << indexEntryCount
            << " / "
            << rowCount
            << '\n';

        // --------------------------------------------------------
        // Cleanup
        // --------------------------------------------------------

        std::filesystem::remove(
            tablePath
        );

        std::filesystem::remove(
            walPath
        );

        std::filesystem::remove(
            indexPath
        );

        std::cout
            << "\nBenchmark completed successfully.\n";

        return 0;
    }
    catch (const std::exception& error) {
        std::filesystem::remove(
            tablePath
        );

        std::filesystem::remove(
            walPath
        );

        std::filesystem::remove(
            indexPath
        );

        std::cerr
            << "ForgeDB benchmark failed: "
            << error.what()
            << '\n';

        return 1;
    }
}