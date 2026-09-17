#include <cstddef>
#include <stdexcept>
#include <vector>

#include <gtest/gtest.h>

#include "forgedb/schema/column.h"
#include "forgedb/schema/data_type.h"
#include "forgedb/schema/schema.h"

namespace forgedb {

TEST(SchemaTest, StoresColumnsInOrder)
{
    Schema schema{
        {
            Column{"id", DataType::Int32},
            Column{"name", DataType::Varchar, 100},
            Column{"salary", DataType::Double}
        }
    };

    ASSERT_EQ(schema.columnCount(), 3);

    EXPECT_EQ(schema.column(0).name(), "id");
    EXPECT_EQ(schema.column(1).name(), "name");
    EXPECT_EQ(schema.column(2).name(), "salary");
}

TEST(SchemaTest, ProvidesColumnAccessByIndex)
{
    Schema schema{
        {
            Column{"id", DataType::Int32},
            Column{"name", DataType::Varchar, 100}
        }
    };

    EXPECT_EQ(schema.column(0).type(), DataType::Int32);
    EXPECT_EQ(schema.column(1).type(), DataType::Varchar);
}

TEST(SchemaTest, FindsColumnIndexByName)
{
    Schema schema{
        {
            Column{"id", DataType::Int32},
            Column{"name", DataType::Varchar, 100},
            Column{"salary", DataType::Double}
        }
    };

    EXPECT_EQ(schema.columnIndex("id"), 0);
    EXPECT_EQ(schema.columnIndex("name"), 1);
    EXPECT_EQ(schema.columnIndex("salary"), 2);
}

TEST(SchemaTest, ReturnsAllColumns)
{
    Schema schema{
        {
            Column{"id", DataType::Int32},
            Column{"name", DataType::Varchar, 100}
        }
    };

    const auto& columns = schema.columns();

    ASSERT_EQ(columns.size(), 2);

    EXPECT_EQ(columns[0].name(), "id");
    EXPECT_EQ(columns[1].name(), "name");
}

TEST(SchemaTest, CalculatesTotalStorageSize)
{
    Schema schema{
        {
            Column{"id", DataType::Int32},
            Column{"score", DataType::Float},
            Column{"salary", DataType::Double},
            Column{"active", DataType::Boolean},
            Column{"name", DataType::Varchar, 50}
        }
    };

    const std::size_t expectedSize =
        sizeof(std::int32_t) +
        sizeof(float) +
        sizeof(double) +
        sizeof(bool) +
        50;

    EXPECT_EQ(schema.storageSize(), expectedSize);
}

TEST(SchemaTest, SupportsEmptySchema)
{
    Schema schema{std::vector<Column>{}};

    EXPECT_EQ(schema.columnCount(), 0);
    EXPECT_TRUE(schema.columns().empty());
    EXPECT_EQ(schema.storageSize(), 0);
}

TEST(SchemaTest, RejectsDuplicateColumnNames)
{
    EXPECT_THROW(
        Schema{
            {
                Column{"id", DataType::Int32},
                Column{"id", DataType::Int64}
            }
        },
        std::invalid_argument
    );
}

TEST(SchemaTest, ThrowsForInvalidColumnIndex)
{
    Schema schema{
        {
            Column{"id", DataType::Int32}
        }
    };

    EXPECT_THROW(
        schema.column(1),
        std::out_of_range
    );
}

TEST(SchemaTest, ThrowsForMissingColumnName)
{
    Schema schema{
        {
            Column{"id", DataType::Int32},
            Column{"name", DataType::Varchar, 100}
        }
    };

    EXPECT_THROW(
        schema.columnIndex("salary"),
        std::out_of_range
    );
}

} // namespace forgedb