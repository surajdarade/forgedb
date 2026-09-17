#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "forgedb/record/tuple.h"
#include "forgedb/record/tuple_serializer.h"
#include "forgedb/record/value.h"

namespace forgedb {

TEST(TupleSerializerTest, SerializesAndDeserializesTuple)
{
    const Tuple original{
        {
            Value{std::int32_t{42}},
            Value{std::int64_t{9876543210LL}},
            Value{3.14F},
            Value{99.95},
            Value{true},
            Value{std::string{"ForgeDB"}}
        }
    };

    const auto bytes =
        TupleSerializer::serialize(original);

    const Tuple restored =
        TupleSerializer::deserialize(bytes);

    ASSERT_EQ(
        restored.size(),
        original.size()
    );

    EXPECT_EQ(
        restored.getValue(0).asInt32(),
        42
    );

    EXPECT_EQ(
        restored.getValue(1).asInt64(),
        9876543210LL
    );

    EXPECT_FLOAT_EQ(
        restored.getValue(2).asFloat(),
        3.14F
    );

    EXPECT_DOUBLE_EQ(
        restored.getValue(3).asDouble(),
        99.95
    );

    EXPECT_TRUE(
        restored.getValue(4).asBool()
    );

    EXPECT_EQ(
        restored.getValue(5).asString(),
        "ForgeDB"
    );
}

TEST(TupleSerializerTest, PreservesNullValues)
{
    const Tuple original{
        {
            Value{std::int32_t{42}},
            Value::null(),
            Value{std::string{"ForgeDB"}},
            Value::null()
        }
    };

    const auto bytes =
        TupleSerializer::serialize(original);

    const Tuple restored =
        TupleSerializer::deserialize(bytes);

    ASSERT_EQ(restored.size(), 4U);

    EXPECT_FALSE(restored.getValue(0).isNull());
    EXPECT_TRUE(restored.getValue(1).isNull());
    EXPECT_FALSE(restored.getValue(2).isNull());
    EXPECT_TRUE(restored.getValue(3).isNull());
}

TEST(TupleSerializerTest, SerializesEmptyTuple)
{
    const Tuple original{{}};

    const auto bytes =
        TupleSerializer::serialize(original);

    const Tuple restored =
        TupleSerializer::deserialize(bytes);

    EXPECT_TRUE(bytes.size() >= sizeof(std::uint32_t));
    EXPECT_EQ(restored.size(), 0U);
}

TEST(TupleSerializerTest, RejectsInvalidTypeTag)
{
    const std::vector<std::uint8_t> bytes{
        1, 0, 0, 0,
        255
    };

    EXPECT_THROW(
        TupleSerializer::deserialize(bytes),
        std::runtime_error
    );
}

TEST(TupleSerializerTest, RejectsTruncatedTuple)
{
    const std::vector<std::uint8_t> bytes{
        1, 0, 0, 0,
        1
    };

    EXPECT_THROW(
        TupleSerializer::deserialize(bytes),
        std::out_of_range
    );
}

TEST(TupleSerializerTest, RejectsTrailingBytes)
{
    const Tuple original{
        {
            Value{std::int32_t{42}}
        }
    };

    auto bytes =
        TupleSerializer::serialize(original);

    bytes.push_back(0xFF);

    EXPECT_THROW(
        TupleSerializer::deserialize(bytes),
        std::runtime_error
    );
}

} // namespace forgedb