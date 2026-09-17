#include "forgedb/index/index_key.h"

#include <stdexcept>
#include <utility>

namespace forgedb {

IndexKey::IndexKey(std::int32_t value)
    : storage_(value)
{
}

IndexKey::IndexKey(std::int64_t value)
    : storage_(value)
{
}

IndexKey::IndexKey(float value)
    : storage_(value)
{
}

IndexKey::IndexKey(double value)
    : storage_(value)
{
}

IndexKey::IndexKey(std::string value)
    : storage_(std::move(value))
{
}

IndexKey::IndexKey(const Value& value)
{
    if (value.isNull()) {
        throw std::invalid_argument(
            "IndexKey: NULL cannot be used as an index key"
        );
    }

    switch (value.type()) {
    case DataType::Int32:
        storage_ = value.asInt32();
        break;

    case DataType::Int64:
        storage_ = value.asInt64();
        break;

    case DataType::Float:
        storage_ = value.asFloat();
        break;

    case DataType::Double:
        storage_ = value.asDouble();
        break;

    case DataType::Varchar:
        storage_ = value.asString();
        break;

    case DataType::Boolean:
        throw std::invalid_argument(
            "IndexKey: BOOLEAN indexes are not supported"
        );
    }
}

DataType IndexKey::type() const noexcept
{
    switch (storage_.index()) {
    case 0:
        return DataType::Int32;

    case 1:
        return DataType::Int64;

    case 2:
        return DataType::Float;

    case 3:
        return DataType::Double;

    case 4:
        return DataType::Varchar;

    default:
        std::terminate();
    }
}

const IndexKey::Storage& IndexKey::storage() const noexcept
{
    return storage_;
}

bool IndexKey::isNumeric() const noexcept
{
    return type() == DataType::Int32 ||
           type() == DataType::Int64 ||
           type() == DataType::Float ||
           type() == DataType::Double;
}

bool operator==(
    const IndexKey& lhs,
    const IndexKey& rhs)
{
    return lhs.storage_ == rhs.storage_;
}

bool operator!=(
    const IndexKey& lhs,
    const IndexKey& rhs)
{
    return !(lhs == rhs);
}

bool operator<(
    const IndexKey& lhs,
    const IndexKey& rhs)
{
    if (lhs.type() != rhs.type()) {
        throw std::invalid_argument(
            "IndexKey: cannot compare different key types"
        );
    }

    return lhs.storage_ < rhs.storage_;
}

bool operator>(
    const IndexKey& lhs,
    const IndexKey& rhs)
{
    return rhs < lhs;
}

bool operator<=(
    const IndexKey& lhs,
    const IndexKey& rhs)
{
    return !(rhs < lhs);
}

bool operator>=(
    const IndexKey& lhs,
    const IndexKey& rhs)
{
    return !(lhs < rhs);
}

} // namespace forgedb