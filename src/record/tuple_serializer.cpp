#include "forgedb/record/tuple_serializer.h"

#include <limits>
#include <cstdint>
#include <stdexcept>

#include "forgedb/storage/serializer.h"

namespace forgedb {

namespace {

enum class ValueTypeTag : std::uint8_t {
    Null = 0,
    Int32 = 1,
    Int64 = 2,
    Float = 3,
    Double = 4,
    Boolean = 5,
    Varchar = 6
};

ValueTypeTag typeTag(const Value& value)
{
    if (value.isNull()) {
        return ValueTypeTag::Null;
    }

    switch (value.type()) {
    case DataType::Int32:
        return ValueTypeTag::Int32;

    case DataType::Int64:
        return ValueTypeTag::Int64;

    case DataType::Float:
        return ValueTypeTag::Float;

    case DataType::Double:
        return ValueTypeTag::Double;

    case DataType::Boolean:
        return ValueTypeTag::Boolean;

    case DataType::Varchar:
        return ValueTypeTag::Varchar;
    }

    throw std::logic_error(
        "TupleSerializer: unsupported value type"
    );
}

void serializeValue(
    std::vector<TupleSerializer::Byte>& buffer,
    const Value& value)
{
    const auto tag = typeTag(value);

    Serializer::writeUInt8(
        buffer,
        static_cast<std::uint8_t>(tag)
    );

    switch (tag) {
    case ValueTypeTag::Null:
        break;

    case ValueTypeTag::Int32:
        Serializer::writeInt32(
            buffer,
            value.asInt32()
        );
        break;

    case ValueTypeTag::Int64:
        Serializer::writeInt64(
            buffer,
            value.asInt64()
        );
        break;

    case ValueTypeTag::Float:
        Serializer::writeFloat(
            buffer,
            value.asFloat()
        );
        break;

    case ValueTypeTag::Double:
        Serializer::writeDouble(
            buffer,
            value.asDouble()
        );
        break;

    case ValueTypeTag::Boolean:
        Serializer::writeBool(
            buffer,
            value.asBool()
        );
        break;

    case ValueTypeTag::Varchar:
        Serializer::writeString(
            buffer,
            value.asString()
        );
        break;
    }
}

Value deserializeValue(
    std::span<const TupleSerializer::Byte> data,
    std::size_t& offset)
{
    const auto rawTag =
        Serializer::readUInt8(data, offset);

    const auto tag =
        static_cast<ValueTypeTag>(rawTag);

    switch (tag) {
    case ValueTypeTag::Null:
        return Value::null();

    case ValueTypeTag::Int32:
        return Value{
            Serializer::readInt32(data, offset)
        };

    case ValueTypeTag::Int64:
        return Value{
            Serializer::readInt64(data, offset)
        };

    case ValueTypeTag::Float:
        return Value{
            Serializer::readFloat(data, offset)
        };

    case ValueTypeTag::Double:
        return Value{
            Serializer::readDouble(data, offset)
        };

    case ValueTypeTag::Boolean:
        return Value{
            Serializer::readBool(data, offset)
        };

    case ValueTypeTag::Varchar:
        return Value{
            Serializer::readString(data, offset)
        };

    default:
        throw std::runtime_error(
            "TupleSerializer: invalid value type tag"
        );
    }
}

} // namespace

std::vector<TupleSerializer::Byte>
TupleSerializer::serialize(const Tuple& tuple)
{
    std::vector<Byte> buffer;

    buffer.reserve(tuple.size() * sizeof(std::uint8_t));

    if (tuple.size() >
        static_cast<std::size_t>(
            std::numeric_limits<std::uint32_t>::max()
        )) {
        throw std::length_error(
            "TupleSerializer: too many values"
        );
    }

    Serializer::writeUInt32(
        buffer,
        static_cast<std::uint32_t>(tuple.size())
    );

    for (const Value& value : tuple.values()) {
        serializeValue(buffer, value);
    }

    return buffer;
}

Tuple TupleSerializer::deserialize(
    std::span<const Byte> data)
{
    std::size_t offset = 0;

    const std::uint32_t valueCount =
        Serializer::readUInt32(data, offset);

    std::vector<Value> values;
    values.reserve(valueCount);

    for (std::uint32_t i = 0; i < valueCount; ++i) {
        values.push_back(
            deserializeValue(data, offset)
        );
    }

    if (offset != data.size()) {
        throw std::runtime_error(
            "TupleSerializer: trailing bytes after tuple"
        );
    }

    return Tuple{std::move(values)};
}

} // namespace forgedb