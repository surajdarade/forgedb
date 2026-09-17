#include <cstdint>
#include <filesystem>
#include <string>

#include <gtest/gtest.h>

#include "forgedb/common/constants.h"
#include "forgedb/storage/disk_manager.h"
#include "forgedb/storage/page.h"

namespace forgedb {

class DiskManagerTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        databasePath_ =
            std::filesystem::temp_directory_path() /
            "forgedb_disk_manager_test.db";

        std::filesystem::remove(databasePath_);
    }

    void TearDown() override
    {
        std::filesystem::remove(databasePath_);
    }

    std::string databasePath_;
};

TEST_F(DiskManagerTest, CreatesDatabaseFile)
{
    DiskManager diskManager(databasePath_);

    EXPECT_TRUE(std::filesystem::exists(databasePath_));
    EXPECT_EQ(diskManager.pageCount(), 1);
}

TEST_F(DiskManagerTest, AllocatesPages)
{
    DiskManager diskManager(databasePath_);

    const PageId firstPage = diskManager.allocatePage();
    const PageId secondPage = diskManager.allocatePage();

    EXPECT_EQ(firstPage.value(), 1);
    EXPECT_EQ(secondPage.value(), 2);

    EXPECT_EQ(diskManager.pageCount(), 3);
}

TEST_F(DiskManagerTest, WritesAndReadsPage)
{
    DiskManager diskManager(databasePath_);

    const PageId pageId = diskManager.allocatePage();

    Page page{pageId};

    page.data()[0] = 42;
    page.data()[100] = 99;
    page.data()[kPageSize - 1] = 123;

    diskManager.writePage(pageId, page);

    Page loadedPage;

    diskManager.readPage(pageId, loadedPage);

    EXPECT_EQ(loadedPage.id(), pageId);
    EXPECT_EQ(loadedPage.data()[0], 42);
    EXPECT_EQ(loadedPage.data()[100], 99);
    EXPECT_EQ(loadedPage.data()[kPageSize - 1], 123);
}

TEST_F(DiskManagerTest, PreservesEntirePage)
{
    DiskManager diskManager(databasePath_);

    const PageId pageId = diskManager.allocatePage();

    Page page{pageId};

    for (std::size_t i = 0; i < page.size(); ++i) {
        page.data()[i] =
            static_cast<std::uint8_t>(i % 256);
    }

    diskManager.writePage(pageId, page);

    Page loadedPage;

    diskManager.readPage(pageId, loadedPage);

    EXPECT_EQ(loadedPage.id(), pageId);

    for (std::size_t i = 0; i < page.size(); ++i) {
        EXPECT_EQ(
            loadedPage.data()[i],
            static_cast<std::uint8_t>(i % 256)
        );
    }
}

TEST_F(DiskManagerTest, PersistsDataAfterReopening)
{
    PageId pageId;

    {
        DiskManager diskManager(databasePath_);

        pageId = diskManager.allocatePage();

        Page page{pageId};

        page.data()[0] = 42;
        page.data()[1] = 84;
        page.data()[2] = 126;

        diskManager.writePage(pageId, page);
    }

    {
        DiskManager diskManager(databasePath_);

        EXPECT_EQ(diskManager.pageCount(), 2);

        Page loadedPage;

        diskManager.readPage(pageId, loadedPage);

        EXPECT_EQ(loadedPage.id(), pageId);
        EXPECT_EQ(loadedPage.data()[0], 42);
        EXPECT_EQ(loadedPage.data()[1], 84);
        EXPECT_EQ(loadedPage.data()[2], 126);
    }
}

TEST_F(DiskManagerTest, CanReadMetadataPage)
{
    DiskManager diskManager(databasePath_);

    Page metadataPage;

    diskManager.readPage(PageId{0}, metadataPage);

    EXPECT_EQ(metadataPage.id(), PageId{0});

    for (const auto byte : metadataPage.data()) {
        EXPECT_EQ(byte, 0);
    }
}

TEST_F(DiskManagerTest, ThrowsWhenReadingNonexistentPage)
{
    DiskManager diskManager(databasePath_);

    Page page;

    EXPECT_THROW(
        diskManager.readPage(PageId{100}, page),
        std::out_of_range
    );
}

TEST_F(DiskManagerTest, FlushesSuccessfully)
{
    DiskManager diskManager(databasePath_);

    const PageId pageId = diskManager.allocatePage();

    Page page{pageId};

    page.data()[0] = 55;

    diskManager.writePage(pageId, page);

    EXPECT_NO_THROW(diskManager.flush());
}

} // namespace forgedb