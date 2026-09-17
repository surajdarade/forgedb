#include "forgedb/buffer/buffer_pool_manager.h"

#include <stdexcept>

namespace forgedb {

BufferPoolManager::BufferPoolManager(
    std::size_t poolSize,
    DiskManager& diskManager)
    : poolSize_(poolSize),
      diskManager_(diskManager),
      frames_(poolSize),
      replacer_(poolSize)
{
    if (poolSize_ == 0) {
        throw std::invalid_argument(
            "BufferPoolManager: pool size must be greater than zero"
        );
    }

    pageTable_.reserve(poolSize_);
}

BufferPoolManager::~BufferPoolManager()
{
    flushAllPages();
}

Page* BufferPoolManager::fetchPage(PageId pageId)
{
    // Page is already present in the buffer pool.
    if (Frame* frame = findFrame(pageId); frame != nullptr) {
        ++frame->pinCount;

        // A pinned frame must not be considered for eviction.
        replacer_.pin(frameIndex(*frame));

        return &frame->page;
    }

    // Obtain an available frame from the LRU replacer.
    std::size_t victimFrameId;

    if (!replacer_.victim(victimFrameId)) {
        return nullptr;
    }

    Frame& frame = frames_[victimFrameId];

    // Remove the old page from the page table.
    if (frame.isOccupied) {
        pageTable_.erase(frame.page.id().value());
    }

    // Load the requested page from disk.
    diskManager_.readPage(pageId, frame.page);

    frame.pinCount = 1;
    frame.isDirty = false;
    frame.isOccupied = true;

    pageTable_[pageId.value()] = victimFrameId;

    return &frame.page;
}

Page* BufferPoolManager::newPage(PageId& pageId)
{
    std::size_t frameId;

    if (!replacer_.victim(frameId)) {
        return nullptr;
    }

    Frame& frame = frames_[frameId];

    // Remove the old page from the page table.
    if (frame.isOccupied) {
        pageTable_.erase(frame.page.id().value());
    }

    // Allocate a physical page on disk.
    pageId = diskManager_.allocatePage();

    // Reset the frame before assigning the new page.
    resetFrame(frame);

    frame.page.setId(pageId);
    frame.pinCount = 1;
    frame.isDirty = true;
    frame.isOccupied = true;

    pageTable_[pageId.value()] = frameId;

    return &frame.page;
}

bool BufferPoolManager::unpinPage(
    PageId pageId,
    bool isDirty)
{
    Frame* frame = findFrame(pageId);

    if (frame == nullptr) {
        return false;
    }

    if (frame->pinCount == 0) {
        return false;
    }

    --frame->pinCount;

    if (isDirty) {
        frame->isDirty = true;
    }

    // Once the page is completely unpinned,
    // make its frame eligible for LRU eviction.
    if (frame->pinCount == 0) {
        replacer_.unpin(frameIndex(*frame));
    }

    return true;
}

bool BufferPoolManager::deletePage(PageId pageId)
{
    Frame* frame = findFrame(pageId);

    if (frame == nullptr) {
        return true;
    }

    if (frame->pinCount > 0) {
        return false;
    }

    const std::size_t frameId = frameIndex(*frame);

    pageTable_.erase(pageId.value());

    replacer_.pin(frameId);

    resetFrame(*frame);

    return true;
}

bool BufferPoolManager::flushPage(PageId pageId)
{
    Frame* frame = findFrame(pageId);

    if (frame == nullptr) {
        return false;
    }

    diskManager_.writePage(
        pageId,
        frame->page
    );

    frame->isDirty = false;

    return true;
}

void BufferPoolManager::flushAllPages()
{
    for (Frame& frame : frames_) {
        if (!frame.isOccupied || !frame.isDirty) {
            continue;
        }

        diskManager_.writePage(
            frame.page.id(),
            frame.page
        );

        frame.isDirty = false;
    }
}

std::size_t BufferPoolManager::poolSize() const noexcept
{
    return poolSize_;
}

BufferPoolManager::Frame*
BufferPoolManager::findFrame(PageId pageId) noexcept
{
    const auto iterator =
        pageTable_.find(pageId.value());

    if (iterator == pageTable_.end()) {
        return nullptr;
    }

    return &frames_[iterator->second];
}

const BufferPoolManager::Frame*
BufferPoolManager::findFrame(PageId pageId) const noexcept
{
    const auto iterator =
        pageTable_.find(pageId.value());

    if (iterator == pageTable_.end()) {
        return nullptr;
    }

    return &frames_[iterator->second];
}

std::size_t BufferPoolManager::frameIndex(
    const Frame& frame) const noexcept
{
    return static_cast<std::size_t>(
        &frame - frames_.data()
    );
}

void BufferPoolManager::resetFrame(Frame& frame) noexcept
{
    frame.page = Page{};
    frame.pinCount = 0;
    frame.isDirty = false;
    frame.isOccupied = false;
}

} // namespace forgedb