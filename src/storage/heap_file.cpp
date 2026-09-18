#include "forgedb/storage/heap_file.h"
#include "forgedb/storage/serializer.h"

#include <algorithm>
#include <stdexcept>

namespace forgedb {

namespace {
constexpr std::uint64_t kHeapMetadataMagic = 0x464F524745484541ULL;
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

    auto pages = dataPages();

    for (auto id : pages) {
        Page* p = bufferPoolManager_.fetchPage(id);

        if (!p) {
            throw std::runtime_error("HeapFile: unable to fetch page");
        }

        HeapPage hp(*p);

        try {
            auto rid = hp.insert(record);
            bufferPoolManager_.unpinPage(id, true);
            return rid;
        } catch (const std::overflow_error&) {
            bufferPoolManager_.unpinPage(id, false);
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
        bufferPoolManager_.unpinPage(id, true);

        pages.push_back(id);
        persistDataPages(pages);

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
        bufferPoolManager_.unpinPage(id.pageId(), true);
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

Page* HeapFile::fetchPage(PageId id) {
    return bufferPoolManager_.fetchPage(id);
}

Page* HeapFile::createPage(PageId& id) {
    return bufferPoolManager_.newPage(id);
}

std::vector<PageId> HeapFile::dataPages() const {
    if (!metadataPageId_) {
        std::vector<PageId> pages;

        for (std::size_t i = 1; i < diskManager_.pageCount(); ++i) {
            pages.emplace_back(
                PageId{static_cast<PageId::ValueType>(i)}
            );
        }

        return pages;
    }

    Page* p =
        const_cast<BufferPoolManager&>(bufferPoolManager_)
            .fetchPage(*metadataPageId_);

    if (!p) {
        throw std::runtime_error("HeapFile: metadata page unavailable");
    }

    std::vector<PageId> pages;

    auto data = std::span<const std::uint8_t>(
        p->data().data(),
        p->data().size()
    );

    try {
        std::size_t off = 0;

        if (Serializer::readUInt64(data, off) != kHeapMetadataMagic) {
            const_cast<BufferPoolManager&>(bufferPoolManager_)
                .unpinPage(*metadataPageId_, false);

            return pages;
        }

        auto count = Serializer::readUInt32(data, off);
        pages.reserve(count);

        for (std::uint32_t i = 0; i < count; ++i) {
            pages.emplace_back(
                PageId{Serializer::readUInt64(data, off)}
            );
        }
    } catch (...) {
        const_cast<BufferPoolManager&>(bufferPoolManager_)
            .unpinPage(*metadataPageId_, false);

        throw;
    }

    const_cast<BufferPoolManager&>(bufferPoolManager_)
        .unpinPage(*metadataPageId_, false);

    return pages;
}

void HeapFile::persistDataPages(const std::vector<PageId>& pages) {
    if (!metadataPageId_) {
        return;
    }

    Page* p = bufferPoolManager_.fetchPage(*metadataPageId_);

    if (!p) {
        throw std::runtime_error("HeapFile: metadata page unavailable");
    }

    std::vector<std::uint8_t> b;

    Serializer::writeUInt64(b, kHeapMetadataMagic);
    Serializer::writeUInt32(
        b,
        static_cast<std::uint32_t>(pages.size())
    );

    for (auto id : pages) {
        Serializer::writeUInt64(b, id.value());
    }

    if (b.size() > p->data().size()) {
        bufferPoolManager_.unpinPage(*metadataPageId_, false);
        throw std::length_error("HeapFile: metadata page full");
    }

    std::fill(p->data().begin(), p->data().end(), Page::Byte{0});
    std::copy(b.begin(), b.end(), p->data().begin());

    bufferPoolManager_.unpinPage(*metadataPageId_, true);
}

}  // namespace forgedb
