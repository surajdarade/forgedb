#pragma once

#include <cstdint>

#include "forgedb/common/page_id.h"

namespace forgedb {

class RecordId {
public:
    using SlotType = std::uint32_t;

    constexpr RecordId() noexcept = default;

    constexpr RecordId(
        PageId pageId,
        SlotType slot
    ) noexcept
        : pageId_(pageId),
          slot_(slot)
    {
    }

    [[nodiscard]] constexpr PageId pageId() const noexcept
    {
        return pageId_;
    }

    [[nodiscard]] constexpr SlotType slot() const noexcept
    {
        return slot_;
    }

    [[nodiscard]] constexpr bool isValid() const noexcept
    {
        return pageId_.value() != 0 || slot_ != 0;
    }

    friend constexpr bool operator==(
        RecordId lhs,
        RecordId rhs
    ) noexcept
    {
        return lhs.pageId_ == rhs.pageId_
            && lhs.slot_ == rhs.slot_;
    }

    friend constexpr bool operator!=(
        RecordId lhs,
        RecordId rhs
    ) noexcept
    {
        return !(lhs == rhs);
    }

private:
    PageId pageId_{};
    SlotType slot_{0};
};

} // namespace forgedb