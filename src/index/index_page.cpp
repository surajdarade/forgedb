#include "forgedb/index/index_page.h"

#include <limits>
#include <stdexcept>

namespace forgedb {

IndexPage::IndexPage(Page& page)
    : page_(page)
{
}

IndexPageType IndexPage::pageType() const noexcept
{
    return static_cast<IndexPageType>(
        readUInt8(kTypeOffset)
    );
}

void IndexPage::setPageType(IndexPageType type) noexcept
{
    writeUInt8(
        kTypeOffset,
        static_cast<std::uint8_t>(type)
    );
}

PageId IndexPage::parentPageId() const noexcept
{
    return PageId{
        readUInt64(kParentPageIdOffset)
    };
}

void IndexPage::setParentPageId(PageId pageId) noexcept
{
    writeUInt64(
        kParentPageIdOffset,
        pageId.value()
    );
}

std::size_t IndexPage::entryCount() const noexcept
{
    return readUInt16(kEntryCountOffset);
}

void IndexPage::setEntryCount(std::size_t count)
{
    if (count >
        std::numeric_limits<std::uint16_t>::max()) {
        throw std::length_error(
            "IndexPage: entry count is too large"
        );
    }

    writeUInt16(
        kEntryCountOffset,
        static_cast<std::uint16_t>(count)
    );
}

std::uint8_t IndexPage::readUInt8(
    std::size_t offset) const noexcept
{
    return page_.data()[offset];
}

void IndexPage::writeUInt8(
    std::size_t offset,
    std::uint8_t value) noexcept
{
    page_.data()[offset] = value;
}

std::uint16_t IndexPage::readUInt16(
    std::size_t offset) const noexcept
{
    const auto& data = page_.data();

    return static_cast<std::uint16_t>(
        data[offset] |
        (static_cast<std::uint16_t>(
            data[offset + 1]
        ) << 8)
    );
}

void IndexPage::writeUInt16(
    std::size_t offset,
    std::uint16_t value) noexcept
{
    auto& data = page_.data();

    data[offset] =
        static_cast<std::uint8_t>(value & 0xFF);

    data[offset + 1] =
        static_cast<std::uint8_t>(
            (value >> 8) & 0xFF
        );
}

std::uint64_t IndexPage::readUInt64(
    std::size_t offset) const noexcept
{
    const auto& data = page_.data();

    std::uint64_t value = 0;

    for (std::size_t i = 0; i < sizeof(value); ++i) {
        value |=
            static_cast<std::uint64_t>(
                data[offset + i]
            ) << (i * 8);
    }

    return value;
}

void IndexPage::writeUInt64(
    std::size_t offset,
    std::uint64_t value) noexcept
{
    auto& data = page_.data();

    for (std::size_t i = 0; i < sizeof(value); ++i) {
        data[offset + i] =
            static_cast<std::uint8_t>(
                value >> (i * 8)
            );
    }
}

} // namespace forgedb