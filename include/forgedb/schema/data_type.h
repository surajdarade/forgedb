#pragma once

#include <cstdint>

namespace forgedb {

enum class DataType : std::uint8_t {
    Boolean,
    Int32,
    Int64,
    Float,
    Double,
    Varchar
};

} // namespace forgedb