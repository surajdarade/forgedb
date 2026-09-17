#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "forgedb/record/tuple.h"

namespace forgedb {

class TupleSerializer {
public:
    using Byte = std::uint8_t;

    TupleSerializer() = delete;

    // Serialize a tuple into its binary representation.
    [[nodiscard]] static std::vector<Byte> serialize(
        const Tuple& tuple
    );

    // Deserialize a tuple from its binary representation.
    [[nodiscard]] static Tuple deserialize(
        std::span<const Byte> data
    );
};

} // namespace forgedb