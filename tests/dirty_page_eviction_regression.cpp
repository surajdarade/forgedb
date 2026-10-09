#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>

#include "forgedb/database/database.h"
#include "forgedb/record/tuple.h"
#include "forgedb/record/value.h"
#include "forgedb/schema/column.h"
#include "forgedb/schema/schema.h"

using namespace forgedb;

int main() {
    const auto path =
        std::filesystem::temp_directory_path() /
        "forgedb_dirty_page_eviction_regression.db";
    const std::string databasePath = path.string();

    std::error_code error;
    std::filesystem::remove(path, error);
    std::filesystem::remove(databasePath + ".wal", error);

    constexpr std::int32_t kRowCount = 7000;
    constexpr std::size_t kSmallBufferPool = 16;

    try {
        {
            auto database = Database::open(databasePath, kSmallBufferPool);
            auto& table = database->createTable(
                "rows",
                Schema{{
                    Column{"id", DataType::Int32},
                    Column{"payload", DataType::Varchar, 64}
                }}
            );

            for (std::int32_t id = 0; id < kRowCount; ++id) {
                (void)table.insert(Tuple{{Value{id}, Value{"payload"}}});
            }

            database->close();
        }

        {
            auto database = Database::open(databasePath, kSmallBufferPool);
            auto& table = database->openTable("rows");
            const auto rows = table.scan();

            if (rows.size() != static_cast<std::size_t>(kRowCount)) {
                std::cerr << "Expected " << kRowCount
                          << " persisted rows, found " << rows.size() << '\n';
                return 1;
            }

            database->close();
        }

        std::filesystem::remove(path, error);
        std::filesystem::remove(databasePath + ".wal", error);
        std::cout << "Dirty-page eviction persistence regression passed ("
                  << kRowCount << " rows, " << kSmallBufferPool
                  << " buffer frames).\n";
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << "Regression test failed: " << exception.what() << '\n';
        std::filesystem::remove(path, error);
        std::filesystem::remove(databasePath + ".wal", error);
        return 1;
    }
}
