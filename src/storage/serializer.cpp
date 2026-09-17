#include "forgedb/storage/serializer.h"

#include <cstring>
#include <limits>
#include <stdexcept>

namespace forgedb {

namespace {

template <typename Unsigned>
void writeUnsigned(std::vector<Serializer::Byte>& buffer, Unsigned value)
{
    for (std::size_t i = 0; i < sizeof(Unsigned); ++i) {
        buffer.push_back(
            static_cast<Serializer::Byte>(value >> (i * 8))
        );
    }
}

template <typename Unsigned>
Unsigned readUnsigned(std::span<const Serializer::Byte> buffer,
                      std::size_t& offset)
{
    if (offset + sizeof(Unsigned) > buffer.size()) {
        throw std::out_of_range("Serializer: buffer underflow");
    }

    Unsigned value = 0;

    for (std::size_t i = 0; i < sizeof(Unsigned); ++i) {
        value = static_cast<Unsigned>(value | (static_cast<Unsigned>(buffer[offset++]) << (i * 8)));
    }

    return value;
}

template <typename Signed, typename Unsigned>
void writeSigned(std::vector<Serializer::Byte>& buffer, Signed value)
{
    const auto unsignedValue = static_cast<Unsigned>(value);
    writeUnsigned(buffer, unsignedValue);
}

template <typename Signed, typename Unsigned>
Signed readSigned(std::span<const Serializer::Byte> buffer,
                  std::size_t& offset)
{
    return static_cast<Signed>(
        readUnsigned<Unsigned>(buffer, offset)
    );
}

} // namespace

void Serializer::writeUInt8(std::vector<Byte>& buffer, std::uint8_t value)
{
    buffer.push_back(value);
}

void Serializer::writeUInt16(std::vector<Byte>& buffer, std::uint16_t value)
{
    writeUnsigned(buffer, value);
}

void Serializer::writeUInt32(std::vector<Byte>& buffer, std::uint32_t value)
{
    writeUnsigned(buffer, value);
}

void Serializer::writeUInt64(std::vector<Byte>& buffer, std::uint64_t value)
{
    writeUnsigned(buffer, value);
}

std::uint8_t Serializer::readUInt8(std::span<const Byte> buffer,
                                   std::size_t& offset)
{
    if (offset >= buffer.size()) {
        throw std::out_of_range("Serializer: buffer underflow");
    }

    return buffer[offset++];
}

std::uint16_t Serializer::readUInt16(std::span<const Byte> buffer,
                                     std::size_t& offset)
{
    return readUnsigned<std::uint16_t>(buffer, offset);
}

std::uint32_t Serializer::readUInt32(std::span<const Byte> buffer,
                                     std::size_t& offset)
{
    return readUnsigned<std::uint32_t>(buffer, offset);
}

std::uint64_t Serializer::readUInt64(std::span<const Byte> buffer,
                                     std::size_t& offset)
{
    return readUnsigned<std::uint64_t>(buffer, offset);
}

void Serializer::writeInt32(std::vector<Byte>& buffer, std::int32_t value)
{
    writeSigned<std::int32_t, std::uint32_t>(buffer, value);
}

void Serializer::writeInt64(std::vector<Byte>& buffer, std::int64_t value)
{
    writeSigned<std::int64_t, std::uint64_t>(buffer, value);
}

std::int32_t Serializer::readInt32(std::span<const Byte> buffer,
                                   std::size_t& offset)
{
    return readSigned<std::int32_t, std::uint32_t>(buffer, offset);
}

std::int64_t Serializer::readInt64(std::span<const Byte> buffer,
                                   std::size_t& offset)
{
    return readSigned<std::int64_t, std::uint64_t>(buffer, offset);
}

void Serializer::writeFloat(std::vector<Byte>& buffer, float value)
{
    static_assert(sizeof(float) == sizeof(std::uint32_t));

    std::uint32_t bits;
    std::memcpy(&bits, &value, sizeof(value));

    writeUInt32(buffer, bits);
}

void Serializer::writeDouble(std::vector<Byte>& buffer, double value)
{
    static_assert(sizeof(double) == sizeof(std::uint64_t));

    std::uint64_t bits;
    std::memcpy(&bits, &value, sizeof(value));

    writeUInt64(buffer, bits);
}

float Serializer::readFloat(std::span<const Byte> buffer,
                            std::size_t& offset)
{
    const std::uint32_t bits = readUInt32(buffer, offset);

    float value;
    std::memcpy(&value, &bits, sizeof(value));

    return value;
}

double Serializer::readDouble(std::span<const Byte> buffer,
                              std::size_t& offset)
{
    const std::uint64_t bits = readUInt64(buffer, offset);

    double value;
    std::memcpy(&value, &bits, sizeof(value));

    return value;
}

void Serializer::writeBool(std::vector<Byte>& buffer, bool value)
{
    writeUInt8(buffer, value ? 1 : 0);
}

bool Serializer::readBool(std::span<const Byte> buffer,
                          std::size_t& offset)
{
    const std::uint8_t value = readUInt8(buffer, offset);

    if (value > 1) {
        throw std::runtime_error("Serializer: invalid boolean value");
    }

    return value == 1;
}

void Serializer::writeString(std::vector<Byte>& buffer,
                             const std::string& value)
{
    if (value.size() > std::numeric_limits<std::uint32_t>::max()) {
        throw std::length_error("Serializer: string is too large");
    }

    writeUInt32(
        buffer,
        static_cast<std::uint32_t>(value.size())
    );

    buffer.insert(
        buffer.end(),
        value.begin(),
        value.end()
    );
}

std::string Serializer::readString(std::span<const Byte> buffer,
                                   std::size_t& offset)
{
    const std::uint32_t length = readUInt32(buffer, offset);

    if (length > buffer.size() - offset) {
        throw std::out_of_range("Serializer: invalid string length");
    }

    const auto* begin =
        reinterpret_cast<const char*>(buffer.data() + offset);

    std::string value(begin, length);

    offset += length;

    return value;
}

} // namespace forgedb