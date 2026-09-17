#pragma once

#include <cstdint>
#include <string>
#include <variant>

#include "forgedb/record/value.h"

namespace forgedb {

class IndexKey {
public:
    using Storage = std::variant<
        std::int32_t,
        std::int64_t,
        float,
        double,
        std::string
    >;

    explicit IndexKey(std::int32_t value);
    explicit IndexKey(std::int64_t value);
    explicit IndexKey(float value);
    explicit IndexKey(double value);
    explicit IndexKey(std::string value);

    explicit IndexKey(const Value& value);

    [[nodiscard]] DataType type() const noexcept;

    [[nodiscard]] const Storage& storage() const noexcept;

    [[nodiscard]] bool isNumeric() const noexcept;

    friend bool operator==(
        const IndexKey& lhs,
        const IndexKey& rhs
    );

    friend bool operator!=(
        const IndexKey& lhs,
        const IndexKey& rhs
    );

    friend bool operator<(
        const IndexKey& lhs,
        const IndexKey& rhs
    );

    friend bool operator>(
        const IndexKey& lhs,
        const IndexKey& rhs
    );

    friend bool operator<=(
        const IndexKey& lhs,
        const IndexKey& rhs
    );

    friend bool operator>=(
        const IndexKey& lhs,
        const IndexKey& rhs
    );

private:
    Storage storage_;
};

} // namespace forgedb