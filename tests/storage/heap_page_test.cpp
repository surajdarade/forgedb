#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "forgedb/common/page_id.h"
#include "forgedb/storage/heap_page.h"
#include "forgedb/storage/page.h"

namespace forgedb {

TEST(HeapPageTest, NewPageIsEmpty)
{
    Page page{PageId{1}};
    HeapPage heapPage{page};

    EXPECT_EQ(heapPage.recordCount(), 0U);
    EXPECT_GT(heapPage.freeSpace(), 0U);
}

TEST(HeapPageTest, InsertsAndReadsRecord)
{
    Page page{PageId{1}};
    HeapPage heapPage{page};

    const std::string input = "ForgeDB";

    const auto record = std::span<const std::uint8_t>{
        reinterpret_cast<const std::uint8_t*>(input.data()),
        input.size()
    };

    const RecordId recordId =
        heapPage.insert(record);

    EXPECT_EQ(recordId.pageId(), PageId{1});
    EXPECT_EQ(recordId.slot(), 0U);
    EXPECT_EQ(heapPage.recordCount(), 1U);

    const auto output =
        heapPage.read(recordId.slot());

    const std::string restored{
        reinterpret_cast<const char*>(output.data()),
        output.size()
    };

    EXPECT_EQ(restored, input);
}

TEST(HeapPageTest, MultipleRecordsUseDifferentSlots)
{
    Page page{PageId{1}};
    HeapPage heapPage{page};

    const std::vector<std::uint8_t> first{'A', 'B'};
    const std::vector<std::uint8_t> second{'C', 'D', 'E'};

    const RecordId firstId =
        heapPage.insert(first);

    const RecordId secondId =
        heapPage.insert(second);

    EXPECT_EQ(firstId.slot(), 0U);
    EXPECT_EQ(secondId.slot(), 1U);

    EXPECT_EQ(
        heapPage.read(firstId.slot()),
        first
    );

    EXPECT_EQ(
        heapPage.read(secondId.slot()),
        second
    );
}

TEST(HeapPageTest, UpdateRecord)
{
    Page page{PageId{1}};
    HeapPage heapPage{page};

    const std::vector<std::uint8_t> original{
        'A', 'B', 'C', 'D'
    };

    const std::vector<std::uint8_t> updated{
        'X', 'Y'
    };

    const RecordId recordId =
        heapPage.insert(original);

    heapPage.update(
        recordId.slot(),
        updated
    );

    EXPECT_EQ(
        heapPage.read(recordId.slot()),
        updated
    );
}

TEST(HeapPageTest, RejectsUpdateThatDoesNotFit)
{
    Page page{PageId{1}};
    HeapPage heapPage{page};

    const std::vector<std::uint8_t> original{
        'A', 'B'
    };

    const std::vector<std::uint8_t> updated{
        'A', 'B', 'C'
    };

    const RecordId recordId =
        heapPage.insert(original);

    EXPECT_THROW(
        heapPage.update(recordId.slot(), updated),
        std::overflow_error
    );
}

TEST(HeapPageTest, DeletesRecord)
{
    Page page{PageId{1}};
    HeapPage heapPage{page};

    const std::vector<std::uint8_t> record{
        'A', 'B', 'C'
    };

    const RecordId recordId =
        heapPage.insert(record);

    heapPage.erase(recordId.slot());

    EXPECT_TRUE(
        heapPage.isDeleted(recordId.slot())
    );

    EXPECT_THROW(
        heapPage.read(recordId.slot()),
        std::runtime_error
    );
}

TEST(HeapPageTest, DeletedSlotCanBeDetected)
{
    Page page{PageId{1}};
    HeapPage heapPage{page};

    const std::vector<std::uint8_t> record{
        'A'
    };

    const RecordId recordId =
        heapPage.insert(record);

    EXPECT_FALSE(
        heapPage.isDeleted(recordId.slot())
    );

    heapPage.erase(recordId.slot());

    EXPECT_TRUE(
        heapPage.isDeleted(recordId.slot())
    );
}

TEST(HeapPageTest, RejectsInvalidSlot)
{
    Page page{PageId{1}};
    HeapPage heapPage{page};

    EXPECT_THROW(
        heapPage.read(0),
        std::out_of_range
    );

    EXPECT_THROW(
        heapPage.erase(0),
        std::out_of_range
    );
}

TEST(HeapPageTest, TracksFreeSpace)
{
    Page page{PageId{1}};
    HeapPage heapPage{page};

    const std::size_t initialFreeSpace =
        heapPage.freeSpace();

    const std::vector<std::uint8_t> record{
        'A', 'B', 'C', 'D', 'E'
    };

    heapPage.insert(record);

    EXPECT_EQ(
        heapPage.freeSpace(),
        initialFreeSpace - record.size() -
        HeapPage::kSlotSize
    );
}

} // namespace forgedb