#include <cstdint>

#include <gtest/gtest.h>

#include "forgedb/common/page_id.h"
#include "forgedb/record/record_id.h"

namespace forgedb {

TEST(RecordIdTest, DefaultRecordIdIsInvalid)
{
    const RecordId recordId;

    EXPECT_FALSE(recordId.isValid());
    EXPECT_EQ(recordId.pageId(), PageId{});
    EXPECT_EQ(recordId.slot(), 0);
}

TEST(RecordIdTest, StoresPageIdAndSlot)
{
    constexpr PageId pageId{42};
    constexpr RecordId recordId{pageId, 7};

    EXPECT_EQ(recordId.pageId(), pageId);
    EXPECT_EQ(recordId.slot(), 7);
}

TEST(RecordIdTest, ValidRecordIdIsRecognized)
{
    constexpr RecordId recordId{
        PageId{10},
        5
    };

    EXPECT_TRUE(recordId.isValid());
}

TEST(RecordIdTest, RecordIdsWithDifferentPagesAreNotEqual)
{
    constexpr RecordId first{
        PageId{10},
        5
    };

    constexpr RecordId second{
        PageId{11},
        5
    };

    EXPECT_NE(first, second);
}

TEST(RecordIdTest, RecordIdsWithDifferentSlotsAreNotEqual)
{
    constexpr RecordId first{
        PageId{10},
        5
    };

    constexpr RecordId second{
        PageId{10},
        6
    };

    EXPECT_NE(first, second);
}

TEST(RecordIdTest, IdenticalRecordIdsAreEqual)
{
    constexpr RecordId first{
        PageId{42},
        7
    };

    constexpr RecordId second{
        PageId{42},
        7
    };

    EXPECT_EQ(first, second);
}

TEST(RecordIdTest, SupportsMaximumSlotValue)
{
    constexpr RecordId recordId{
        PageId{100},
        UINT32_MAX
    };

    EXPECT_EQ(recordId.slot(), UINT32_MAX);
    EXPECT_TRUE(recordId.isValid());
}

} // namespace forgedb