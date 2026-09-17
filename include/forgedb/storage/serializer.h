#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace forgedb {

class Serializer {
public:
    using Byte = std::uint8_t;

    Serializer() = delete;

    // Integer serialization
    static void writeUInt8(std::vector<Byte>& buffer, std::uint8_t value);
    static void writeUInt16(std::vector<Byte>& buffer, std::uint16_t value);
    static void writeUInt32(std::vector<Byte>& buffer, std::uint32_t value);
    static void writeUInt64(std::vector<Byte>& buffer, std::uint64_t value);

    static std::uint8_t readUInt8(std::span<const Byte> buffer,
                                   std::size_t& offset);

    static std::uint16_t readUInt16(std::span<const Byte> buffer,
                                    std::size_t& offset);

    static std::uint32_t readUInt32(std::span<const Byte> buffer,
                                    std::size_t& offset);

    static std::uint64_t readUInt64(std::span<const Byte> buffer,
                                    std::size_t& offset);

    // Signed integer serialization
    static void writeInt32(std::vector<Byte>& buffer, std::int32_t value);
    static void writeInt64(std::vector<Byte>& buffer, std::int64_t value);

    static std::int32_t readInt32(std::span<const Byte> buffer,
                                  std::size_t& offset);

    static std::int64_t readInt64(std::span<const Byte> buffer,
                                  std::size_t& offset);

    // Floating-point serialization
    static void writeFloat(std::vector<Byte>& buffer, float value);
    static void writeDouble(std::vector<Byte>& buffer, double value);

    static float readFloat(std::span<const Byte> buffer,
                           std::size_t& offset);

    static double readDouble(std::span<const Byte> buffer,
                             std::size_t& offset);

    // Boolean serialization
    static void writeBool(std::vector<Byte>& buffer, bool value);

    static bool readBool(std::span<const Byte> buffer,
                         std::size_t& offset);

    // String serialization
    static void writeString(std::vector<Byte>& buffer,
                            const std::string& value);

    static std::string readString(std::span<const Byte> buffer,
                                  std::size_t& offset);
};

} // namespace forgedb