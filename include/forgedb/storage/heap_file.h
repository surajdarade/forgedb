#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>
#include <optional>

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

private:
    [[nodiscard]] Page* fetchPage(PageId pageId);

    [[nodiscard]] Page* createPage(
        PageId& pageId
    );

    [[nodiscard]] std::vector<PageId> dataPages() const;
    void persistDataPages(const std::vector<PageId>& pages);

    DiskManager& diskManager_;
    BufferPoolManager& bufferPoolManager_;
    std::optional<PageId> metadataPageId_;
};

} // namespace forgedb