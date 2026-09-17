#pragma once

#include <cstdint>
#include <string>
#include <variant>

#include "forgedb/schema/data_type.h"

namespace forgedb {

class Value {
public:
    using Storage = std::variant<
        std::monostate,
        std::int32_t,
        std::int64_t,
        float,
        double,
        bool,
        std::string
    >;

    Value() noexcept;

    explicit Value(std::int32_t value);
    explicit Value(std::int64_t value);
    explicit Value(float value);
    explicit Value(double value);
    explicit Value(bool value);
    explicit Value(std::string value);
    explicit Value(const char* value);

    [[nodiscard]] static Value null() noexcept;

    [[nodiscard]] bool isNull() const noexcept;

    [[nodiscard]] DataType type() const;

    [[nodiscard]] std::int32_t asInt32() const;
    [[nodiscard]] std::int64_t asInt64() const;
    [[nodiscard]] float asFloat() const;
    [[nodiscard]] double asDouble() const;
    [[nodiscard]] bool asBool() const;
    [[nodiscard]] const std::string& asString() const;

    [[nodiscard]] const Storage& storage() const noexcept;

    friend bool operator==(
        const Value& lhs,
        const Value& rhs
    );

    friend bool operator!=(
        const Value& lhs,
        const Value& rhs
    );

private:
    Storage storage_;
};

} // namespace forgedb