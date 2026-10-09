#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <vector>
#include <optional>
#include <map>
#include <unordered_map>

#include "forgedb/buffer/buffer_pool_manager.h"
#include "forgedb/record/record_id.h"
#include "forgedb/storage/heap_page.h"

namespace forgedb {

class HeapFile {
public:
    HeapFile(
        DiskManager& diskManager,
        BufferPoolManager& bufferPoolManager,
        std::optional<PageId> metadataPageId = std::nullopt
    );

    [[nodiscard]] RecordId insert(
        std::span<const std::uint8_t> record
    );

    [[nodiscard]] std::vector<std::uint8_t> read(
        RecordId recordId
    );

    void update(
        RecordId recordId,
        std::span<const std::uint8_t> record
    );

    void erase(RecordId recordId);

    [[nodiscard]] std::vector<RecordId> scan() const;

    // Visits each live record while its heap page is pinned. The record view
    // is valid only for the duration of the visitor call.
    void forEachRecord(
        const std::function<void(RecordId, std::span<const std::uint8_t>)>& visitor
    ) const;

private:
    [[nodiscard]] Page* fetchPage(PageId pageId);

    [[nodiscard]] Page* createPage(
        PageId& pageId
    );

    [[nodiscard]] const std::vector<PageId>& dataPages() const;
    void persistDataPages(const std::vector<PageId>& pages);
    void initializeFreeSpaceIndex(const std::vector<PageId>& pages);
    void updateFreeSpaceIndex(PageId pageId, std::size_t freeSpace);

    DiskManager& diskManager_;
    BufferPoolManager& bufferPoolManager_;
    std::optional<PageId> metadataPageId_;
    // Cache the persisted page directory; it changes only when a data page is
    // allocated. This avoids decoding the full directory on every insertion.
    mutable std::optional<std::vector<PageId>> dataPagesCache_;
    // Metadata overflow pages form a linked directory when the data-page list
    // no longer fits in a single 4 KiB metadata page.
    mutable std::optional<std::vector<PageId>> metadataDirectoryPagesCache_;
    // V1 directories need a one-time full rewrite to migrate to V2. After
    // migration, appends update only the active tail and a newly linked page.
    mutable bool metadataDirectoryNeedsMigration_{false};
    using FreeSpaceIndex = std::multimap<std::size_t, PageId>;
    using FreeSpaceIndexIterator = FreeSpaceIndex::iterator;
    bool freeSpaceIndexInitialized_{false};
    FreeSpaceIndex freeSpaceIndex_;
    std::unordered_map<PageId::ValueType, FreeSpaceIndexIterator>
        freeSpaceIndexEntries_;
};

} // namespace forgedb