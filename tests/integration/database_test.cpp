#include <filesystem>
#include <gtest/gtest.h>
#include "forgedb/database/database.h"
namespace forgedb {
TEST(DatabaseTest, CatalogAndRowsSurviveReopen) {
    const auto path = std::filesystem::temp_directory_path() / "forgedb_database_test.db";
    std::filesystem::remove(path);
    {
        auto db = Database::open(path.string(), 16);
        auto& table = db->createTable("users", Schema{{
            Column{"id", DataType::Int32},
            Column{"name", DataType::Varchar, 32}
        }});
        const auto inserted = table.insert(Tuple{{Value{std::int32_t{7}}, Value{"Alice"}}});
        ASSERT_TRUE(inserted.isValid());
        db->close();
    }
    {
        auto db = Database::open(path.string(), 16);
        ASSERT_TRUE(db->hasTable("users"));
        Query query;
        query.where({0, ComparisonOperator::Equal, Value{std::int32_t{7}}});
        const auto rows = db->select("users", query);
        ASSERT_EQ(rows.size(), 1U);
        EXPECT_EQ(rows[0].getValue(1).asString(), "Alice");
        db->close();
    }
    std::filesystem::remove(path);
    std::filesystem::remove(path.string() + ".wal");
}
}
