#include <cstddef>
#include <cstdint>
#include <stdexcept>

#include <gtest/gtest.h>

#include "forgedb/schema/column.h"
#include "forgedb/schema/data_type.h"

namespace forgedb {

TEST(ColumnTest, StoresNameAndType)
{
    const Column column{
        "id",
        DataType::Int32
    };

    EXPECT_EQ(column.name(), "id");
    EXPECT_EQ(column.type(), DataType::Int32);
    EXPECT_EQ(column.length(), 0);
}

TEST(ColumnTest, StoresVarcharLength)
{
    const Column column{
        "name",
        DataType::Varchar,
        100
    };

    EXPECT_EQ(column.name(), "name");
    EXPECT_EQ(column.type(), DataType::Varchar);
    EXPECT_EQ(column.length(), 100);
}

TEST(ColumnTest, CalculatesBooleanStorageSize)
{
    const Column column{
        "active",
        DataType::Boolean
    };

    EXPECT_EQ(column.storageSize(), sizeof(bool));
}

TEST(ColumnTest, CalculatesInt32StorageSize)
{
    const Column column{
        "id",
        DataType::Int32
    };

    EXPECT_EQ(
        column.storageSize(),
        sizeof(std::int32_t)
    );
}

TEST(ColumnTest, CalculatesInt64StorageSize)
{
    const Column column{
        "id",
        DataType::Int64
    };

    EXPECT_EQ(
        column.storageSize(),
        sizeof(std::int64_t)
    );
}

TEST(ColumnTest, CalculatesFloatStorageSize)
{
    const Column column{
        "score",
        DataType::Float
    };

    EXPECT_EQ(
        column.storageSize(),
        sizeof(float)
    );
}

TEST(ColumnTest, CalculatesDoubleStorageSize)
{
    const Column column{
        "salary",
        DataType::Double
    };

    EXPECT_EQ(
        column.storageSize(),
        sizeof(double)
    );
}

TEST(ColumnTest, CalculatesVarcharStorageSize)
{
    const Column column{
        "name",
        DataType::Varchar,
        255
    };

    EXPECT_EQ(column.storageSize(), 255);
}

TEST(ColumnTest, RejectsEmptyColumnName)
{
    EXPECT_THROW(
        Column{"", DataType::Int32},
        std::invalid_argument
    );
}

TEST(ColumnTest, RejectsVarcharWithoutLength)
{
    EXPECT_THROW(
        Column{"name", DataType::Varchar},
        std::invalid_argument
    );
}

TEST(ColumnTest, RejectsLengthForNonVarchar)
{
    EXPECT_THROW(
        Column{"id", DataType::Int32, 10},
        std::invalid_argument
    );
}

} // namespace forgedb