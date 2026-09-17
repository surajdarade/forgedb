#include <cstddef>
#include <stdexcept>

#include <gtest/gtest.h>

#include "forgedb/buffer/lru_replacer.h"

namespace forgedb {

class LRUReplacerTest : public ::testing::Test {
protected:
    LRUReplacer replacer_{3};
};

TEST_F(LRUReplacerTest, StartsEmpty)
{
    EXPECT_EQ(replacer_.size(), 0);
}

TEST_F(LRUReplacerTest, UnpinAddsFrame)
{
    replacer_.unpin(1);

    EXPECT_EQ(replacer_.size(), 1);
}

TEST_F(LRUReplacerTest, DoesNotAddSameFrameTwice)
{
    replacer_.unpin(1);
    replacer_.unpin(1);

    EXPECT_EQ(replacer_.size(), 1);
}

TEST_F(LRUReplacerTest, VictimReturnsLeastRecentlyUnpinnedFrame)
{
    replacer_.unpin(1);
    replacer_.unpin(2);
    replacer_.unpin(3);

    std::size_t frameId = 0;

    ASSERT_TRUE(replacer_.victim(frameId));

    EXPECT_EQ(frameId, 1);
    EXPECT_EQ(replacer_.size(), 2);
}

TEST_F(LRUReplacerTest, VictimFollowsLRUOrder)
{
    replacer_.unpin(1);
    replacer_.unpin(2);
    replacer_.unpin(3);

    std::size_t frameId = 0;

    ASSERT_TRUE(replacer_.victim(frameId));
    EXPECT_EQ(frameId, 1);

    ASSERT_TRUE(replacer_.victim(frameId));
    EXPECT_EQ(frameId, 2);

    ASSERT_TRUE(replacer_.victim(frameId));
    EXPECT_EQ(frameId, 3);

    EXPECT_EQ(replacer_.size(), 0);
}

TEST_F(LRUReplacerTest, VictimFailsWhenEmpty)
{
    std::size_t frameId = 123;

    EXPECT_FALSE(replacer_.victim(frameId));

    EXPECT_EQ(frameId, 123);
}

TEST_F(LRUReplacerTest, PinRemovesFrameFromReplacementSet)
{
    replacer_.unpin(1);
    replacer_.unpin(2);

    EXPECT_EQ(replacer_.size(), 2);

    replacer_.pin(1);

    EXPECT_EQ(replacer_.size(), 1);

    std::size_t frameId = 0;

    ASSERT_TRUE(replacer_.victim(frameId));

    EXPECT_EQ(frameId, 2);
}

TEST_F(LRUReplacerTest, PinUnknownFrameDoesNothing)
{
    replacer_.unpin(1);

    replacer_.pin(99);

    EXPECT_EQ(replacer_.size(), 1);
}

TEST_F(LRUReplacerTest, UnpinAfterPinMakesFrameEvictableAgain)
{
    replacer_.unpin(1);
    replacer_.pin(1);

    EXPECT_EQ(replacer_.size(), 0);

    replacer_.unpin(1);

    EXPECT_EQ(replacer_.size(), 1);

    std::size_t frameId = 0;

    ASSERT_TRUE(replacer_.victim(frameId));

    EXPECT_EQ(frameId, 1);
}

TEST_F(LRUReplacerTest, CapacityIsRespected)
{
    replacer_.unpin(1);
    replacer_.unpin(2);
    replacer_.unpin(3);
    replacer_.unpin(4);

    EXPECT_EQ(replacer_.size(), 3);

    std::size_t frameId = 0;

    ASSERT_TRUE(replacer_.victim(frameId));
    EXPECT_EQ(frameId, 1);

    ASSERT_TRUE(replacer_.victim(frameId));
    EXPECT_EQ(frameId, 2);

    ASSERT_TRUE(replacer_.victim(frameId));
    EXPECT_EQ(frameId, 3);

    EXPECT_FALSE(replacer_.victim(frameId));
}

TEST_F(LRUReplacerTest, UnpinDoesNotRefreshExistingFrame)
{
    replacer_.unpin(1);
    replacer_.unpin(2);
    replacer_.unpin(3);

    // Repeated unpin should not move frame 1 to MRU.
    replacer_.unpin(1);

    std::size_t frameId = 0;

    ASSERT_TRUE(replacer_.victim(frameId));

    EXPECT_EQ(frameId, 1);
}

TEST_F(LRUReplacerTest, VictimRemovesFrameFromReplacementSet)
{
    replacer_.unpin(1);
    replacer_.unpin(2);

    std::size_t frameId = 0;

    ASSERT_TRUE(replacer_.victim(frameId));

    EXPECT_EQ(replacer_.size(), 1);

    // Frame 1 can be added again after being evicted.
    replacer_.unpin(1);

    EXPECT_EQ(replacer_.size(), 2);
}

TEST(LRUReplacerStandaloneTest, RejectsZeroCapacity)
{
    EXPECT_THROW(
        LRUReplacer replacer{0},
        std::invalid_argument
    );
}

} // namespace forgedb