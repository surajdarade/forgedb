#include "forgedb/storage/heap_page.h"

#include <cstring>
#include <limits>
#include <stdexcept>

namespace forgedb {

namespace {

constexpr std::uint16_t kDeletedOffset = 0;
constexpr std::uint16_t kDeletedSize = 0;

} // namespace

HeapPage::HeapPage(Page& page)
    : page_(page)
{
    // A newly allocated zero-filled page already represents
    // an empty heap page.
    if (recordCount() == 0 &&
        readUInt16(2) == 0) {
        writeUInt16(
            2,
            static_cast<std::uint16_t>(kPageSize)
        );
    }
}

std::size_t HeapPage::recordCount() const
{
    return readUInt16(0);
}

std::size_t HeapPage::freeSpace() const
{
    const std::size_t slotEnd =
        kHeaderSize +
        recordCount() * kSlotSize;

    const std::size_t dataStart =
        readUInt16(2);

    if (dataStart < slotEnd) {
        return 0;
    }

    return dataStart - slotEnd;
}

RecordId HeapPage::insert(
    std::span<const Byte> record)
{
    if (record.empty()) {
        throw std::invalid_argument(
            "HeapPage: record cannot be empty"
        );
    }

    if (record.size() >
        std::numeric_limits<std::uint16_t>::max()) {
        throw std::length_error(
            "HeapPage: record is too large"
        );
    }

    const std::size_t required =
        requiredSpace(record.size());

    if (freeSpace() < required) {
        throw std::overflow_error(
            "HeapPage: insufficient free space"
        );
    }

    const std::uint32_t slot =
        static_cast<std::uint32_t>(recordCount());

    const std::size_t newDataStart =
        readUInt16(2) - record.size();

    std::memcpy(
        page_.data().data() + newDataStart,
        record.data(),
        record.size()
    );

    writeSlot(
        slot,
        Slot{
            static_cast<std::uint16_t>(newDataStart),
            static_cast<std::uint16_t>(record.size())
        }
    );

    writeUInt16(
        0,
        static_cast<std::uint16_t>(recordCount() + 1)
    );

    writeUInt16(
        2,
        static_cast<std::uint16_t>(newDataStart)
    );

    return RecordId{
        page_.id(),
        slot
    };
}

std::vector<HeapPage::Byte> HeapPage::read(
    std::uint32_t slot) const
{
    validateSlot(slot);

    const Slot slotData = readSlot(slot);

    if (isDeleted(slot)) {
        throw std::runtime_error(
            "HeapPage: record has been deleted"
        );
    }

    std::vector<Byte> record(slotData.size);

    std::memcpy(
        record.data(),
        page_.data().data() + slotData.offset,
        slotData.size
    );

    return record;
}

void HeapPage::update(
    std::uint32_t slot,
    std::span<const Byte> record)
{
    validateSlot(slot);

    if (record.empty()) {
        throw std::invalid_argument(
            "HeapPage: record cannot be empty"
        );
    }

    if (record.size() >
        std::numeric_limits<std::uint16_t>::max()) {
        throw std::length_error(
            "HeapPage: record is too large"
        );
    }

    Slot slotData = readSlot(slot);

    if (isDeleted(slot)) {
        throw std::runtime_error(
            "HeapPage: cannot update deleted record"
        );
    }

    if (record.size() > slotData.size) {
        compactWithUpdatedRecord(slot, record);
        return;
    }

    std::memcpy(
        page_.data().data() + slotData.offset,
        record.data(),
        record.size()
    );

    slotData.size = static_cast<std::uint16_t>(record.size());
    writeSlot(slot, slotData);
}

void HeapPage::compactWithUpdatedRecord(std::uint32_t slot, std::span<const Byte> record) {
    const std::size_t count = recordCount();
    std::vector<std::vector<Byte>> records(count);
    for (std::size_t i = 0; i < count; ++i) {
        if (!isDeleted(static_cast<std::uint32_t>(i))) records[i] = read(static_cast<std::uint32_t>(i));
    }
    records[slot] = std::vector<Byte>(record.begin(), record.end());
    std::size_t required = kHeaderSize + count * kSlotSize;
    for (const auto& bytes : records) required += bytes.size();
    if (required > kPageSize) throw std::overflow_error("HeapPage: insufficient space for updated record");
    std::fill(page_.data().begin(), page_.data().end(), 0);
    writeUInt16(0, static_cast<std::uint16_t>(count));
    std::size_t dataStart = kPageSize;
    for (std::size_t i = 0; i < count; ++i) {
        if (records[i].empty()) {
            writeSlot(static_cast<std::uint32_t>(i), Slot{kDeletedOffset, kDeletedSize});
            continue;
        }
        dataStart -= records[i].size();
        std::memcpy(page_.data().data() + dataStart, records[i].data(), records[i].size());
        writeSlot(static_cast<std::uint32_t>(i), Slot{static_cast<std::uint16_t>(dataStart), static_cast<std::uint16_t>(records[i].size())});
    }
    writeUInt16(2, static_cast<std::uint16_t>(dataStart));
}

void HeapPage::erase(std::uint32_t slot)
{
    validateSlot(slot);

    if (isDeleted(slot)) {
        return;
    }

    writeSlot(
        slot,
        Slot{
            kDeletedOffset,
            kDeletedSize
        }
    );
}

bool HeapPage::isDeleted(
    std::uint32_t slot) const
{
    validateSlot(slot);

    const Slot slotData = readSlot(slot);

    return slotData.offset == kDeletedOffset &&
           slotData.size == kDeletedSize;
}

std::uint16_t HeapPage::readUInt16(
    std::size_t offset) const
{
    if (offset + sizeof(std::uint16_t) > kPageSize) {
        throw std::out_of_range(
            "HeapPage: metadata offset out of range"
        );
    }

    const auto& data = page_.data();

    return static_cast<std::uint16_t>(
        data[offset] |
        (static_cast<std::uint16_t>(data[offset + 1]) << 8)
    );
}

void HeapPage::writeUInt16(
    std::size_t offset,
    std::uint16_t value)
{
    if (offset + sizeof(std::uint16_t) > kPageSize) {
        throw std::out_of_range(
            "HeapPage: metadata offset out of range"
        );
    }

    auto& data = page_.data();

    data[offset] =
        static_cast<Byte>(value & 0xFF);

    data[offset + 1] =
        static_cast<Byte>((value >> 8) & 0xFF);
}

HeapPage::Slot HeapPage::readSlot(
    std::uint32_t slot) const
{
    validateSlot(slot);

    const std::size_t offset =
        slotOffset(slot);

    return Slot{
        readUInt16(offset),
        readUInt16(offset + sizeof(std::uint16_t))
    };
}

void HeapPage::writeSlot(
    std::uint32_t slot,
    Slot slotData)
{
    const std::size_t offset =
        slotOffset(slot);

    writeUInt16(
        offset,
        slotData.offset
    );

    writeUInt16(
        offset + sizeof(std::uint16_t),
        slotData.size
    );
}

std::size_t HeapPage::slotOffset(
    std::uint32_t slot) const noexcept
{
    return kHeaderSize +
           static_cast<std::size_t>(slot) * kSlotSize;
}

std::size_t HeapPage::recordDataStart() const noexcept
{
    return readUInt16(2);
}

std::size_t HeapPage::requiredSpace(
    std::size_t recordSize) const noexcept
{
    return kSlotSize + recordSize;
}

void HeapPage::validateSlot(
    std::uint32_t slot) const
{
    if (slot >= recordCount()) {
        throw std::out_of_range(
            "HeapPage: slot out of range"
        );
    }
}

} // namespace forgedb