#pragma once

#include <cstddef>
#include <unordered_map>
#include <vector>

#include "forgedb/buffer/lru_replacer.h"
#include "forgedb/common/page_id.h"
#include "forgedb/storage/disk_manager.h"
#include "forgedb/storage/page.h"

namespace forgedb {

class BufferPoolManager {
public:
    explicit BufferPoolManager(
        std::size_t poolSize,
        DiskManager& diskManager
    );

    ~BufferPoolManager();

    BufferPoolManager(const BufferPoolManager&) = delete;
    BufferPoolManager& operator=(const BufferPoolManager&) = delete;

    BufferPoolManager(BufferPoolManager&&) = delete;
    BufferPoolManager& operator=(BufferPoolManager&&) = delete;

    // Fetch an existing page into the buffer pool.
    // The returned page is pinned.
    // Returns nullptr if no frame is available.
    [[nodiscard]] Page* fetchPage(PageId pageId);

    // Allocate a new page and place it in the buffer pool.
    // The returned page is pinned.
    // Returns nullptr if no frame is available.
    [[nodiscard]] Page* newPage(PageId& pageId);

    // Unpin a page.
    // A page becomes eligible for eviction when its pin count reaches zero.
    bool unpinPage(PageId pageId, bool isDirty);

    // Delete a page from the buffer pool.
    // Returns false if the page is pinned.
    bool deletePage(PageId pageId);

    // Flush a page currently residing in the buffer pool.
    bool flushPage(PageId pageId);

    // Flush all dirty pages currently residing in the buffer pool.
    void flushAllPages();

    [[nodiscard]] std::size_t poolSize() const noexcept;

private:
    struct Frame {
        Page page{};
        std::size_t pinCount{0};
        bool isDirty{false};
        bool isOccupied{false};
    };

    [[nodiscard]] Frame* findFrame(PageId pageId) noexcept;

    [[nodiscard]] const Frame* findFrame(PageId pageId) const noexcept;

    [[nodiscard]] std::size_t frameIndex(
        const Frame& frame
    ) const noexcept;

    void resetFrame(Frame& frame) noexcept;

    std::size_t poolSize_;
    DiskManager& diskManager_;

    std::vector<Frame> frames_;

    std::unordered_map<
        PageId::ValueType,
        std::size_t
    > pageTable_;

    LRUReplacer replacer_;
};

} // namespace forgedb