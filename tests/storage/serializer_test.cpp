#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "forgedb/storage/serializer.h"

namespace forgedb {

TEST(SerializerTest, WriteAndReadUInt8)
{
    std::vector<Serializer::Byte> buffer;

    Serializer::writeUInt8(buffer, 255);

    std::size_t offset = 0;

    EXPECT_EQ(
        Serializer::readUInt8(buffer, offset),
        255
    );

    EXPECT_EQ(offset, 1);
}

TEST(SerializerTest, WriteAndReadUInt16)
{
    std::vector<Serializer::Byte> buffer;

    Serializer::writeUInt16(buffer, 0x1234);

    std::size_t offset = 0;

    EXPECT_EQ(
        Serializer::readUInt16(buffer, offset),
        0x1234
    );

    EXPECT_EQ(offset, sizeof(std::uint16_t));
}

TEST(SerializerTest, WriteAndReadUInt32)
{
    std::vector<Serializer::Byte> buffer;

    Serializer::writeUInt32(buffer, 0x12345678);

    std::size_t offset = 0;

    EXPECT_EQ(
        Serializer::readUInt32(buffer, offset),
        0x12345678
    );

    EXPECT_EQ(offset, sizeof(std::uint32_t));
}

TEST(SerializerTest, WriteAndReadUInt64)
{
    std::vector<Serializer::Byte> buffer;

    Serializer::writeUInt64(buffer, 0x123456789ABCDEF0ULL);

    std::size_t offset = 0;

    EXPECT_EQ(
        Serializer::readUInt64(buffer, offset),
        0x123456789ABCDEF0ULL
    );

    EXPECT_EQ(offset, sizeof(std::uint64_t));
}

TEST(SerializerTest, UsesLittleEndianEncoding)
{
    std::vector<Serializer::Byte> buffer;

    Serializer::writeUInt32(buffer, 0x12345678);

    ASSERT_EQ(buffer.size(), 4);

    EXPECT_EQ(buffer[0], 0x78);
    EXPECT_EQ(buffer[1], 0x56);
    EXPECT_EQ(buffer[2], 0x34);
    EXPECT_EQ(buffer[3], 0x12);
}

TEST(SerializerTest, WriteAndReadSignedIntegers)
{
    std::vector<Serializer::Byte> buffer;

    Serializer::writeInt32(buffer, -123456);
    Serializer::writeInt64(buffer, -9876543210LL);

    std::size_t offset = 0;

    EXPECT_EQ(
        Serializer::readInt32(buffer, offset),
        -123456
    );

    EXPECT_EQ(
        Serializer::readInt64(buffer, offset),
        -9876543210LL
    );

    EXPECT_EQ(offset, sizeof(std::int32_t) + sizeof(std::int64_t));
}

TEST(SerializerTest, WriteAndReadFloatingPointValues)
{
    std::vector<Serializer::Byte> buffer;

    Serializer::writeFloat(buffer, 3.14159F);
    Serializer::writeDouble(buffer, 12345.6789);

    std::size_t offset = 0;

    EXPECT_FLOAT_EQ(
        Serializer::readFloat(buffer, offset),
        3.14159F
    );

    EXPECT_DOUBLE_EQ(
        Serializer::readDouble(buffer, offset),
        12345.6789
    );
}

TEST(SerializerTest, WriteAndReadBooleanValues)
{
    std::vector<Serializer::Byte> buffer;

    Serializer::writeBool(buffer, true);
    Serializer::writeBool(buffer, false);

    std::size_t offset = 0;

    EXPECT_TRUE(
        Serializer::readBool(buffer, offset)
    );

    EXPECT_FALSE(
        Serializer::readBool(buffer, offset)
    );

    EXPECT_EQ(offset, 2);
}

TEST(SerializerTest, WriteAndReadString)
{
    std::vector<Serializer::Byte> buffer;

    const std::string expected = "ForgeDB";

    Serializer::writeString(buffer, expected);

    std::size_t offset = 0;

    EXPECT_EQ(
        Serializer::readString(buffer, offset),
        expected
    );

    EXPECT_EQ(
        offset,
        sizeof(std::uint32_t) + expected.size()
    );
}

TEST(SerializerTest, HandlesEmptyString)
{
    std::vector<Serializer::Byte> buffer;

    Serializer::writeString(buffer, "");

    std::size_t offset = 0;

    EXPECT_EQ(
        Serializer::readString(buffer, offset),
        ""
    );

    EXPECT_EQ(offset, sizeof(std::uint32_t));
}

TEST(SerializerTest, SupportsSequentialValues)
{
    std::vector<Serializer::Byte> buffer;

    Serializer::writeUInt32(buffer, 42);
    Serializer::writeInt64(buffer, -1000);
    Serializer::writeBool(buffer, true);
    Serializer::writeString(buffer, "ForgeDB");

    std::size_t offset = 0;

    EXPECT_EQ(
        Serializer::readUInt32(buffer, offset),
        42
    );

    EXPECT_EQ(
        Serializer::readInt64(buffer, offset),
        -1000
    );

    EXPECT_TRUE(
        Serializer::readBool(buffer, offset)
    );

    EXPECT_EQ(
        Serializer::readString(buffer, offset),
        "ForgeDB"
    );

    EXPECT_EQ(offset, buffer.size());
}

TEST(SerializerTest, ThrowsWhenReadingPastBuffer)
{
    std::vector<Serializer::Byte> buffer;

    std::size_t offset = 0;

    EXPECT_THROW(
        Serializer::readUInt64(buffer, offset),
        std::out_of_range
    );
}

TEST(SerializerTest, ThrowsWhenStringLengthExceedsBuffer)
{
    std::vector<Serializer::Byte> buffer;

    // String length = 10, but no string data follows.
    Serializer::writeUInt32(buffer, 10);

    std::size_t offset = 0;

    EXPECT_THROW(
        Serializer::readString(buffer, offset),
        std::out_of_range
    );
}

TEST(SerializerTest, RejectsInvalidBoolean)
{
    std::vector<Serializer::Byte> buffer;

    Serializer::writeUInt8(buffer, 2);

    std::size_t offset = 0;

    EXPECT_THROW(
        Serializer::readBool(buffer, offset),
        std::runtime_error
    );
}

} // namespace forgedb