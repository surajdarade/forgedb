#include <gtest/gtest.h>

#include "forgedb/common/page_id.h"

namespace forgedb {

TEST(PageIdTest, StoresAndReturnsValue)
{
    constexpr PageId pageId{42};

    EXPECT_EQ(pageId.value(), 42);
}

TEST(PageIdTest, EqualityWorks)
{
    constexpr PageId first{42};
    constexpr PageId second{42};
    constexpr PageId different{43};

    EXPECT_EQ(first, second);
    EXPECT_NE(first, different);
}

} // namespace forgedb