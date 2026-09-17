#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "forgedb/schema/column.h"

namespace forgedb {

class Schema {
public:
    explicit Schema(std::vector<Column> columns);

    [[nodiscard]] std::size_t columnCount() const noexcept;

    [[nodiscard]] const Column& column(
        std::size_t index
    ) const;

    [[nodiscard]] std::size_t columnIndex(
        const std::string& name
    ) const;

    [[nodiscard]] const std::vector<Column>& columns() const noexcept;

    [[nodiscard]] std::size_t storageSize() const noexcept;

private:
    std::vector<Column> columns_;
};

} // namespace forgedb