#pragma once

#include <cstddef>
#include <cstdint>

#include "forgedb/common/page_id.h"
#include "forgedb/storage/page.h"

namespace forgedb {

enum class IndexPageType : std::uint8_t {
    Internal = 0,
    Leaf = 1
};

class IndexPage {
public:
    static constexpr std::size_t kTypeOffset = 0;
    static constexpr std::size_t kParentPageIdOffset = 1;
    static constexpr std::size_t kEntryCountOffset = 9;

    static constexpr std::size_t kHeaderSize = 11;

    explicit IndexPage(Page& page);

    [[nodiscard]] IndexPageType pageType() const noexcept;
    void setPageType(IndexPageType type) noexcept;

    [[nodiscard]] PageId parentPageId() const noexcept;
    void setParentPageId(PageId pageId) noexcept;

    [[nodiscard]] std::size_t entryCount() const noexcept;
    void setEntryCount(std::size_t count);

protected:
    Page& page_;

    [[nodiscard]] std::uint8_t readUInt8(
        std::size_t offset
    ) const noexcept;

    void writeUInt8(
        std::size_t offset,
        std::uint8_t value
    ) noexcept;

    [[nodiscard]] std::uint16_t readUInt16(
        std::size_t offset
    ) const noexcept;

    void writeUInt16(
        std::size_t offset,
        std::uint16_t value
    ) noexcept;

    [[nodiscard]] std::uint64_t readUInt64(
        std::size_t offset
    ) const noexcept;

    void writeUInt64(
        std::size_t offset,
        std::uint64_t value
    ) noexcept;
};

} // namespace forgedb