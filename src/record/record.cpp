#include "forgedb/record/value.h"

#include <stdexcept>
#include <utility>

namespace forgedb {

Value::Value() noexcept
    : storage_(std::monostate{})
{
}

Value::Value(std::int32_t value)
    : storage_(value)
{
}

Value::Value(std::int64_t value)
    : storage_(value)
{
}

Value::Value(float value)
    : storage_(value)
{
}

Value::Value(double value)
    : storage_(value)
{
}

Value::Value(bool value)
    : storage_(value)
{
}

Value::Value(std::string value)
    : storage_(std::move(value))
{
}

Value::Value(const char* value)
    : storage_(std::string(value))
{
    if (value == nullptr) {
        throw std::invalid_argument(
            "Value: string cannot be null"
        );
    }
}

Value Value::null() noexcept
{
    return Value{};
}

bool Value::isNull() const noexcept
{
    return std::holds_alternative<std::monostate>(storage_);
}

DataType Value::type() const
{
    switch (storage_.index()) {
    case 0:
        throw std::logic_error(
            "Value: NULL does not have a DataType"
        );

    case 1:
        return DataType::Int32;

    case 2:
        return DataType::Int64;

    case 3:
        return DataType::Float;

    case 4:
        return DataType::Double;

    case 5:
        return DataType::Boolean;

    case 6:
        return DataType::Varchar;

    default:
        throw std::logic_error(
            "Value: unknown storage type"
        );
    }
}

std::int32_t Value::asInt32() const
{
    return std::get<std::int32_t>(storage_);
}

std::int64_t Value::asInt64() const
{
    return std::get<std::int64_t>(storage_);
}

float Value::asFloat() const
{
    return std::get<float>(storage_);
}

double Value::asDouble() const
{
    return std::get<double>(storage_);
}

bool Value::asBool() const
{
    return std::get<bool>(storage_);
}

const std::string& Value::asString() const
{
    return std::get<std::string>(storage_);
}

const Value::Storage& Value::storage() const noexcept
{
    return storage_;
}

bool operator==(const Value& lhs, const Value& rhs)
{
    return lhs.storage_ == rhs.storage_;
}

bool operator!=(const Value& lhs, const Value& rhs)
{
    return !(lhs == rhs);
}

} // namespace forgedb