#include <filesystem>
#include <gtest/gtest.h>
#include "forgedb/buffer/buffer_pool_manager.h"
#include "forgedb/index/persistent_b_plus_tree.h"
#include "forgedb/storage/disk_manager.h"
namespace forgedb {
TEST(PersistentBPlusTreeTest, PersistsInsertLookupAndDelete) {
    const auto path = std::filesystem::temp_directory_path() / "forgedb_persistent_index_test.db";
    std::filesystem::remove(path);
    PageId metadata;
    {
        DiskManager disk(path.string());
        BufferPoolManager pool(64, disk);
        PersistentBPlusTree tree(pool);
        metadata = tree.metadataPageId();
        for (std::int32_t i = 0; i < 200; ++i)
            ASSERT_TRUE(tree.insert(IndexKey{i}, RecordId{PageId{static_cast<std::uint64_t>(i + 10)}, 0}));
        ASSERT_EQ(tree.lookup(IndexKey{42}).size(), 1U);
        pool.flushAllPages();
    }
    {
        DiskManager disk(path.string());
        BufferPoolManager pool(64, disk);
        PersistentBPlusTree tree(pool, metadata);
        ASSERT_EQ(tree.size(), 200U);
        ASSERT_EQ(tree.scan(IndexKey{10}, IndexKey{19}).size(), 10U);
        for (std::int32_t i = 0; i < 100; ++i)
            ASSERT_TRUE(tree.remove(IndexKey{i}, RecordId{PageId{static_cast<std::uint64_t>(i + 10)}, 0}));
        EXPECT_EQ(tree.size(), 100U);
        pool.flushAllPages();
    }
    std::filesystem::remove(path);
}
}
