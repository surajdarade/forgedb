#include "forgedb/storage/heap_file.h"

#include <stdexcept>

namespace forgedb {

HeapFile::HeapFile(
    DiskManager& diskManager,
    BufferPoolManager& bufferPoolManager)
    : diskManager_(diskManager),
      bufferPoolManager_(bufferPoolManager)
{
}

RecordId HeapFile::insert(
    std::span<const std::uint8_t> record)
{
    if (record.empty()) {
        throw std::invalid_argument(
            "HeapFile: record cannot be empty"
        );
    }

    // Try existing data pages first.
    //
    // Page 0 is reserved for database metadata.
    // Data pages therefore start at PageId 1.
    const std::size_t pageCount =
        diskManager_.pageCount();

    for (std::size_t pageIndex = 1;
         pageIndex < pageCount;
         ++pageIndex) {

        const PageId pageId{
            static_cast<PageId::ValueType>(pageIndex)
        };

        Page* page =
            bufferPoolManager_.fetchPage(pageId);

        if (page == nullptr) {
            throw std::runtime_error(
                "HeapFile: unable to fetch page"
            );
        }

        HeapPage heapPage{*page};

        try {
            const RecordId recordId =
                heapPage.insert(record);

            bufferPoolManager_.unpinPage(
                pageId,
                true
            );

            return recordId;
        }
        catch (const std::overflow_error&) {
            bufferPoolManager_.unpinPage(
                pageId,
                false
            );
        }
    }

    // No existing page has enough space.
    // Create a new one.
    PageId pageId;

    Page* page =
        bufferPoolManager_.newPage(pageId);

    if (page == nullptr) {
        throw std::runtime_error(
            "HeapFile: unable to allocate new page"
        );
    }

    HeapPage heapPage{*page};

    try {
        const RecordId recordId =
            heapPage.insert(record);

        bufferPoolManager_.unpinPage(
            pageId,
            true
        );

        return recordId;
    }
    catch (...) {
        bufferPoolManager_.unpinPage(
            pageId,
            false
        );

        throw;
    }
}

std::vector<std::uint8_t> HeapFile::read(
    RecordId recordId)
{
    if (!recordId.isValid()) {
        throw std::invalid_argument(
            "HeapFile: invalid record ID"
        );
    }

    Page* page =
        fetchPage(recordId.pageId());

    if (page == nullptr) {
        throw std::runtime_error(
            "HeapFile: unable to fetch page"
        );
    }

    try {
        HeapPage heapPage{*page};

        auto record =
            heapPage.read(recordId.slot());

        bufferPoolManager_.unpinPage(
            recordId.pageId(),
            false
        );

        return record;
    }
    catch (...) {
        bufferPoolManager_.unpinPage(
            recordId.pageId(),
            false
        );

        throw;
    }
}

void HeapFile::update(
    RecordId recordId,
    std::span<const std::uint8_t> record)
{
    if (!recordId.isValid()) {
        throw std::invalid_argument(
            "HeapFile: invalid record ID"
        );
    }

    Page* page =
        fetchPage(recordId.pageId());

    if (page == nullptr) {
        throw std::runtime_error(
            "HeapFile: unable to fetch page"
        );
    }

    try {
        HeapPage heapPage{*page};

        heapPage.update(
            recordId.slot(),
            record
        );

        bufferPoolManager_.unpinPage(
            recordId.pageId(),
            true
        );
    }
    catch (...) {
        bufferPoolManager_.unpinPage(
            recordId.pageId(),
            false
        );

        throw;
    }
}

void HeapFile::erase(RecordId recordId)
{
    if (!recordId.isValid()) {
        throw std::invalid_argument(
            "HeapFile: invalid record ID"
        );
    }

    Page* page =
        fetchPage(recordId.pageId());

    if (page == nullptr) {
        throw std::runtime_error(
            "HeapFile: unable to fetch page"
        );
    }

    try {
        HeapPage heapPage{*page};

        heapPage.erase(recordId.slot());

        bufferPoolManager_.unpinPage(
            recordId.pageId(),
            true
        );
    }
    catch (...) {
        bufferPoolManager_.unpinPage(
            recordId.pageId(),
            false
        );

        throw;
    }
}

Page* HeapFile::fetchPage(PageId pageId)
{
    return bufferPoolManager_.fetchPage(pageId);
}

Page* HeapFile::createPage(PageId& pageId)
{
    return bufferPoolManager_.newPage(pageId);
}

} // namespace forgedb