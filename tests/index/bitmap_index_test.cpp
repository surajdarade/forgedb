#include <gtest/gtest.h>
#include "forgedb/index/bitmap_index.h"
namespace forgedb {
TEST(BitmapIndexTest, SupportsLargeSparsePageIds) {
    BitmapIndex index;
    RecordId id{PageId{0xFFFFFFFFFFULL}, 17};
    EXPECT_TRUE(index.insert(IndexKey{std::int64_t{42}}, id));
    EXPECT_EQ(index.lookup(IndexKey{std::int64_t{42}}).at(0), id);
    EXPECT_TRUE(index.remove(IndexKey{std::int64_t{42}}, id));
    EXPECT_TRUE(index.lookup(IndexKey{std::int64_t{42}}).empty());
}
}
