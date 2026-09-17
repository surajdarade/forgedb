#include <cstdint>
#include <stdexcept>
#include <string>

#include <gtest/gtest.h>

#include "forgedb/index/index_key.h"
#include "forgedb/record/value.h"
#include "forgedb/schema/data_type.h"

namespace forgedb {

TEST(IndexKeyTest, StoresInt32)
{
    const IndexKey key{std::int32_t{42}};

    EXPECT_EQ(key.type(), DataType::Int32);
    EXPECT_TRUE(key.isNumeric());

    EXPECT_EQ(
        std::get<std::int32_t>(key.storage()),
        42
    );
}

TEST(IndexKeyTest, StoresInt64)
{
    const IndexKey key{std::int64_t{9876543210LL}};

    EXPECT_EQ(key.type(), DataType::Int64);
    EXPECT_TRUE(key.isNumeric());
}

TEST(IndexKeyTest, StoresFloat)
{
    const IndexKey key{3.14F};

    EXPECT_EQ(key.type(), DataType::Float);
    EXPECT_TRUE(key.isNumeric());
}

TEST(IndexKeyTest, StoresDouble)
{
    const IndexKey key{99.95};

    EXPECT_EQ(key.type(), DataType::Double);
    EXPECT_TRUE(key.isNumeric());
}

TEST(IndexKeyTest, StoresString)
{
    const IndexKey key{
        std::string{"ForgeDB"}
    };

    EXPECT_EQ(key.type(), DataType::Varchar);
    EXPECT_FALSE(key.isNumeric());

    EXPECT_EQ(
        std::get<std::string>(key.storage()),
        "ForgeDB"
    );
}

TEST(IndexKeyTest, CanBeCreatedFromValue)
{
    const Value value{
        std::int32_t{42}
    };

    const IndexKey key{value};

    EXPECT_EQ(key.type(), DataType::Int32);

    EXPECT_EQ(
        std::get<std::int32_t>(key.storage()),
        42
    );
}

TEST(IndexKeyTest, RejectsNullValue)
{
    const Value value = Value::null();

    EXPECT_THROW(
        IndexKey{value},
        std::invalid_argument
    );
}

TEST(IndexKeyTest, RejectsBooleanValue)
{
    const Value value{true};

    EXPECT_THROW(
        IndexKey{value},
        std::invalid_argument
    );
}

TEST(IndexKeyTest, ComparesEqualKeys)
{
    const IndexKey first{
        std::int32_t{42}
    };

    const IndexKey second{
        std::int32_t{42}
    };

    EXPECT_EQ(first, second);
}

TEST(IndexKeyTest, ComparesDifferentKeys)
{
    const IndexKey first{
        std::int32_t{10}
    };

    const IndexKey second{
        std::int32_t{20}
    };

    EXPECT_LT(first, second);
    EXPECT_GT(second, first);
    EXPECT_LE(first, second);
    EXPECT_GE(second, first);
}

TEST(IndexKeyTest, ComparesStrings)
{
    const IndexKey first{
        std::string{"Alice"}
    };

    const IndexKey second{
        std::string{"Bob"}
    };

    EXPECT_LT(first, second);
}

TEST(IndexKeyTest, RejectsDifferentTypes)
{
    const IndexKey intKey{
        std::int32_t{42}
    };

    const IndexKey stringKey{
        std::string{"42"}
    };

    EXPECT_THROW(
        intKey < stringKey,
        std::invalid_argument
    );
}

TEST(IndexKeyTest, InequalityWorks)
{
    const IndexKey first{
        std::int32_t{10}
    };

    const IndexKey second{
        std::int32_t{20}
    };

    EXPECT_NE(first, second);
}

} // namespace forgedb