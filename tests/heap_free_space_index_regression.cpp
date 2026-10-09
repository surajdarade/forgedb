#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "forgedb/database/database.h"
#include "forgedb/record/tuple.h"
#include "forgedb/record/value.h"
#include "forgedb/schema/column.h"
#include "forgedb/schema/schema.h"

using namespace forgedb;

int main() {
    const auto path = std::filesystem::temp_directory_path() /
        "forgedb_free_space_index_regression.db";
    const std::string databasePath = path.string();
    std::error_code error;
    std::filesystem::remove(path, error);
    std::filesystem::remove(databasePath + ".wal", error);

    constexpr std::int32_t kInitialRows = 3000;
    constexpr std::int32_t kAdditionalRows = 1000;
    constexpr std::size_t kBufferPoolPages = 16;

    try {
        std::vector<RecordId> ids;
        ids.reserve(kInitialRows);
        {
            auto database = Database::open(databasePath, kBufferPoolPages);
            auto& table = database->createTable(
                "rows",
                Schema{{Column{"id", DataType::Int32},
                       Column{"payload", DataType::Varchar, 64}}});

            for (std::int32_t id = 0; id < kInitialRows; ++id) {
                const auto length = static_cast<std::size_t>((id % 8) + 1);
                ids.push_back(table.insert(Tuple{{
                    Value{id}, Value{std::string(length, static_cast<char>('a' + id % 26))}}}));
            }

            // Mix short records into the existing heap after the free-space index is warm.
            RecordId lastInserted;
            for (std::int32_t i = 0; i < kAdditionalRows; ++i) {
                const auto id = kInitialRows + i;
                lastInserted = table.insert(Tuple{{Value{id}, Value{"small"}}});
            }

            // Grow one recent record while its page has sufficient headroom.
            table.update(lastInserted, Tuple{{
                Value{kInitialRows + kAdditionalRows - 1}, Value{std::string(64, 'z')}}});
            ids.push_back(lastInserted);
            database->close();
        }

        {
            auto database = Database::open(databasePath, kBufferPoolPages);
            auto& table = database->openTable("rows");
            const auto rows = table.scan();
            const auto expected = static_cast<std::size_t>(kInitialRows + kAdditionalRows);
            if (rows.size() != expected) {
                std::cerr << "Expected " << expected << " rows after reopen, found "
                          << rows.size() << '\n';
                return 1;
            }

            const auto updated = table.get(ids.back());
            if (updated.getValue(0).asInt32() != kInitialRows + kAdditionalRows - 1 ||
                updated.getValue(1).asString() != std::string(64, 'z')) {
                std::cerr << "Updated row did not survive close/reopen\n";
                return 1;
            }
            database->close();
        }

        std::filesystem::remove(path, error);
        std::filesystem::remove(databasePath + ".wal", error);
        std::cout << "Heap free-space index regression passed (variable-size rows, updates, reopen).\n";
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << "Regression test failed: " << exception.what() << '\n';
        std::filesystem::remove(path, error);
        std::filesystem::remove(databasePath + ".wal", error);
        return 1;
    }
}
