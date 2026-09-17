#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "forgedb/common/constants.h"
#include "forgedb/record/record_id.h"
#include "forgedb/storage/page.h"

namespace forgedb {

class HeapPage {
public:
    using Byte = std::uint8_t;

    // Header:
    // [uint16_t record count]
    // [uint16_t free space offset]
    //
    // Slot directory grows upward immediately after the header.
    // Record data grows downward from the end of the page.
    struct Slot {
        std::uint16_t offset;
        std::uint16_t size;
    };

    static constexpr std::size_t kHeaderSize = 4;
    static constexpr std::size_t kSlotSize = 4;

    explicit HeapPage(Page& page);

    [[nodiscard]] std::size_t recordCount() const;

    [[nodiscard]] std::size_t freeSpace() const;

    // Insert a serialized record.
    // Returns the slot number assigned to the record.
    [[nodiscard]] RecordId insert(
        std::span<const Byte> record
    );

    // Read a record from the given slot.
    [[nodiscard]] std::vector<Byte> read(
        std::uint32_t slot
    ) const;

    // Replace an existing record.
    // The replacement must fit into the current record area.
    void update(
        std::uint32_t slot,
        std::span<const Byte> record
    );

    // Mark a record as deleted.
    void erase(std::uint32_t slot);

    [[nodiscard]] bool isDeleted(
        std::uint32_t slot
    ) const;

private:
    [[nodiscard]] std::uint16_t readUInt16(
        std::size_t offset
    ) const;

    void writeUInt16(
        std::size_t offset,
        std::uint16_t value
    );

    [[nodiscard]] Slot readSlot(
        std::uint32_t slot
    ) const;

    void writeSlot(
        std::uint32_t slot,
        Slot slotData
    );

    [[nodiscard]] std::size_t slotOffset(
        std::uint32_t slot
    ) const noexcept;

    [[nodiscard]] std::size_t recordDataStart() const noexcept;

    [[nodiscard]] std::size_t requiredSpace(
        std::size_t recordSize
    ) const noexcept;

    void validateSlot(
        std::uint32_t slot
    ) const;

    void compactWithUpdatedRecord(std::uint32_t slot, std::span<const Byte> record);

    Page& page_;
};

} // namespace forgedb