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
    if (const auto freeFrame = findFreeFrame(); freeFrame.has_value()) {
        victimFrameId = *freeFrame;
    } else if (!replacer_.victim(victimFrameId)) {
        return nullptr;
    }

    Frame& frame = frames_[victimFrameId];

    // A dirty victim must reach disk before its frame is reused. Otherwise
    // eviction silently discards modifications that have not been flushed.
    if (frame.isOccupied && frame.isDirty) {
        try {
            diskManager_.writePage(frame.page.id(), frame.page);
            frame.isDirty = false;
        } catch (...) {
            // victim() removed this unpinned frame from the replacer; restore
            // its evictable status if the write fails and preserve the page.
            replacer_.unpin(victimFrameId);
            throw;
        }
    }

    // Remove the old page from the page table only after any dirty data is safe.
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
    if (const auto freeFrame = findFreeFrame(); freeFrame.has_value()) {
        frameId = *freeFrame;
    } else if (!replacer_.victim(frameId)) {
        return nullptr;
    }

    Frame& frame = frames_[frameId];

    // Reusing a victim frame must not discard dirty page contents.
    if (frame.isOccupied && frame.isDirty) {
        try {
            diskManager_.writePage(frame.page.id(), frame.page);
            frame.isDirty = false;
        } catch (...) {
            replacer_.unpin(frameId);
            throw;
        }
    }

    // Remove the old page from the page table only after any dirty data is safe.
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
    if (frameId < nextFreeFrame_) {
        nextFreeFrame_ = frameId;
    }

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
    diskManager_.flush();

    frame->isDirty = false;

    return true;
}

void BufferPoolManager::flushAllPages()
{
    bool wroteAnyPage = false;
    for (Frame& frame : frames_) {
        if (!frame.isOccupied || !frame.isDirty) {
            continue;
        }

        diskManager_.writePage(
            frame.page.id(),
            frame.page
        );

        frame.isDirty = false;
        wroteAnyPage = true;
    }

    if (wroteAnyPage) {
        diskManager_.flush();
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

std::optional<std::size_t> BufferPoolManager::findFreeFrame() noexcept {
    // Frames are initially consumed in ascending order. Remember the first
    // possible free slot instead of rescanning the occupied prefix on every
    // allocation. Deletion moves this cursor backward when it creates a hole.
    while (nextFreeFrame_ < frames_.size() &&
           frames_[nextFreeFrame_].isOccupied) {
        ++nextFreeFrame_;
    }

    if (nextFreeFrame_ == frames_.size()) {
        return std::nullopt;
    }

    return nextFreeFrame_;
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