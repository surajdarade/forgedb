#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "forgedb/common/constants.h"
#include "forgedb/common/page_id.h"

namespace forgedb {

class Page {
public:
    using Byte = std::uint8_t;
    using Data = std::array<Byte, kPageSize>;

    Page() noexcept = default;

    explicit Page(PageId id) noexcept
        : id_(id) {}

    [[nodiscard]] PageId id() const noexcept {
        return id_;
    }

    void setId(PageId id) noexcept {
        id_ = id;
    }

    [[nodiscard]] const Data& data() const noexcept {
        return data_;
    }

    [[nodiscard]] Data& data() noexcept {
        return data_;
    }

    [[nodiscard]] constexpr std::size_t size() const noexcept {
        return kPageSize;
    }

private:
    PageId id_{};
    Data data_{};
};

} // namespace forgedb