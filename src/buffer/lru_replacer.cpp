#include "forgedb/buffer/lru_replacer.h"

#include <stdexcept>

namespace forgedb {

LRUReplacer::LRUReplacer(std::size_t capacity)
    : capacity_(capacity)
{
    if (capacity_ == 0) {
        throw std::invalid_argument(
            "LRUReplacer: capacity must be greater than zero"
        );
    }

    frameTable_.reserve(capacity_);
}

bool LRUReplacer::victim(std::size_t& frameId)
{
    if (lruList_.empty()) {
        return false;
    }

    // The front contains the least recently used frame.
    frameId = lruList_.front();

    lruList_.pop_front();

    frameTable_.erase(frameId);

    return true;
}

void LRUReplacer::pin(std::size_t frameId)
{
    const auto iterator = frameTable_.find(frameId);

    if (iterator == frameTable_.end()) {
        return;
    }

    lruList_.erase(iterator->second);
    frameTable_.erase(iterator);
}

void LRUReplacer::unpin(std::size_t frameId)
{
    // Already unpinned.
    if (frameTable_.contains(frameId)) {
        return;
    }

    // Do not exceed the configured capacity.
    if (lruList_.size() >= capacity_) {
        return;
    }

    lruList_.push_back(frameId);

    auto iterator = std::prev(lruList_.end());

    frameTable_.emplace(frameId, iterator);
}

std::size_t LRUReplacer::size() const noexcept
{
    return lruList_.size();
}

} // namespace forgedb