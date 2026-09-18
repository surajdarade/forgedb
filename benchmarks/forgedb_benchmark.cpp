#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
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

constexpr std::uint32_t kRandomSeed = 42;

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
        << " [rows] [buffer_pool_pages]\n\n"
        << "Arguments:\n"
        << "  rows                Number of records to benchmark.\n"
        << "  buffer_pool_pages   Number of pages in the buffer pool.\n\n"
        << "Examples:\n"
        << "  " << executable << " 1000 64\n"
        << "  " << executable << " 5000 64\n"
        << "  " << executable << " 50000 256\n\n"
        << "Defaults:\n"
        << "  rows                "
        << kDefaultRowCount
        << '\n'
        << "  buffer_pool_pages   "
        << kDefaultBufferPoolPages
        << '\n';
}

} // namespace

int main(int argc, char* argv[]) {
    std::size_t rowCount = kDefaultRowCount;
    std::size_t bufferPoolPages = kDefaultBufferPoolPages;

    try {
        if (argc > 3) {
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

            indexEntryCount =
                index.size();

            indexBufferPool.flushAllPages();
            indexDisk.flush();
        }

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