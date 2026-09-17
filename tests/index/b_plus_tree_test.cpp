#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "forgedb/index/b_plus_tree.h"
#include "forgedb/index/index_key.h"
#include "forgedb/record/record_id.h"

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

TEST(BPlusTreeTest, StartsEmpty)
{
    BPlusTree tree;

    EXPECT_EQ(tree.size(), 0U);
    EXPECT_FALSE(
        tree.contains(IndexKey{std::int32_t{42}})
    );

    EXPECT_TRUE(
        tree.lookup(IndexKey{std::int32_t{42}}).empty()
    );
}

TEST(BPlusTreeTest, InsertsAndLooksUpKey)
{
    BPlusTree tree;

    const RecordId recordId = rid(1, 0);

    EXPECT_TRUE(
        tree.insert(
            IndexKey{std::int32_t{42}},
            recordId
        )
    );

    EXPECT_EQ(tree.size(), 1U);
    EXPECT_TRUE(
        tree.contains(IndexKey{std::int32_t{42}})
    );

    const auto result =
        tree.lookup(IndexKey{std::int32_t{42}});

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result[0], recordId);
}

TEST(BPlusTreeTest, RejectsDuplicateKeyAndRecordId)
{
    BPlusTree tree;

    const IndexKey key{
        std::int32_t{42}
    };

    const RecordId recordId = rid(1, 0);

    EXPECT_TRUE(
        tree.insert(key, recordId)
    );

    EXPECT_FALSE(
        tree.insert(key, recordId)
    );

    EXPECT_EQ(tree.size(), 1U);
}

TEST(BPlusTreeTest, SupportsDuplicateKeys)
{
    BPlusTree tree;

    const IndexKey key{
        std::int32_t{42}
    };

    const RecordId first = rid(1, 0);
    const RecordId second = rid(1, 1);
    const RecordId third = rid(2, 0);

    EXPECT_TRUE(tree.insert(key, first));
    EXPECT_TRUE(tree.insert(key, second));
    EXPECT_TRUE(tree.insert(key, third));

    EXPECT_EQ(tree.size(), 3U);

    const auto result = tree.lookup(key);

    ASSERT_EQ(result.size(), 3U);

    EXPECT_EQ(result[0], first);
    EXPECT_EQ(result[1], second);
    EXPECT_EQ(result[2], third);
}

TEST(BPlusTreeTest, LooksUpMissingKey)
{
    BPlusTree tree;

    tree.insert(
        IndexKey{std::int32_t{10}},
        rid(1, 0)
    );

    EXPECT_TRUE(
        tree.lookup(
            IndexKey{std::int32_t{20}}
        ).empty()
    );
}

TEST(BPlusTreeTest, ContainsOnlyExistingKeys)
{
    BPlusTree tree;

    tree.insert(
        IndexKey{std::int32_t{10}},
        rid(1, 0)
    );

    tree.insert(
        IndexKey{std::int32_t{20}},
        rid(1, 1)
    );

    EXPECT_TRUE(
        tree.contains(
            IndexKey{std::int32_t{10}}
        )
    );

    EXPECT_TRUE(
        tree.contains(
            IndexKey{std::int32_t{20}}
        )
    );

    EXPECT_FALSE(
        tree.contains(
            IndexKey{std::int32_t{15}}
        )
    );
}

TEST(BPlusTreeTest, PerformsRangeScan)
{
    BPlusTree tree;

    for (std::int32_t key = 1; key <= 10; ++key) {
        tree.insert(
            IndexKey{key},
            rid(
                static_cast<std::uint64_t>(key),
                0
            )
        );
    }

    const auto result =
        tree.scan(
            IndexKey{std::int32_t{3}},
            IndexKey{std::int32_t{7}}
        );

    ASSERT_EQ(result.size(), 5U);

    EXPECT_EQ(result[0], rid(3, 0));
    EXPECT_EQ(result[1], rid(4, 0));
    EXPECT_EQ(result[2], rid(5, 0));
    EXPECT_EQ(result[3], rid(6, 0));
    EXPECT_EQ(result[4], rid(7, 0));
}

TEST(BPlusTreeTest, RangeScanIsInclusive)
{
    BPlusTree tree;

    for (std::int32_t key = 1; key <= 5; ++key) {
        tree.insert(
            IndexKey{key},
            rid(
                static_cast<std::uint64_t>(key),
                0
            )
        );
    }

    const auto result =
        tree.scan(
            IndexKey{std::int32_t{2}},
            IndexKey{std::int32_t{4}}
        );

    ASSERT_EQ(result.size(), 3U);

    EXPECT_EQ(result[0], rid(2, 0));
    EXPECT_EQ(result[1], rid(3, 0));
    EXPECT_EQ(result[2], rid(4, 0));
}

