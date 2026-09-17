#include <cstddef>
#include <cstdint>

#include <gtest/gtest.h>

#include "forgedb/common/constants.h"
#include "forgedb/common/page_id.h"
#include "forgedb/storage/page.h"

namespace forgedb {

TEST(PageTest, HasExpectedSize)
{
    Page page;

    EXPECT_EQ(page.size(), kPageSize);
    EXPECT_EQ(page.data().size(), kPageSize);
}

TEST(PageTest, StoresPageId)
{
    constexpr PageId expectedId{42};

    Page page{expectedId};

    EXPECT_EQ(page.id(), expectedId);
}

TEST(PageTest, PageDataIsInitialized)
{
    Page page;

    for (const std::uint8_t byte : page.data()) {
        EXPECT_EQ(byte, 0);
    }
}

TEST(PageTest, DataCanBeModified)
{
    Page page;

    page.data()[0] = 42;
    page.data()[kPageSize - 1] = 99;

    EXPECT_EQ(page.data()[0], 42);
    EXPECT_EQ(page.data()[kPageSize - 1], 99);
}

} // namespace forgedb