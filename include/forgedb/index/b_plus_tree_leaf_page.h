#pragma once

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include "forgedb/index/index_page.h"
#include "forgedb/index/index_key.h"
#include "forgedb/record/record_id.h"

namespace forgedb {

class PersistentBPlusTree;

class BPlusTreeLeafPage final : public IndexPage {
public:
    struct Entry {
        IndexKey key;
        RecordId recordId;
    };

    static constexpr std::size_t kNextPageIdOffset =
        IndexPage::kHeaderSize;

    static constexpr std::size_t kHeaderSize =
        IndexPage::kHeaderSize + sizeof(std::uint64_t);

    explicit BPlusTreeLeafPage(Page& page);

    [[nodiscard]] std::size_t size() const noexcept;

    [[nodiscard]] bool isEmpty() const noexcept;

    [[nodiscard]] bool isFull() const noexcept;

    [[nodiscard]] bool isUnderflow(
        std::size_t minimumEntries
    ) const noexcept;

    [[nodiscard]] PageId nextPageId() const noexcept;

    void setNextPageId(PageId pageId) noexcept;

    [[nodiscard]] IndexKey keyAt(
        std::size_t index
    ) const;

    [[nodiscard]] RecordId recordIdAt(
        std::size_t index
    ) const;

    [[nodiscard]] std::size_t lowerBound(
        const IndexKey& key
    ) const;

    [[nodiscard]] std::size_t upperBound(
        const IndexKey& key
    ) const;

    // Searches serialized entries directly. When provided, continueToNextPage
    // is false once this leaf proves the key cannot appear in later leaves.
    [[nodiscard]] std::vector<RecordId> lookup(
        const IndexKey& key,
        bool* continueToNextPage = nullptr
    ) const;

    // Scans one serialized leaf without materializing all entries. The output
    // includes both bounds; continueToNextPage is false once this leaf proves
    // that later leaves cannot contain another match.
    [[nodiscard]] std::vector<RecordId> scanRange(
        const IndexKey& lower,
        const IndexKey& upper,
        bool* continueToNextPage = nullptr
    ) const;

    [[nodiscard]] Entry entryAt(std::size_t index) const;

    [[nodiscard]] std::vector<Entry> entries() const;

    // Returns the greatest entry in this leaf without materializing every entry.
    [[nodiscard]] Entry lastEntry() const;

    void rewrite(const std::vector<Entry>& entries);

    [[nodiscard]] bool insert(
        const IndexKey& key,
        RecordId recordId
    );

    [[nodiscard]] bool remove(
        const IndexKey& key,
        RecordId recordId
    );

    [[nodiscard]] std::size_t freeSpace() const noexcept;

private:
    friend class PersistentBPlusTree;

    static constexpr std::size_t kEntryHeaderSize =
        sizeof(std::uint8_t) +
        sizeof(std::uint32_t) +
        sizeof(std::uint64_t) +
        sizeof(std::uint32_t);

    [[nodiscard]] std::size_t entrySize(
        const Entry& entry
    ) const;

    [[nodiscard]] Entry readEntry(
        std::size_t index
    ) const;

    void writeEntry(
        std::size_t offset,
        const Entry& entry
    );

    [[nodiscard]] std::vector<Entry> readEntries() const;

    [[nodiscard]] std::pair<Entry, std::size_t> lastEntryAndEndOffset() const;

    // Used by PersistentBPlusTree's monotonic-insert path. The caller must
    // have established that the new key sorts after the current last key.
    [[nodiscard]] std::size_t appendAtEnd(
        const IndexKey& key,
        RecordId recordId,
        std::size_t endOffset);

    [[nodiscard]] std::size_t serializedEndOffset() const;

    void rewriteEntries(
        const std::vector<Entry>& entries
    );

    [[nodiscard]] static bool entryLess(
        const Entry& lhs,
        const Entry& rhs
    );

    [[nodiscard]] static bool entryEqual(
        const Entry& lhs,
        const Entry& rhs
    );

    [[nodiscard]] static std::size_t serializedKeySize(
        const IndexKey& key
    );

    [[nodiscard]] static std::size_t serializedEntrySize(
        const Entry& entry
    );
};

} // namespace forgedb