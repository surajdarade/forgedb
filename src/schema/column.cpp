#include "forgedb/schema/column.h"

#include <stdexcept>

namespace forgedb {

Column::Column(
    std::string name,
    DataType type,
    std::size_t length)
    : name_(std::move(name)),
      type_(type),
      length_(length)
{
    if (name_.empty()) {
        throw std::invalid_argument(
            "Column: name cannot be empty"
        );
    }

    if (type_ == DataType::Varchar && length_ == 0) {
        throw std::invalid_argument(
            "Column: VARCHAR requires a non-zero length"
        );
    }

    if (type_ != DataType::Varchar && length_ != 0) {
        throw std::invalid_argument(
            "Column: length is only valid for VARCHAR"
        );
    }
}

const std::string& Column::name() const noexcept
{
    return name_;
}

DataType Column::type() const noexcept
{
    return type_;
}

std::size_t Column::length() const noexcept
{
    return length_;
}

std::size_t Column::storageSize() const noexcept
{
    switch (type_) {
    case DataType::Boolean:
        return sizeof(bool);

    case DataType::Int32:
        return sizeof(std::int32_t);

    case DataType::Int64:
        return sizeof(std::int64_t);

    case DataType::Float:
        return sizeof(float);

    case DataType::Double:
        return sizeof(double);

    case DataType::Varchar:
        return length_;
    }

    throw std::logic_error(
        "Column: unknown data type"
    );
}

} // namespace forgedb