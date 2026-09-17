#pragma once

#include <cstdint>

namespace forgedb {

class PageId {
public:
    using ValueType = std::uint64_t;

    constexpr PageId() noexcept = default;

    explicit constexpr PageId(ValueType value) noexcept
        : value_(value) {}

    [[nodiscard]] constexpr ValueType value() const noexcept {
        return value_;
    }

    friend constexpr bool operator==(PageId lhs, PageId rhs) noexcept {
        return lhs.value_ == rhs.value_;
    }

    friend constexpr bool operator!=(PageId lhs, PageId rhs) noexcept {
        return !(lhs == rhs);
    }

private:
    ValueType value_{0};
};

} // namespace forgedb