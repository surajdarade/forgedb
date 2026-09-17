#pragma once

#include <cstddef>
#include <string>
#include <utility>

#include "forgedb/schema/data_type.h"

namespace forgedb {

class Column {
public:
    Column(
        std::string name,
        DataType type,
        std::size_t length = 0
    );

    [[nodiscard]] const std::string& name() const noexcept;

    [[nodiscard]] DataType type() const noexcept;

    [[nodiscard]] std::size_t length() const noexcept;

    // Returns the fixed storage size for fixed-width types.
    // For Varchar, returns the configured maximum length.
    [[nodiscard]] std::size_t storageSize() const noexcept;

private:
    std::string name_;
    DataType type_;
    std::size_t length_;
};

} // namespace forgedb