#include <filesystem>
#include <stdexcept>
#include <string>

#include <gtest/gtest.h>

#include "forgedb/buffer/buffer_pool_manager.h"
#include "forgedb/storage/disk_manager.h"

namespace forgedb {

class BufferPoolManagerTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        databasePath_ =
            std::filesystem::temp_directory_path() /
            "forgedb_buffer_pool_test.db";

        std::filesystem::remove(databasePath_);
    }

    void TearDown() override
    {
        std::filesystem::remove(databasePath_);
    }

    std::string databasePath_;
};

TEST_F(BufferPoolManagerTest, CreatesPoolWithExpectedSize)
{
    DiskManager diskManager(databasePath_);
    BufferPoolManager bufferPool(3, diskManager);

    EXPECT_EQ(bufferPool.poolSize(), 3);
}

TEST_F(BufferPoolManagerTest, FetchPageLoadsPageIntoBuffer)
{
    DiskManager diskManager(databasePath_);

    const PageId pageId = diskManager.allocatePage();

    Page page{pageId};
    page.data()[0] = 42;

    diskManager.writePage(pageId, page);

    BufferPoolManager bufferPool(2, diskManager);

    Page* fetchedPage = bufferPool.fetchPage(pageId);

    ASSERT_NE(fetchedPage, nullptr);
    EXPECT_EQ(fetchedPage->id(), pageId);
    EXPECT_EQ(fetchedPage->data()[0], 42);

    EXPECT_TRUE(
        bufferPool.unpinPage(pageId, false)
    );
}

TEST_F(BufferPoolManagerTest, FetchingSamePageReturnsSameFrame)
{
    DiskManager diskManager(databasePath_);

    const PageId pageId = diskManager.allocatePage();

    BufferPoolManager bufferPool(2, diskManager);

    Page* first = bufferPool.fetchPage(pageId);
    Page* second = bufferPool.fetchPage(pageId);

    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);

    EXPECT_EQ(first, second);

    EXPECT_TRUE(bufferPool.unpinPage(pageId, false));
    EXPECT_TRUE(bufferPool.unpinPage(pageId, false));
}

TEST_F(BufferPoolManagerTest, NewPageAllocatesAndPinsPage)
{
    DiskManager diskManager(databasePath_);

    BufferPoolManager bufferPool(2, diskManager);

    PageId pageId;

    Page* page = bufferPool.newPage(pageId);

    ASSERT_NE(page, nullptr);

    EXPECT_EQ(page->id(), pageId);
    EXPECT_EQ(pageId.value(), 1);

    EXPECT_TRUE(
        bufferPool.unpinPage(pageId, false)
    );
}

TEST_F(BufferPoolManagerTest, NewPageCanBeModified)
{
    DiskManager diskManager(databasePath_);

    BufferPoolManager bufferPool(2, diskManager);

    PageId pageId;

    Page* page = bufferPool.newPage(pageId);

    ASSERT_NE(page, nullptr);

    page->data()[0] = 123;
    page->data()[4095] = 99;

    EXPECT_TRUE(
        bufferPool.unpinPage(pageId, true)
    );

    EXPECT_TRUE(
        bufferPool.flushPage(pageId)
    );

    Page loadedPage;

    diskManager.readPage(pageId, loadedPage);

    EXPECT_EQ(loadedPage.data()[0], 123);
    EXPECT_EQ(loadedPage.data()[4095], 99);
}

TEST_F(BufferPoolManagerTest, DirtyPageIsWrittenDuringEviction)
{
    DiskManager diskManager(databasePath_);

    BufferPoolManager bufferPool(1, diskManager);

    PageId firstPageId;

    Page* firstPage = bufferPool.newPage(firstPageId);

    ASSERT_NE(firstPage, nullptr);

    firstPage->data()[0] = 42;

    EXPECT_TRUE(
        bufferPool.unpinPage(firstPageId, true)
    );

    // The only frame is now available for eviction.
    const PageId secondPageId = diskManager.allocatePage();

    Page* secondPage = bufferPool.fetchPage(secondPageId);

    ASSERT_NE(secondPage, nullptr);

    Page loadedFirstPage;

    diskManager.readPage(firstPageId, loadedFirstPage);

    EXPECT_EQ(loadedFirstPage.data()[0], 42);

    EXPECT_TRUE(
        bufferPool.unpinPage(secondPageId, false)
    );
}

TEST_F(BufferPoolManagerTest, CannotEvictPinnedPage)
{
    DiskManager diskManager(databasePath_);

    BufferPoolManager bufferPool(1, diskManager);

    PageId firstPageId;

    Page* firstPage = bufferPool.newPage(firstPageId);

    ASSERT_NE(firstPage, nullptr);

    // First page remains pinned.

    PageId secondPageId;

    Page* secondPage = bufferPool.newPage(secondPageId);

    EXPECT_EQ(secondPage, nullptr);

    EXPECT_TRUE(
        bufferPool.unpinPage(firstPageId, false)
    );
}

