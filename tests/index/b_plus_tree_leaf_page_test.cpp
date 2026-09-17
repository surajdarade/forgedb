#include <cstdint>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "forgedb/index/b_plus_tree_leaf_page.h"
#include "forgedb/record/record_id.h"
#include "forgedb/storage/page.h"

namespace forgedb {

namespace {

RecordId rid(
    std::uint64_t page,
    std::uint32_t slot)
{
    return RecordId{
        PageId{page},
        slot
    };
}

} // namespace

TEST(BPlusTreeLeafPageTest, InitializesAsEmptyLeaf)
{
    Page page{PageId{10}};

    BPlusTreeLeafPage leaf{page};

    EXPECT_EQ(
        leaf.pageType(),
        IndexPageType::Leaf
    );

    EXPECT_EQ(leaf.size(), 0U);
    EXPECT_TRUE(leaf.isEmpty());

    EXPECT_EQ(
        leaf.parentPageId(),
        PageId{}
    );

    EXPECT_EQ(
        leaf.nextPageId(),
        PageId{}
    );
}

TEST(BPlusTreeLeafPageTest, InsertsEntry)
{
    Page page{PageId{10}};

    BPlusTreeLeafPage leaf{page};

    const IndexKey key{
        std::int32_t{42}
    };

    const RecordId recordId = rid(1, 0);

    EXPECT_TRUE(
        leaf.insert(key, recordId)
    );

    EXPECT_EQ(leaf.size(), 1U);
    EXPECT_EQ(leaf.keyAt(0), key);
    EXPECT_EQ(leaf.recordIdAt(0), recordId);
}

TEST(BPlusTreeLeafPageTest, MaintainsSortedOrder)
{
    Page page{PageId{10}};

    BPlusTreeLeafPage leaf{page};

    leaf.insert(
        IndexKey{std::int32_t{30}},
        rid(3, 0)
    );

    leaf.insert(
        IndexKey{std::int32_t{10}},
        rid(1, 0)
    );

    leaf.insert(
        IndexKey{std::int32_t{20}},
        rid(2, 0)
    );

    EXPECT_EQ(
        leaf.keyAt(0),
        IndexKey{std::int32_t{10}}
    );

    EXPECT_EQ(
        leaf.keyAt(1),
        IndexKey{std::int32_t{20}}
    );

    EXPECT_EQ(
        leaf.keyAt(2),
        IndexKey{std::int32_t{30}}
    );
}

TEST(BPlusTreeLeafPageTest, SupportsDuplicateKeys)
{
    Page page{PageId{10}};

    BPlusTreeLeafPage leaf{page};

    const IndexKey key{
        std::int32_t{42}
    };

    EXPECT_TRUE(
        leaf.insert(key, rid(1, 0))
    );

    EXPECT_TRUE(
        leaf.insert(key, rid(1, 1))
    );

    EXPECT_TRUE(
        leaf.insert(key, rid(2, 0))
    );

    EXPECT_EQ(leaf.size(), 3U);

    const auto result =
        leaf.lookup(key);

    ASSERT_EQ(result.size(), 3U);

    EXPECT_EQ(result[0], rid(1, 0));
    EXPECT_EQ(result[1], rid(1, 1));
    EXPECT_EQ(result[2], rid(2, 0));
}

TEST(BPlusTreeLeafPageTest, RejectsDuplicateEntry)
{
    Page page{PageId{10}};

    BPlusTreeLeafPage leaf{page};

    const IndexKey key{
        std::int32_t{42}
    };

    const RecordId recordId = rid(1, 0);

    EXPECT_TRUE(
        leaf.insert(key, recordId)
    );

    EXPECT_FALSE(
        leaf.insert(key, recordId)
    );

    EXPECT_EQ(leaf.size(), 1U);
}

TEST(BPlusTreeLeafPageTest, SupportsStringKeys)
{
    Page page{PageId{10}};

    BPlusTreeLeafPage leaf{page};

    const IndexKey key{
        std::string{"Alice"}
    };

    const RecordId recordId = rid(1, 0);

    EXPECT_TRUE(
        leaf.insert(key, recordId)
    );

    EXPECT_EQ(
        leaf.keyAt(0),
        key
    );

    EXPECT_EQ(
        leaf.lookup(key)[0],
        recordId
    );
}

TEST(BPlusTreeLeafPageTest, LowerBoundFindsFirstMatchingPosition)
{
    Page page{PageId{10}};

    BPlusTreeLeafPage leaf{page};

    leaf.insert(
        IndexKey{std::int32_t{10}},
        rid(1, 0)
    );

    leaf.insert(
        IndexKey{std::int32_t{20}},
        rid(2, 0)
    );

    leaf.insert(
        IndexKey{std::int32_t{30}},
        rid(3, 0)
    );

    EXPECT_EQ(
        leaf.lowerBound(
            IndexKey{std::int32_t{20}}
        ),
        1U
    );

    EXPECT_EQ(
        leaf.lowerBound(
            IndexKey{std::int32_t{25}}
        ),
        2U
    );

    EXPECT_EQ(
        leaf.lowerBound(
            IndexKey{std::int32_t{5}}
        ),
        0U
    );
}

TEST(BPlusTreeLeafPageTest, UpperBoundFindsPositionAfterDuplicates)
{
    Page page{PageId{10}};

    BPlusTreeLeafPage leaf{page};

    const IndexKey key{
        std::int32_t{20}
    };

    leaf.insert(key, rid(1, 0));
    leaf.insert(key, rid(1, 1));
    leaf.insert(
        IndexKey{std::int32_t{30}},
        rid(3, 0)
    );

    EXPECT_EQ(
        leaf.upperBound(key),
        2U
    );
}

TEST(BPlusTreeLeafPageTest, RemovesEntry)
{
    Page page{PageId{10}};

    BPlusTreeLeafPage leaf{page};

    const IndexKey key{
        std::int32_t{42}
    };

    const RecordId recordId = rid(1, 0);

    leaf.insert(key, recordId);

    EXPECT_TRUE(
        leaf.remove(key, recordId)
    );

    EXPECT_EQ(leaf.size(), 0U);

    EXPECT_TRUE(
        leaf.lookup(key).empty()
    );
}

TEST(BPlusTreeLeafPageTest, RemovingMissingEntryReturnsFalse)
{
    Page page{PageId{10}};

    BPlusTreeLeafPage leaf{page};

    EXPECT_FALSE(
        leaf.remove(
            IndexKey{std::int32_t{42}},
            rid(1, 0)
        )
    );
}

TEST(BPlusTreeLeafPageTest, MaintainsNextLeafPointer)
{
    Page page{PageId{10}};

    BPlusTreeLeafPage leaf{page};

    const PageId nextPage{25};

    leaf.setNextPageId(nextPage);

    EXPECT_EQ(
        leaf.nextPageId(),
        nextPage
    );
}

TEST(BPlusTreeLeafPageTest, DataSurvivesPageReconstruction)
{
    Page page{PageId{10}};

    {
        BPlusTreeLeafPage leaf{page};

        leaf.insert(
            IndexKey{std::int32_t{42}},
            rid(1, 0)
        );

        leaf.insert(
            IndexKey{std::string{"Alice"}},
            rid(2, 0)
        );

        leaf.setNextPageId(PageId{20});
    }

    BPlusTreeLeafPage reopened{page};

    EXPECT_EQ(reopened.size(), 2U);

    EXPECT_EQ(
        reopened.lookup(
            IndexKey{std::int32_t{42}}
        )[0],
        rid(1, 0)
    );

    EXPECT_EQ(
        reopened.lookup(
            IndexKey{std::string{"Alice"}}
        )[0],
        rid(2, 0)
    );

    EXPECT_EQ(
        reopened.nextPageId(),
        PageId{20}
    );
}

TEST(BPlusTreeLeafPageTest, ReportsFreeSpace)
{
    Page page{PageId{10}};

    BPlusTreeLeafPage leaf{page};

    const std::size_t initialSpace =
        leaf.freeSpace();

    leaf.insert(
        IndexKey{std::int32_t{42}},
        rid(1, 0)
    );

    EXPECT_LT(
        leaf.freeSpace(),
        initialSpace
    );
}

} // namespace forgedb