TEST(BPlusTreeTest, RangeScanAcrossLeafSplit)
{
    BPlusTree tree{3};

    for (std::int32_t key = 1; key <= 20; ++key) {
        EXPECT_TRUE(
            tree.insert(
                IndexKey{key},
                rid(
                    static_cast<std::uint64_t>(key),
                    0
                )
            )
        );
    }

    EXPECT_EQ(tree.size(), 20U);

    const auto result =
        tree.scan(
            IndexKey{std::int32_t{5}},
            IndexKey{std::int32_t{15}}
        );

    ASSERT_EQ(result.size(), 11U);

    for (std::size_t i = 0; i < result.size(); ++i) {
        EXPECT_EQ(
            result[i],
            rid(
                static_cast<std::uint64_t>(i + 5),
                0
            )
        );
    }
}

TEST(BPlusTreeTest, RemovesEntry)
{
    BPlusTree tree;

    const IndexKey key{
        std::int32_t{42}
    };

    const RecordId recordId = rid(1, 0);

    tree.insert(key, recordId);

    EXPECT_TRUE(
        tree.remove(key, recordId)
    );

    EXPECT_EQ(tree.size(), 0U);

    EXPECT_FALSE(
        tree.contains(key)
    );

    EXPECT_TRUE(
        tree.lookup(key).empty()
    );
}

TEST(BPlusTreeTest, RemovingMissingEntryReturnsFalse)
{
    BPlusTree tree;

    EXPECT_FALSE(
        tree.remove(
            IndexKey{std::int32_t{42}},
            rid(1, 0)
        )
    );
}

TEST(BPlusTreeTest, RemovesOneDuplicateEntry)
{
    BPlusTree tree;

    const IndexKey key{
        std::int32_t{42}
    };

    const RecordId first = rid(1, 0);
    const RecordId second = rid(1, 1);

    tree.insert(key, first);
    tree.insert(key, second);

    EXPECT_TRUE(
        tree.remove(key, first)
    );

    EXPECT_EQ(tree.size(), 1U);

    const auto result =
        tree.lookup(key);

    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result[0], second);
}

TEST(BPlusTreeTest, SupportsStringKeys)
{
    BPlusTree tree;

    const RecordId alice = rid(1, 0);
    const RecordId bob = rid(1, 1);

    tree.insert(
        IndexKey{std::string{"Alice"}},
        alice
    );

    tree.insert(
        IndexKey{std::string{"Bob"}},
        bob
    );

    EXPECT_EQ(
        tree.lookup(
            IndexKey{std::string{"Alice"}}
        )[0],
        alice
    );

    const auto result =
        tree.scan(
            IndexKey{std::string{"Alice"}},
            IndexKey{std::string{"Bob"}}
        );

    ASSERT_EQ(result.size(), 2U);

    EXPECT_EQ(result[0], alice);
    EXPECT_EQ(result[1], bob);
}

TEST(BPlusTreeTest, RejectsInvalidRange)
{
    BPlusTree tree;

    EXPECT_THROW(
        tree.scan(
            IndexKey{std::int32_t{20}},
            IndexKey{std::int32_t{10}}
        ),
        std::invalid_argument
    );
}

TEST(BPlusTreeTest, RejectsDifferentKeyTypesInRange)
{
    BPlusTree tree;

    EXPECT_THROW(
        tree.scan(
            IndexKey{std::int32_t{10}},
            IndexKey{std::string{"20"}}
        ),
        std::invalid_argument
    );
}

TEST(BPlusTreeTest, MaintainsSizeAfterManyOperations)
{
    BPlusTree tree{3};

    constexpr int kCount = 100;

    for (int i = 0; i < kCount; ++i) {
        EXPECT_TRUE(
            tree.insert(
                IndexKey{
                    static_cast<std::int32_t>(i)
                },
                rid(
                    static_cast<std::uint64_t>(i),
                    0
                )
            )
        );
    }

    EXPECT_EQ(
        tree.size(),
        static_cast<std::size_t>(kCount)
    );

    for (int i = 0; i < kCount; i += 2) {
        EXPECT_TRUE(
            tree.remove(
                IndexKey{
                    static_cast<std::int32_t>(i)
                },
                rid(
                    static_cast<std::uint64_t>(i),
                    0
                )
            )
        );
    }

    EXPECT_EQ(
        tree.size(),
        static_cast<std::size_t>(kCount / 2)
    );
}

} // namespace forgedb