TEST_F(BufferPoolManagerTest, UnpinFailsForUnknownPage)
{
    DiskManager diskManager(databasePath_);

    BufferPoolManager bufferPool(2, diskManager);

    EXPECT_FALSE(
        bufferPool.unpinPage(PageId{999}, false)
    );
}

TEST_F(BufferPoolManagerTest, UnpinFailsWhenPageIsNotPinned)
{
    DiskManager diskManager(databasePath_);

    BufferPoolManager bufferPool(2, diskManager);

    const PageId pageId = diskManager.allocatePage();

    Page* page = bufferPool.fetchPage(pageId);

    ASSERT_NE(page, nullptr);

    EXPECT_TRUE(
        bufferPool.unpinPage(pageId, false)
    );

    EXPECT_FALSE(
        bufferPool.unpinPage(pageId, false)
    );
}

TEST_F(BufferPoolManagerTest, DeleteUnpinnedPage)
{
    DiskManager diskManager(databasePath_);

    BufferPoolManager bufferPool(2, diskManager);

    const PageId pageId = diskManager.allocatePage();

    Page* page = bufferPool.fetchPage(pageId);

    ASSERT_NE(page, nullptr);

    EXPECT_TRUE(
        bufferPool.unpinPage(pageId, false)
    );

    EXPECT_TRUE(
        bufferPool.deletePage(pageId)
    );

    EXPECT_EQ(
        bufferPool.fetchPage(pageId),
        nullptr
    );
}

TEST_F(BufferPoolManagerTest, CannotDeletePinnedPage)
{
    DiskManager diskManager(databasePath_);

    BufferPoolManager bufferPool(2, diskManager);

    const PageId pageId = diskManager.allocatePage();

    Page* page = bufferPool.fetchPage(pageId);

    ASSERT_NE(page, nullptr);

    EXPECT_FALSE(
        bufferPool.deletePage(pageId)
    );

    EXPECT_TRUE(
        bufferPool.unpinPage(pageId, false)
    );
}

TEST_F(BufferPoolManagerTest, FlushPageWritesDirtyData)
{
    DiskManager diskManager(databasePath_);

    BufferPoolManager bufferPool(2, diskManager);

    const PageId pageId = diskManager.allocatePage();

    Page* page = bufferPool.fetchPage(pageId);

    ASSERT_NE(page, nullptr);

    page->data()[100] = 77;

    EXPECT_TRUE(
        bufferPool.unpinPage(pageId, true)
    );

    EXPECT_TRUE(
        bufferPool.flushPage(pageId)
    );

    Page loadedPage;

    diskManager.readPage(pageId, loadedPage);

    EXPECT_EQ(loadedPage.data()[100], 77);
}

TEST_F(BufferPoolManagerTest, FlushAllPagesWritesDirtyPages)
{
    DiskManager diskManager(databasePath_);

    BufferPoolManager bufferPool(3, diskManager);

    const PageId firstPageId = diskManager.allocatePage();
    const PageId secondPageId = diskManager.allocatePage();

    Page* firstPage = bufferPool.fetchPage(firstPageId);
    Page* secondPage = bufferPool.fetchPage(secondPageId);

    ASSERT_NE(firstPage, nullptr);
    ASSERT_NE(secondPage, nullptr);

    firstPage->data()[0] = 11;
    secondPage->data()[0] = 22;

    EXPECT_TRUE(
        bufferPool.unpinPage(firstPageId, true)
    );

    EXPECT_TRUE(
        bufferPool.unpinPage(secondPageId, true)
    );

    bufferPool.flushAllPages();

    Page loadedFirst;
    Page loadedSecond;

    diskManager.readPage(firstPageId, loadedFirst);
    diskManager.readPage(secondPageId, loadedSecond);

    EXPECT_EQ(loadedFirst.data()[0], 11);
    EXPECT_EQ(loadedSecond.data()[0], 22);
}

TEST_F(BufferPoolManagerTest, DataPersistsAfterBufferPoolRecreation)
{
    PageId pageId;

    {
        DiskManager diskManager(databasePath_);
        BufferPoolManager bufferPool(2, diskManager);

        Page* page = bufferPool.newPage(pageId);

        ASSERT_NE(page, nullptr);

        page->data()[0] = 55;
        page->data()[1] = 66;

        EXPECT_TRUE(
            bufferPool.unpinPage(pageId, true)
        );
    }

    {
        DiskManager diskManager(databasePath_);
        BufferPoolManager bufferPool(2, diskManager);

        Page* page = bufferPool.fetchPage(pageId);

        ASSERT_NE(page, nullptr);

        EXPECT_EQ(page->data()[0], 55);
        EXPECT_EQ(page->data()[1], 66);

        EXPECT_TRUE(
            bufferPool.unpinPage(pageId, false)
        );
    }
}

} // namespace forgedb