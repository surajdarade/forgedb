#include <cstdint>
#include <string>

#include <gtest/gtest.h>

#include "forgedb/index/b_plus_tree_internal_page.h"
#include "forgedb/storage/page.h"

namespace forgedb {

TEST(BPlusTreeInternalPageTest, InitializesAsInternalPage)
{
    Page page{PageId{10}};

    BPlusTreeInternalPage internal{page};

    EXPECT_EQ(
        internal.pageType(),
        IndexPageType::Internal
    );

    EXPECT_EQ(internal.size(), 0U);
    EXPECT_EQ(internal.childCount(), 1U);
}

TEST(BPlusTreeInternalPageTest, StoresFirstChild)
{
    Page page{PageId{10}};

    BPlusTreeInternalPage internal{page};

    internal.setFirstChild(PageId{100});

    EXPECT_EQ(
        internal.childAt(0),
        PageId{100}
    );
}

TEST(BPlusTreeInternalPageTest, InsertsChildrenAndKeys)
{
    Page page{PageId{10}};

    BPlusTreeInternalPage internal{page};

    internal.setFirstChild(PageId{100});

    internal.insertChild(
        1,
        IndexKey{std::int32_t{20}},
        PageId{200}
    );

    internal.insertChild(
        2,
        IndexKey{std::int32_t{40}},
        PageId{300}
    );

    EXPECT_EQ(internal.size(), 2U);
    EXPECT_EQ(internal.childCount(), 3U);

    EXPECT_EQ(
        internal.keyAt(0),
        IndexKey{std::int32_t{20}}
    );

    EXPECT_EQ(
        internal.keyAt(1),
        IndexKey{std::int32_t{40}}
    );

    EXPECT_EQ(
        internal.childAt(0),
        PageId{100}
    );

    EXPECT_EQ(
        internal.childAt(1),
        PageId{200}
    );

    EXPECT_EQ(
        internal.childAt(2),
        PageId{300}
    );
}

TEST(BPlusTreeInternalPageTest, RoutesKeysToCorrectChild)
{
    Page page{PageId{10}};

    BPlusTreeInternalPage internal{page};

    internal.setFirstChild(PageId{100});

    internal.insertChild(
        1,
        IndexKey{std::int32_t{20}},
        PageId{200}
    );

    internal.insertChild(
        2,
        IndexKey{std::int32_t{40}},
        PageId{300}
    );

    EXPECT_EQ(
        internal.lookupChild(
            IndexKey{std::int32_t{10}}
        ),
        PageId{100}
    );

    EXPECT_EQ(
        internal.lookupChild(
            IndexKey{std::int32_t{20}}
        ),
        PageId{200}
    );

    EXPECT_EQ(
        internal.lookupChild(
            IndexKey{std::int32_t{30}}
        ),
        PageId{200}
    );

    EXPECT_EQ(
        internal.lookupChild(
            IndexKey{std::int32_t{40}}
        ),
        PageId{300}
    );

    EXPECT_EQ(
        internal.lookupChild(
            IndexKey{std::int32_t{100}}
        ),
        PageId{300}
    );
}

TEST(BPlusTreeInternalPageTest, RemovesChild)
{
    Page page{PageId{10}};

    BPlusTreeInternalPage internal{page};

    internal.setFirstChild(PageId{100});

    internal.insertChild(
        1,
        IndexKey{std::int32_t{20}},
        PageId{200}
    );

    internal.insertChild(
        2,
        IndexKey{std::int32_t{40}},
        PageId{300}
    );

    internal.removeChild(1);

    EXPECT_EQ(internal.size(), 1U);
    EXPECT_EQ(internal.childCount(), 2U);

    EXPECT_EQ(
        internal.childAt(0),
        PageId{100}
    );

    EXPECT_EQ(
        internal.childAt(1),
        PageId{300}
    );

    EXPECT_EQ(
        internal.keyAt(0),
        IndexKey{std::int32_t{40}}
    );
}

TEST(BPlusTreeInternalPageTest, UpdatesKey)
{
    Page page{PageId{10}};

    BPlusTreeInternalPage internal{page};

    internal.setFirstChild(PageId{100});

    internal.insertChild(
        1,
        IndexKey{std::int32_t{20}},
        PageId{200}
    );

    internal.setKey(
        0,
        IndexKey{std::int32_t{50}}
    );

    EXPECT_EQ(
        internal.keyAt(0),
        IndexKey{std::int32_t{50}}
    );
}

TEST(BPlusTreeInternalPageTest, UpdatesChild)
{
    Page page{PageId{10}};

    BPlusTreeInternalPage internal{page};

    internal.setFirstChild(PageId{100});

    internal.insertChild(
        1,
        IndexKey{std::int32_t{20}},
        PageId{200}
    );

    internal.setChildAt(
        1,
        PageId{500}
    );

    EXPECT_EQ(
        internal.childAt(1),
        PageId{500}
    );
}

TEST(BPlusTreeInternalPageTest, SupportsStringSeparatorKeys)
{
    Page page{PageId{10}};

    BPlusTreeInternalPage internal{page};

    internal.setFirstChild(PageId{100});

    internal.insertChild(
        1,
        IndexKey{std::string{"Bob"}},
        PageId{200}
    );

    EXPECT_EQ(
        internal.keyAt(0),
        IndexKey{std::string{"Bob"}}
    );

    EXPECT_EQ(
        internal.lookupChild(
            IndexKey{std::string{"Alice"}}
        ),
        PageId{100}
    );

    EXPECT_EQ(
        internal.lookupChild(
            IndexKey{std::string{"Bob"}}
        ),
        PageId{200}
    );
}

TEST(BPlusTreeInternalPageTest, DataSurvivesPageReconstruction)
{
    Page page{PageId{10}};

    {
        BPlusTreeInternalPage internal{page};

        internal.setParentPageId(
            PageId{5}
        );

        internal.setFirstChild(
            PageId{100}
        );

        internal.insertChild(
            1,
            IndexKey{std::int32_t{20}},
            PageId{200}
        );

        internal.insertChild(
            2,
            IndexKey{std::int32_t{40}},
            PageId{300}
        );
    }

    BPlusTreeInternalPage reopened{page};

    EXPECT_EQ(reopened.size(), 2U);
    EXPECT_EQ(reopened.childCount(), 3U);

    EXPECT_EQ(
        reopened.parentPageId(),
        PageId{5}
    );

    EXPECT_EQ(
        reopened.keyAt(0),
        IndexKey{std::int32_t{20}}
    );

    EXPECT_EQ(
        reopened.keyAt(1),
        IndexKey{std::int32_t{40}}
    );

    EXPECT_EQ(
        reopened.childAt(0),
        PageId{100}
    );

    EXPECT_EQ(
        reopened.childAt(1),
        PageId{200}
    );

    EXPECT_EQ(
        reopened.childAt(2),
        PageId{300}
    );
}

} // namespace forgedb