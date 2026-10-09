#include <cstdint>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

#include "forgedb/database/database.h"
#include "forgedb/record/tuple.h"
#include "forgedb/record/value.h"
#include "forgedb/schema/column.h"
#include "forgedb/schema/schema.h"

using namespace forgedb;

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
}

int main() {
    const auto path = std::filesystem::temp_directory_path() /
        "forgedb_heap_scan_batch_regression.db";
    const std::string databasePath = path.string();
    std::error_code error;
    std::filesystem::remove(path, error);
    std::filesystem::remove(databasePath + ".wal", error);

    constexpr std::int32_t kRows = 2500;
    constexpr std::int32_t kDeleteStride = 7;
    std::vector<RecordId> ids;
    ids.reserve(kRows);

    try {
        {
            auto database = Database::open(databasePath, 32);
            auto& table = database->createTable(
                "scan_rows",
                Schema{{Column{"id", DataType::Int32},
                       Column{"payload", DataType::Varchar, 64}}});

            for (std::int32_t i = 0; i < kRows; ++i) {
                ids.push_back(table.insert(Tuple{{
                    Value{i}, Value{"scan-payload-" + std::to_string(i)}}}));
            }
            for (std::int32_t i = 0; i < kRows; i += kDeleteStride) {
                table.remove(ids[static_cast<std::size_t>(i)]);
            }

            const auto rows = table.scan();
            const auto deletedCount = static_cast<std::size_t>((kRows - 1) / kDeleteStride + 1);
            require(rows.size() == static_cast<std::size_t>(kRows) - deletedCount,
                    "scan returned incorrect live-row count");

            std::unordered_set<std::int32_t> seen;
            seen.reserve(rows.size());
            for (const auto& row : rows) {
                const auto id = row.getValue(0).asInt32();
                require(id >= 0 && id < kRows, "scan returned out-of-range row ID");
                require(id % kDeleteStride != 0, "scan returned a deleted row");
                require(row.getValue(1).asString() == "scan-payload-" + std::to_string(id),
                        "scan payload did not match row ID");
                require(seen.insert(id).second, "scan returned a duplicate row");
            }
            require(seen.size() == rows.size(), "scan uniqueness check failed");
            database->close();
        }

        {
            auto database = Database::open(databasePath, 32);
            auto& table = database->openTable("scan_rows");
            const auto rows = table.scan();
            const auto deletedCount = static_cast<std::size_t>((kRows - 1) / kDeleteStride + 1);
            require(rows.size() == static_cast<std::size_t>(kRows) - deletedCount,
                    "scan count changed after reopen");
            database->close();
        }

        std::filesystem::remove(path, error);
        std::filesystem::remove(databasePath + ".wal", error);
        std::cout << "Heap batched scan regression passed (deleted rows, payloads, reopen).\n";
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << "Heap batched scan regression failed: " << exception.what() << '\n';
        std::filesystem::remove(path, error);
        std::filesystem::remove(databasePath + ".wal", error);
        return 1;
    }
}
