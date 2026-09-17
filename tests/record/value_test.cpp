#include <cstdint>
#include <stdexcept>
#include <string>

#include <gtest/gtest.h>

#include "forgedb/record/value.h"
#include "forgedb/schema/data_type.h"

namespace forgedb {

TEST(ValueTest, DefaultValueIsNull)
{
    const Value value;

    EXPECT_TRUE(value.isNull());
}

TEST(ValueTest, NullFactoryCreatesNullValue)
{
    const Value value = Value::null();

    EXPECT_TRUE(value.isNull());
}

TEST(ValueTest, StoresInt32)
{
    const Value value{std::int32_t{42}};

    EXPECT_FALSE(value.isNull());
    EXPECT_EQ(value.type(), DataType::Int32);
    EXPECT_EQ(value.asInt32(), 42);
}

TEST(ValueTest, StoresInt64)
{
    const Value value{std::int64_t{9876543210LL}};

    EXPECT_FALSE(value.isNull());
    EXPECT_EQ(value.type(), DataType::Int64);
    EXPECT_EQ(value.asInt64(), 9876543210LL);
}

TEST(ValueTest, StoresFloat)
{
    const Value value{3.14F};

    EXPECT_FALSE(value.isNull());
    EXPECT_EQ(value.type(), DataType::Float);
    EXPECT_FLOAT_EQ(value.asFloat(), 3.14F);
}

TEST(ValueTest, StoresDouble)
{
    const Value value{3.141592653589793};

    EXPECT_FALSE(value.isNull());
    EXPECT_EQ(value.type(), DataType::Double);
    EXPECT_DOUBLE_EQ(value.asDouble(), 3.141592653589793);
}

TEST(ValueTest, StoresBoolean)
{
    const Value trueValue{true};
    const Value falseValue{false};

    EXPECT_EQ(trueValue.type(), DataType::Boolean);
    EXPECT_TRUE(trueValue.asBool());

    EXPECT_EQ(falseValue.type(), DataType::Boolean);
    EXPECT_FALSE(falseValue.asBool());
}

TEST(ValueTest, StoresString)
{
    const Value value{std::string{"ForgeDB"}};

    EXPECT_FALSE(value.isNull());
    EXPECT_EQ(value.type(), DataType::Varchar);
    EXPECT_EQ(value.asString(), "ForgeDB");
}

TEST(ValueTest, StoresCString)
{
    const Value value{"ForgeDB"};

    EXPECT_EQ(value.type(), DataType::Varchar);
    EXPECT_EQ(value.asString(), "ForgeDB");
}

TEST(ValueTest, RejectsNullCString)
{
    EXPECT_THROW(
        Value{static_cast<const char*>(nullptr)},
        std::invalid_argument
    );
}

TEST(ValueTest, NullHasNoDataType)
{
    const Value value = Value::null();

    EXPECT_THROW(
        value.type(),
        std::logic_error
    );
}

TEST(ValueTest, WrongAccessorThrows)
{
    const Value value{std::int32_t{42}};

    EXPECT_THROW(
        value.asString(),
        std::bad_variant_access
    );

    EXPECT_THROW(
        value.asDouble(),
        std::bad_variant_access
    );
}

TEST(ValueTest, EqualityWorks)
{
    const Value first{std::int32_t{42}};
    const Value second{std::int32_t{42}};
    const Value third{std::int32_t{100}};

    EXPECT_EQ(first, second);
    EXPECT_NE(first, third);
}

TEST(ValueTest, ValuesOfDifferentTypesAreNotEqual)
{
    const Value intValue{std::int32_t{42}};
    const Value doubleValue{42.0};

    EXPECT_NE(intValue, doubleValue);
}

TEST(ValueTest, NullValuesAreEqual)
{
    const Value first = Value::null();
    const Value second = Value::null();

    EXPECT_EQ(first, second);
}

TEST(ValueTest, StorageIsAccessible)
{
    const Value value{std::int64_t{123456}};

    const auto& storage = value.storage();

    EXPECT_TRUE(
        std::holds_alternative<std::int64_t>(storage)
    );

    EXPECT_EQ(
        std::get<std::int64_t>(storage),
        123456
    );
}

} // namespace forgedb