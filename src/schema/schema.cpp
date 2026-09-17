#include "forgedb/schema/schema.h"

#include <stdexcept>
#include <utility>

namespace forgedb {

Schema::Schema(std::vector<Column> columns)
    : columns_(std::move(columns))
{
    for (std::size_t i = 0; i < columns_.size(); ++i) {
        for (std::size_t j = i + 1; j < columns_.size(); ++j) {
            if (columns_[i].name() == columns_[j].name()) {
                throw std::invalid_argument(
                    "Schema: duplicate column name"
                );
            }
        }
    }
}

std::size_t Schema::columnCount() const noexcept
{
    return columns_.size();
}

const Column& Schema::column(std::size_t index) const
{
    if (index >= columns_.size()) {
        throw std::out_of_range(
            "Schema: column index out of range"
        );
    }

    return columns_[index];
}

std::size_t Schema::columnIndex(const std::string& name) const
{
    for (std::size_t i = 0; i < columns_.size(); ++i) {
        if (columns_[i].name() == name) {
            return i;
        }
    }

    throw std::out_of_range(
        "Schema: column not found"
    );
}

const std::vector<Column>& Schema::columns() const noexcept
{
    return columns_;
}

std::size_t Schema::storageSize() const noexcept
{
    std::size_t totalSize = 0;

    for (const Column& column : columns_) {
        totalSize += column.storageSize();
    }

    return totalSize;
}

} // namespace forgedb