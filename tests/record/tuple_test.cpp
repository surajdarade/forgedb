#include <cstdint>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "forgedb/record/record_id.h"
#include "forgedb/record/tuple.h"
#include "forgedb/record/value.h"
#include "forgedb/schema/column.h"
#include "forgedb/schema/data_type.h"
#include "forgedb/schema/schema.h"

namespace forgedb {

namespace {

Schema createTestSchema()
{
    return Schema{
        {
            Column{"id", DataType::Int32},
            Column{"name", DataType::Varchar, 100},
            Column{"salary", DataType::Double},
            Column{"active", DataType::Boolean}
        }
    };
}

} // namespace

TEST(TupleTest, StoresValues)
{
    Tuple tuple{
        {
            Value{std::int32_t{42}},
            Value{std::string{"Suraj"}},
            Value{95000.0},
            Value{true}
        }
    };

    EXPECT_EQ(tuple.size(), 4U);

    EXPECT_EQ(
        tuple.getValue(0).asInt32(),
        42
    );

    EXPECT_EQ(
        tuple.getValue(1).asString(),
        "Suraj"
    );

    EXPECT_DOUBLE_EQ(
        tuple.getValue(2).asDouble(),
        95000.0
    );

    EXPECT_TRUE(
        tuple.getValue(3).asBool()
    );
}

TEST(TupleTest, SupportsMutableValues)
{
    Tuple tuple{
        {
            Value{std::int32_t{42}},
            Value{std::string{"Suraj"}}
        }
    };

    tuple.getValue(0) = Value{std::int32_t{100}};
    tuple.getValue(1) = Value{std::string{"ForgeDB"}};

    EXPECT_EQ(
        tuple.getValue(0).asInt32(),
        100
    );

    EXPECT_EQ(
        tuple.getValue(1).asString(),
        "ForgeDB"
    );
}

TEST(TupleTest, ValuesCanBeAccessedThroughVector)
{
    Tuple tuple{
        {
            Value{std::int32_t{42}},
            Value{std::string{"Suraj"}}
        }
    };

    const auto& values = tuple.values();

    ASSERT_EQ(values.size(), 2U);
    EXPECT_EQ(values[0].asInt32(), 42);
    EXPECT_EQ(values[1].asString(), "Suraj");
}

TEST(TupleTest, RejectsOutOfRangeAccess)
{
    Tuple tuple{
        {
            Value{std::int32_t{42}}
        }
    };

    EXPECT_THROW(
        tuple.getValue(1),
        std::out_of_range
    );

    const Tuple& constTuple = tuple;

    EXPECT_THROW(
        constTuple.getValue(1),
        std::out_of_range
    );
}

TEST(TupleTest, StoresRecordId)
{
    const RecordId recordId{
        PageId{5},
        2
    };

    Tuple tuple{
        {
            Value{std::int32_t{42}}
        },
        recordId
    };

    EXPECT_EQ(
        tuple.recordId(),
        recordId
    );
}

TEST(TupleTest, RecordIdCanBeChanged)
{
    Tuple tuple{
        {
            Value{std::int32_t{42}}
        }
    };

    const RecordId recordId{
        PageId{10},
        7
    };

    tuple.setRecordId(recordId);

    EXPECT_EQ(
        tuple.recordId(),
        recordId
    );
}

TEST(TupleTest, MatchesSchema)
{
    const Schema schema = createTestSchema();

    Tuple tuple{
        {
            Value{std::int32_t{42}},
            Value{std::string{"Suraj"}},
            Value{95000.0},
            Value{true}
        }
    };

    EXPECT_TRUE(
        tuple.matchesSchema(schema)
    );
}

TEST(TupleTest, RejectsDifferentColumnCount)
{
    const Schema schema = createTestSchema();

    Tuple tuple{
        {
            Value{std::int32_t{42}},
            Value{std::string{"Suraj"}}
        }
    };

    EXPECT_FALSE(
        tuple.matchesSchema(schema)
    );
}

TEST(TupleTest, RejectsWrongValueType)
{
    const Schema schema = createTestSchema();

    Tuple tuple{
        {
            Value{std::int64_t{42}},
            Value{std::string{"Suraj"}},
            Value{95000.0},
            Value{true}
        }
    };

    EXPECT_FALSE(
        tuple.matchesSchema(schema)
    );
}

TEST(TupleTest, AllowsNullValues)
{
    const Schema schema = createTestSchema();

    Tuple tuple{
        {
            Value{std::int32_t{42}},
            Value::null(),
            Value{95000.0},
            Value::null()
        }
    };

    EXPECT_TRUE(
        tuple.matchesSchema(schema)
    );

    EXPECT_TRUE(tuple.getValue(1).isNull());
    EXPECT_TRUE(tuple.getValue(3).isNull());
}

TEST(TupleTest, EmptyTupleIsValidForEmptySchema)
{
    const Schema schema{{}};

    const Tuple tuple{{}};

    EXPECT_EQ(tuple.size(), 0U);
    EXPECT_TRUE(tuple.matchesSchema(schema));
}

} // namespace forgedb