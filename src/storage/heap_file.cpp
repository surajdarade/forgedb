#include "forgedb/storage/heap_file.h"
#include "forgedb/storage/serializer.h"

#include <algorithm>
#include <stdexcept>

namespace forgedb {

namespace {
// V1 stored the complete data-page directory in one metadata page. Keep
// reading it for compatibility, but write the chained V2 format going forward.
constexpr std::uint64_t kHeapMetadataMagicV1 = 0x464F524745484541ULL;
constexpr std::uint64_t kHeapMetadataMagicV2 = 0x464F524745484532ULL;
constexpr std::size_t kHeapMetadataV1HeaderSize = sizeof(std::uint64_t) + sizeof(std::uint32_t);
constexpr std::size_t kHeapMetadataV2HeaderSize = sizeof(std::uint64_t) + sizeof(std::uint32_t) + sizeof(std::uint64_t);
constexpr std::size_t kHeapMetadataV2EntriesPerPage =
    (kPageSize - kHeapMetadataV2HeaderSize) / sizeof(std::uint64_t);
}  // namespace

HeapFile::HeapFile(
    DiskManager& d,
    BufferPoolManager& b,
    std::optional<PageId> m
)
    : diskManager_(d),
      bufferPoolManager_(b),
      metadataPageId_(m) {}

RecordId HeapFile::insert(std::span<const std::uint8_t> record) {
    if (record.empty()) {
        throw std::invalid_argument("HeapFile: record cannot be empty");
    }

    const auto& pages = dataPages();
    initializeFreeSpaceIndex(pages);

    // Keep the append path cheap. Older pages are considered through the
    // free-space index, rather than fetching every previously allocated page
    // on each insert once the newest page is full.
    const auto tryInsert = [&](PageId id) -> std::optional<RecordId> {
        Page* p = bufferPoolManager_.fetchPage(id);
        if (!p) {
            throw std::runtime_error("HeapFile: unable to fetch page");
        }

        HeapPage hp(*p);
        std::optional<RecordId> inserted;
        try {
            inserted = hp.insert(record);
        } catch (const std::overflow_error&) {
            const auto remaining = hp.freeSpace();
            if (!bufferPoolManager_.unpinPage(id, false)) {
                throw std::runtime_error("HeapFile: failed to unpin full page");
            }
            updateFreeSpaceIndex(id, remaining);
            return std::nullopt;
        } catch (...) {
            bufferPoolManager_.unpinPage(id, false);
            throw;
        }

        const auto remaining = hp.freeSpace();
        if (!bufferPoolManager_.unpinPage(id, true)) {
            throw std::runtime_error("HeapFile: failed to unpin inserted page");
        }
        updateFreeSpaceIndex(id, remaining);
        return inserted;
    };

    if (!pages.empty()) {
        if (auto rid = tryInsert(pages.back())) {
            return *rid;
        }

        const std::size_t requiredSpace =
            record.size() + HeapPage::kSlotSize;
        auto candidate = freeSpaceIndex_.lower_bound(requiredSpace);
        while (candidate != freeSpaceIndex_.end()) {
            const PageId candidateId = candidate->second;
            ++candidate;
            if (auto rid = tryInsert(candidateId)) {
                return *rid;
            }
        }
    }

    PageId id;
    Page* p = bufferPoolManager_.newPage(id);

    if (!p) {
        throw std::runtime_error("HeapFile: unable to allocate page");
    }

    HeapPage hp(*p);

    try {
        auto rid = hp.insert(record);
        const auto remaining = hp.freeSpace();
        bufferPoolManager_.unpinPage(id, true);

        // With a persisted heap, append directly to the cached directory rather
        // than copying the entire page-id vector on every insert. If persistence
        // fails, roll back the in-memory directory entry.
        if (metadataPageId_) {
            auto& cachedPages = *dataPagesCache_;
            cachedPages.push_back(id);
            try {
                persistDataPages(cachedPages);
            } catch (...) {
                cachedPages.pop_back();
                throw;
            }
        }
        updateFreeSpaceIndex(id, remaining);

        return rid;
    } catch (...) {
        bufferPoolManager_.unpinPage(id, false);
        throw;
    }
}

std::vector<std::uint8_t> HeapFile::read(RecordId id) {
    if (!id.isValid() ||
        id.pageId().value() == 0 ||
        id.pageId().value() >= diskManager_.pageCount()) {
        throw std::invalid_argument("HeapFile: invalid record ID");
    }

    Page* p = fetchPage(id.pageId());

    if (!p) {
        throw std::runtime_error("HeapFile: unable to fetch page");
    }

    try {
        HeapPage hp(*p);
        auto r = hp.read(id.slot());
        bufferPoolManager_.unpinPage(id.pageId(), false);
        return r;
    } catch (...) {
        bufferPoolManager_.unpinPage(id.pageId(), false);
        throw;
    }
}

void HeapFile::update(
    RecordId id,
    std::span<const std::uint8_t> record
) {
    if (!id.isValid() ||
        id.pageId().value() == 0 ||
        id.pageId().value() >= diskManager_.pageCount()) {
        throw std::invalid_argument("HeapFile: invalid record ID");
    }

    Page* p = fetchPage(id.pageId());

    if (!p) {
        throw std::runtime_error("HeapFile: unable to fetch page");
    }

    try {
        HeapPage hp(*p);
        hp.update(id.slot(), record);
        const auto remaining = hp.freeSpace();
        bufferPoolManager_.unpinPage(id.pageId(), true);
        if (freeSpaceIndexInitialized_) {
            updateFreeSpaceIndex(id.pageId(), remaining);
        }
    } catch (...) {
        bufferPoolManager_.unpinPage(id.pageId(), false);
        throw;
    }
}

void HeapFile::erase(RecordId id) {
    if (!id.isValid() ||
        id.pageId().value() == 0 ||
        id.pageId().value() >= diskManager_.pageCount()) {
        throw std::invalid_argument("HeapFile: invalid record ID");
    }

    Page* p = fetchPage(id.pageId());

    if (!p) {
        throw std::runtime_error("HeapFile: unable to fetch page");
    }

    try {
        HeapPage hp(*p);
        hp.erase(id.slot());
        bufferPoolManager_.unpinPage(id.pageId(), true);
    } catch (...) {
        bufferPoolManager_.unpinPage(id.pageId(), false);
        throw;
    }
}

std::vector<RecordId> HeapFile::scan() const {
    std::vector<RecordId> ids;

    for (auto pageId : dataPages()) {
        Page* p =
            const_cast<BufferPoolManager&>(bufferPoolManager_).fetchPage(pageId);

        if (!p) {
            throw std::runtime_error("HeapFile: unable to fetch page");
        }

        HeapPage hp(*p);

        for (std::size_t i = 0; i < hp.recordCount(); ++i) {
            if (!hp.isDeleted(static_cast<std::uint32_t>(i))) {
                ids.emplace_back(
                    pageId,
                    static_cast<std::uint32_t>(i)
                );
            }
        }

        const_cast<BufferPoolManager&>(bufferPoolManager_)
            .unpinPage(pageId, false);
    }

    return ids;
}

void HeapFile::forEachRecord(
    const std::function<void(RecordId, std::span<const std::uint8_t>)>& visitor
) const {
    if (!visitor) {
        throw std::invalid_argument("HeapFile: record visitor cannot be empty");
    }

    auto& bufferPool = const_cast<BufferPoolManager&>(bufferPoolManager_);
    for (const PageId pageId : dataPages()) {
        Page* page = bufferPool.fetchPage(pageId);
        if (!page) {
            throw std::runtime_error("HeapFile: unable to fetch page during scan");
        }

        try {
            HeapPage heapPage(*page);
            const auto count = heapPage.recordCount();
            for (std::size_t slot = 0; slot < count; ++slot) {
                const auto slotId = static_cast<std::uint32_t>(slot);
                if (heapPage.isDeleted(slotId)) {
                    continue;
                }

                // The page stays pinned until the visitor has consumed the view.
                visitor(RecordId{pageId, slotId}, heapPage.readView(slotId));
            }
        } catch (...) {
            bufferPool.unpinPage(pageId, false);
            throw;
        }

        if (!bufferPool.unpinPage(pageId, false)) {
            throw std::runtime_error("HeapFile: failed to unpin scanned page");
        }
    }
}

Page* HeapFile::fetchPage(PageId id) {
    return bufferPoolManager_.fetchPage(id);
}

Page* HeapFile::createPage(PageId& id) {
    return bufferPoolManager_.newPage(id);
}

void HeapFile::initializeFreeSpaceIndex(const std::vector<PageId>& pages) {
    if (freeSpaceIndexInitialized_) {
        return;
    }

    freeSpaceIndexInitialized_ = true;
    try {
        for (const PageId id : pages) {
            Page* p = bufferPoolManager_.fetchPage(id);
            if (!p) {
                throw std::runtime_error("HeapFile: unable to fetch page for free-space index");
            }

            std::size_t remaining = 0;
            try {
                HeapPage hp(*p);
                remaining = hp.freeSpace();
                if (!bufferPoolManager_.unpinPage(id, false)) {
                    throw std::runtime_error("HeapFile: failed to unpin indexed page");
                }
            } catch (...) {
                bufferPoolManager_.unpinPage(id, false);
                throw;
            }
            updateFreeSpaceIndex(id, remaining);
        }
    } catch (...) {
        freeSpaceIndex_.clear();
        freeSpaceIndexEntries_.clear();
        freeSpaceIndexInitialized_ = false;
        throw;
    }
}

void HeapFile::updateFreeSpaceIndex(PageId pageId, std::size_t freeSpace) {
    if (!freeSpaceIndexInitialized_) {
        return;
    }

    const auto key = pageId.value();
    const auto existing = freeSpaceIndexEntries_.find(key);
    if (existing != freeSpaceIndexEntries_.end()) {
        freeSpaceIndex_.erase(existing->second);
        freeSpaceIndexEntries_.erase(existing);
    }

    const auto inserted = freeSpaceIndex_.emplace(freeSpace, pageId);
    freeSpaceIndexEntries_.emplace(key, inserted);
}

const std::vector<PageId>& HeapFile::dataPages() const {
    if (metadataPageId_ && dataPagesCache_) {
        return *dataPagesCache_;
    }

    if (!metadataPageId_) {
        // Legacy heaps without a metadata page treat every allocated page as a
        // data page. Refresh this cache on each call because other components
        // may allocate pages through the shared DiskManager.
        dataPagesCache_.emplace();
        auto& pages = *dataPagesCache_;
        pages.clear();
        for (std::size_t i = 1; i < diskManager_.pageCount(); ++i) {
            pages.emplace_back(PageId{static_cast<PageId::ValueType>(i)});
        }
        return pages;
    }

    std::vector<PageId> pages;
    std::vector<PageId> directoryPages;
    PageId current = *metadataPageId_;
    bool firstPage = true;

    while (current.value() != 0) {
        // A directory chain cannot legitimately contain more pages than the
        // database itself. This also guards against corrupted cyclic links.
        if (directoryPages.size() >= diskManager_.pageCount()) {
            throw std::runtime_error("HeapFile: metadata directory chain is cyclic or invalid");
        }

        Page* page = const_cast<BufferPoolManager&>(bufferPoolManager_).fetchPage(current);
        if (!page) {
            throw std::runtime_error("HeapFile: metadata page unavailable");
        }

        auto data = std::span<const std::uint8_t>(page->data().data(), page->data().size());
        bool pinned = true;
        try {
            std::size_t offset = 0;
            const auto magic = Serializer::readUInt64(data, offset);

            if (firstPage && magic == kHeapMetadataMagicV1) {
                metadataDirectoryNeedsMigration_ = true;
                const auto count = Serializer::readUInt32(data, offset);
                if (count > (data.size() - offset) / sizeof(std::uint64_t)) {
                    throw std::runtime_error("HeapFile: invalid V1 metadata page count");
                }
                pages.reserve(count);
                for (std::uint32_t i = 0; i < count; ++i) {
                    pages.emplace_back(PageId{Serializer::readUInt64(data, offset)});
                }
                directoryPages.push_back(current);
                const bool unpinned = const_cast<BufferPoolManager&>(bufferPoolManager_).unpinPage(current, false);
                pinned = false;
                if (!unpinned) throw std::runtime_error("HeapFile: failed to unpin metadata page");
                break;
            }

            if (magic != kHeapMetadataMagicV2) {
                if (!firstPage) {
                    throw std::runtime_error("HeapFile: invalid metadata overflow page magic");
                }
                // A new, zeroed metadata page means the heap has no data pages yet.
                directoryPages.push_back(current);
                const bool unpinned = const_cast<BufferPoolManager&>(bufferPoolManager_).unpinPage(current, false);
                pinned = false;
                if (!unpinned) throw std::runtime_error("HeapFile: failed to unpin empty metadata page");
                metadataDirectoryPagesCache_ = directoryPages;
                dataPagesCache_ = std::move(pages);
                return *dataPagesCache_;
            }

            const auto count = Serializer::readUInt32(data, offset);
            const PageId next{Serializer::readUInt64(data, offset)};
            if (count > (data.size() - offset) / sizeof(std::uint64_t)) {
                throw std::runtime_error("HeapFile: invalid V2 metadata page count");
            }
            pages.reserve(pages.size() + count);
            for (std::uint32_t i = 0; i < count; ++i) {
                pages.emplace_back(PageId{Serializer::readUInt64(data, offset)});
            }
            directoryPages.push_back(current);
            const bool unpinned = const_cast<BufferPoolManager&>(bufferPoolManager_).unpinPage(current, false);
            pinned = false;
            if (!unpinned) throw std::runtime_error("HeapFile: failed to unpin metadata page");
            current = next;
            firstPage = false;
        } catch (...) {
            if (pinned) const_cast<BufferPoolManager&>(bufferPoolManager_).unpinPage(current, false);
            throw;
        }
    }

    metadataDirectoryPagesCache_ = std::move(directoryPages);
    dataPagesCache_ = std::move(pages);
    return *dataPagesCache_;
}

void HeapFile::persistDataPages(const std::vector<PageId>& pages) {
    if (!metadataPageId_) {
        return;
    }

    if (!metadataDirectoryPagesCache_) {
        metadataDirectoryPagesCache_ = std::vector<PageId>{*metadataPageId_};
    }

    const std::size_t neededPages = std::max<std::size_t>(
        1, (pages.size() + kHeapMetadataV2EntriesPerPage - 1) / kHeapMetadataV2EntriesPerPage);
    const std::size_t previousDirectoryPageCount = metadataDirectoryPagesCache_->size();

    while (metadataDirectoryPagesCache_->size() < neededPages) {
        PageId newMetadataPageId;
        Page* newMetadataPage = bufferPoolManager_.newPage(newMetadataPageId);
        if (!newMetadataPage) {
            throw std::runtime_error("HeapFile: unable to allocate metadata overflow page");
        }
        if (!bufferPoolManager_.unpinPage(newMetadataPageId, false)) {
            throw std::runtime_error("HeapFile: failed to unpin new metadata overflow page");
        }
        metadataDirectoryPagesCache_->push_back(newMetadataPageId);
    }

    // Appending one data page changes only the current tail directory page.
    // If a new directory page was needed, also rewrite its predecessor to link
    // to the new page. A legacy V1 directory is rewritten in full exactly once.
    std::vector<std::size_t> pageIndices;
    if (metadataDirectoryNeedsMigration_) {
        pageIndices.reserve(neededPages);
        for (std::size_t i = neededPages; i-- > 0;) {
            pageIndices.push_back(i);
        }
    } else {
        pageIndices.push_back(neededPages - 1);
        if (neededPages > previousDirectoryPageCount && neededPages > 1) {
            pageIndices.push_back(neededPages - 2);
        }
    }

    for (const std::size_t pageIndex : pageIndices) {
        const PageId metadataId = (*metadataDirectoryPagesCache_)[pageIndex];
        Page* metadataPage = bufferPoolManager_.fetchPage(metadataId);
        if (!metadataPage) {
            throw std::runtime_error("HeapFile: metadata page unavailable");
        }

        const std::size_t firstEntry = pageIndex * kHeapMetadataV2EntriesPerPage;
        const std::size_t entryCount = firstEntry < pages.size()
            ? std::min(kHeapMetadataV2EntriesPerPage, pages.size() - firstEntry)
            : 0;
        const PageId next = pageIndex + 1 < neededPages
            ? (*metadataDirectoryPagesCache_)[pageIndex + 1]
            : PageId{0};

        std::vector<std::uint8_t> bytes;
        bytes.reserve(kHeapMetadataV2HeaderSize + entryCount * sizeof(std::uint64_t));
        Serializer::writeUInt64(bytes, kHeapMetadataMagicV2);
        Serializer::writeUInt32(bytes, static_cast<std::uint32_t>(entryCount));
        Serializer::writeUInt64(bytes, next.value());
        for (std::size_t i = 0; i < entryCount; ++i) {
            Serializer::writeUInt64(bytes, pages[firstEntry + i].value());
        }

        std::fill(metadataPage->data().begin(), metadataPage->data().end(), Page::Byte{0});
        std::copy(bytes.begin(), bytes.end(), metadataPage->data().begin());
        if (!bufferPoolManager_.unpinPage(metadataId, true)) {
            throw std::runtime_error("HeapFile: failed to persist metadata page");
        }
    }

    metadataDirectoryNeedsMigration_ = false;
    if (!dataPagesCache_ || &pages != &*dataPagesCache_) {
        dataPagesCache_ = pages;
    }
}

}  // namespace forgedb
