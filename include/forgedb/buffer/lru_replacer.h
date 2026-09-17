#pragma once

#include <cstddef>
#include <list>
#include <unordered_map>

namespace forgedb {

class LRUReplacer {
public:
    explicit LRUReplacer(std::size_t capacity);

    LRUReplacer(const LRUReplacer&) = delete;
    LRUReplacer& operator=(const LRUReplacer&) = delete;

    LRUReplacer(LRUReplacer&&) = delete;
    LRUReplacer& operator=(LRUReplacer&&) = delete;

    // Select and remove the least recently used unpinned frame.
    // Returns false if no frame is available for eviction.
    bool victim(std::size_t& frameId);

    // Mark a frame as pinned so it cannot be evicted.
    void pin(std::size_t frameId);

    // Mark a frame as unpinned and eligible for eviction.
    void unpin(std::size_t frameId);

    [[nodiscard]] std::size_t size() const noexcept;

private:
    using FrameId = std::size_t;
    using LruList = std::list<FrameId>;
    using Iterator = LruList::iterator;

    std::size_t capacity_;

    // Front = least recently used.
    // Back = most recently used.
    LruList lruList_;

    // Frame ID -> position in lruList_.
    std::unordered_map<FrameId, Iterator> frameTable_;
};

} // namespace forgedb