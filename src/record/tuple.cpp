#include "forgedb/record/tuple.h"

#include <stdexcept>
#include <utility>

namespace forgedb {

Tuple::Tuple(
    std::vector<Value> values,
    RecordId recordId)
    : values_(std::move(values)),
      recordId_(recordId)
{
}

std::size_t Tuple::size() const noexcept
{
    return values_.size();
}

const Value& Tuple::getValue(std::size_t index) const
{
    if (index >= values_.size()) {
        throw std::out_of_range(
            "Tuple: value index out of range"
        );
    }

    return values_[index];
}

Value& Tuple::getValue(std::size_t index)
{
    if (index >= values_.size()) {
        throw std::out_of_range(
            "Tuple: value index out of range"
        );
    }

    return values_[index];
}

const std::vector<Value>& Tuple::values() const noexcept
{
    return values_;
}

RecordId Tuple::recordId() const noexcept
{
    return recordId_;
}

void Tuple::setRecordId(RecordId recordId) noexcept
{
    recordId_ = recordId;
}

bool Tuple::matchesSchema(const Schema& schema) const noexcept
{
    if (values_.size() != schema.columnCount()) {
        return false;
    }

    for (std::size_t i = 0; i < values_.size(); ++i) {
        const Value& value = values_[i];
        const Column& column = schema.column(i);

        if (value.isNull()) {
            continue;
        }

        if (value.type() != column.type()) {
            return false;
        }
    }

    return true;
}

} // namespace forgedb