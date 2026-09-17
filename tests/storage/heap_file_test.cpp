#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "forgedb/buffer/buffer_pool_manager.h"
#include "forgedb/record/record_id.h"
#include "forgedb/storage/disk_manager.h"
#include "forgedb/storage/heap_file.h"

namespace forgedb {

namespace {

class HeapFileTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        databasePath_ =
            std::filesystem::temp_directory_path() /
            "forgedb_heap_file_test.db";

        std::filesystem::remove(databasePath_);

        diskManager_ =
            std::make_unique<DiskManager>(
                databasePath_.string()
            );

        bufferPoolManager_ =
            std::make_unique<BufferPoolManager>(
                3,
                *diskManager_
            );

        heapFile_ =
            std::make_unique<HeapFile>(
                *diskManager_,
                *bufferPoolManager_
            );
    }

    void TearDown() override
    {
        heapFile_.reset();
        bufferPoolManager_.reset();
        diskManager_.reset();

        std::filesystem::remove(databasePath_);
    }

    std::filesystem::path databasePath_;

    std::unique_ptr<DiskManager> diskManager_;
    std::unique_ptr<BufferPoolManager> bufferPoolManager_;
    std::unique_ptr<HeapFile> heapFile_;
};

} // namespace

TEST_F(HeapFileTest, InsertsAndReadsRecord)
{
    const std::vector<std::uint8_t> record{
        'F', 'o', 'r', 'g', 'e', 'D', 'B'
    };

    const RecordId recordId =
        heapFile_->insert(record);

    EXPECT_TRUE(recordId.isValid());
    EXPECT_EQ(recordId.pageId(), PageId{1});
    EXPECT_EQ(recordId.slot(), 0U);

    const auto restored =
        heapFile_->read(recordId);

    EXPECT_EQ(restored, record);
}

TEST_F(HeapFileTest, InsertsMultipleRecords)
{
    const std::vector<std::uint8_t> first{
        'A', 'B', 'C'
    };

    const std::vector<std::uint8_t> second{
        'D', 'E', 'F'
    };

    const RecordId firstId =
        heapFile_->insert(first);

    const RecordId secondId =
        heapFile_->insert(second);

    EXPECT_NE(firstId, secondId);

    EXPECT_EQ(
        heapFile_->read(firstId),
        first
    );

    EXPECT_EQ(
        heapFile_->read(secondId),
        second
    );
}

TEST_F(HeapFileTest, StoresLargeRecordsAcrossPages)
{
    const std::vector<std::uint8_t> record(
        2000,
        42
    );

    std::vector<RecordId> recordIds;

    for (int i = 0; i < 5; ++i) {
        recordIds.push_back(
            heapFile_->insert(record)
        );
    }

    EXPECT_EQ(
        diskManager_->pageCount(),
        4U
    );

    for (const RecordId recordId : recordIds) {
        EXPECT_EQ(
            heapFile_->read(recordId),
            record
        );
    }
}

TEST_F(HeapFileTest, UpdatesRecord)
{
    const std::vector<std::uint8_t> original{
        'O', 'L', 'D'
    };

    const std::vector<std::uint8_t> updated{
        'N', 'E', 'W'
    };

    const RecordId recordId =
        heapFile_->insert(original);

    heapFile_->update(
        recordId,
        updated
    );

    EXPECT_EQ(
        heapFile_->read(recordId),
        updated
    );
}

TEST_F(HeapFileTest, DeletesRecord)
{
    const std::vector<std::uint8_t> record{
        'A', 'B', 'C'
    };

    const RecordId recordId =
        heapFile_->insert(record);

    heapFile_->erase(recordId);

    EXPECT_THROW(
        heapFile_->read(recordId),
        std::runtime_error
    );
}

TEST_F(HeapFileTest, RejectsInvalidRecordId)
{
    const RecordId invalidId{};

    EXPECT_THROW(
        heapFile_->read(invalidId),
        std::invalid_argument
    );

    EXPECT_THROW(
        heapFile_->update(
            invalidId,
            std::vector<std::uint8_t>{'A'}
        ),
        std::invalid_argument
    );

    EXPECT_THROW(
        heapFile_->erase(invalidId),
        std::invalid_argument
    );
}

TEST_F(HeapFileTest, PersistsRecordsAfterFlush)
{
    const std::vector<std::uint8_t> record{
        'P', 'e', 'r', 's', 'i', 's', 't'
    };

    const RecordId recordId =
        heapFile_->insert(record);

    bufferPoolManager_->flushAllPages();
    diskManager_->flush();

    heapFile_.reset();
    bufferPoolManager_.reset();
    diskManager_.reset();

    diskManager_ =
        std::make_unique<DiskManager>(
            databasePath_.string()
        );

    bufferPoolManager_ =
        std::make_unique<BufferPoolManager>(
            3,
            *diskManager_
        );

    heapFile_ =
        std::make_unique<HeapFile>(
            *diskManager_,
            *bufferPoolManager_
        );

    EXPECT_EQ(
        heapFile_->read(recordId),
        record
    );
}

TEST_F(HeapFileTest, DifferentRecordsCanOccupySamePage)
{
    const std::vector<std::uint8_t> first(
        100,
        1
    );

    const std::vector<std::uint8_t> second(
        100,
        2
    );

    const RecordId firstId =
        heapFile_->insert(first);

    const RecordId secondId =
        heapFile_->insert(second);

    EXPECT_EQ(
        firstId.pageId(),
        secondId.pageId()
    );

    EXPECT_NE(
        firstId.slot(),
        secondId.slot()
    );
}

} // namespace forgedb