#include <cstdint>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>

#include <gtest/gtest.h>

#include "forgedb/buffer/buffer_pool_manager.h"
#include "forgedb/record/table.h"
#include "forgedb/storage/disk_manager.h"
#include "forgedb/storage/heap_file.h"

namespace forgedb {

namespace {

class TableTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        databasePath_ =
            std::filesystem::temp_directory_path() /
            "forgedb_table_test.db";

        std::filesystem::remove(databasePath_);

        diskManager_ =
            std::make_unique<DiskManager>(
                databasePath_.string()
            );

        bufferPoolManager_ =
            std::make_unique<BufferPoolManager>(
                5,
                *diskManager_
            );

        heapFile_ =
            std::make_unique<HeapFile>(
                *diskManager_,
                *bufferPoolManager_
            );

        table_ =
            std::make_unique<Table>(
                createSchema(),
                *heapFile_
            );
    }

    void TearDown() override
    {
        table_.reset();
        heapFile_.reset();
        bufferPoolManager_.reset();
        diskManager_.reset();

        std::filesystem::remove(databasePath_);
    }

    static Schema createSchema()
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

    std::filesystem::path databasePath_;

    std::unique_ptr<DiskManager> diskManager_;
    std::unique_ptr<BufferPoolManager> bufferPoolManager_;
    std::unique_ptr<HeapFile> heapFile_;
    std::unique_ptr<Table> table_;
};

} // namespace

TEST_F(TableTest, ExposesSchema)
{
    EXPECT_EQ(
        table_->schema().columnCount(),
        4U
    );

    EXPECT_EQ(
        table_->schema().column(0).name(),
        "id"
    );

    EXPECT_EQ(
        table_->schema().column(1).name(),
        "name"
    );
}

TEST_F(TableTest, InsertsAndRetrievesTuple)
{
    const Tuple original{
        {
            Value{std::int32_t{42}},
            Value{std::string{"Suraj"}},
            Value{95000.0},
            Value{true}
        }
    };

    const RecordId recordId =
        table_->insert(original);

    EXPECT_TRUE(recordId.isValid());

    const Tuple restored =
        table_->get(recordId);

    ASSERT_EQ(restored.size(), 4U);

    EXPECT_EQ(
        restored.getValue(0).asInt32(),
        42
    );

    EXPECT_EQ(
        restored.getValue(1).asString(),
        "Suraj"
    );

    EXPECT_DOUBLE_EQ(
        restored.getValue(2).asDouble(),
        95000.0
    );

    EXPECT_TRUE(
        restored.getValue(3).asBool()
    );

    EXPECT_EQ(
        restored.recordId(),
        recordId
    );
}

TEST_F(TableTest, InsertsTupleContainingNullValues)
{
    const Tuple tuple{
        {
            Value{std::int32_t{42}},
            Value::null(),
            Value{95000.0},
            Value::null()
        }
    };

    const RecordId recordId =
        table_->insert(tuple);

    const Tuple restored =
        table_->get(recordId);

    EXPECT_TRUE(
        restored.getValue(1).isNull()
    );

    EXPECT_TRUE(
        restored.getValue(3).isNull()
    );
}

TEST_F(TableTest, RejectsTupleWithWrongColumnCount)
{
    const Tuple tuple{
        {
            Value{std::int32_t{42}}
        }
    };

    EXPECT_THROW(
        table_->insert(tuple),
        std::invalid_argument
    );
}

TEST_F(TableTest, RejectsTupleWithWrongType)
{
    const Tuple tuple{
        {
            Value{std::int64_t{42}},
            Value{std::string{"Suraj"}},
            Value{95000.0},
            Value{true}
        }
    };

    EXPECT_THROW(
        table_->insert(tuple),
        std::invalid_argument
    );
}

TEST_F(TableTest, UpdatesTuple)
{
    const Tuple original{
        {
            Value{std::int32_t{42}},
            Value{std::string{"Suraj"}},
            Value{95000.0},
            Value{true}
        }
    };

    const RecordId recordId =
        table_->insert(original);

    const Tuple updated{
        {
            Value{std::int32_t{42}},
            Value{std::string{"ForgeDB"}},
            Value{100000.0},
            Value{false}
        }
    };

    table_->update(
        recordId,
        updated
    );

    const Tuple restored =
        table_->get(recordId);

    EXPECT_EQ(
        restored.getValue(1).asString(),
        "ForgeDB"
    );

    EXPECT_DOUBLE_EQ(
        restored.getValue(2).asDouble(),
        100000.0
    );

    EXPECT_FALSE(
        restored.getValue(3).asBool()
    );
}

TEST_F(TableTest, DeletesTuple)
{
    const Tuple tuple{
        {
            Value{std::int32_t{42}},
            Value{std::string{"Suraj"}},
            Value{95000.0},
            Value{true}
        }
    };

    const RecordId recordId =
        table_->insert(tuple);

    table_->remove(recordId);

    EXPECT_THROW(
        table_->get(recordId),
        std::runtime_error
    );
}

TEST_F(TableTest, RejectsInvalidUpdatedTuple)
{
    const Tuple original{
        {
            Value{std::int32_t{42}},
            Value{std::string{"Suraj"}},
            Value{95000.0},
            Value{true}
        }
    };

    const RecordId recordId =
        table_->insert(original);

    const Tuple invalid{
        {
            Value{std::int64_t{42}},
            Value{std::string{"Suraj"}},
            Value{95000.0},
            Value{true}
        }
    };

    EXPECT_THROW(
        table_->update(recordId, invalid),
        std::invalid_argument
    );
}

} // namespace forgedb