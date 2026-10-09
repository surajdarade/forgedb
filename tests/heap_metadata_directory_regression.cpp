#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <vector>

#include "forgedb/buffer/buffer_pool_manager.h"
#include "forgedb/storage/disk_manager.h"
#include "forgedb/storage/heap_file.h"
#include "forgedb/storage/heap_page.h"
#include "forgedb/storage/serializer.h"

using namespace forgedb;

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
}

int main() {
    const auto path = std::filesystem::temp_directory_path() /
        "forgedb_heap_metadata_directory_regression.db";
    std::error_code error;
    std::filesystem::remove(path, error);

    constexpr std::size_t kRecords = 520;
    const std::vector<std::uint8_t> record(4000, 0x5A);
    PageId metadataPageId{};

    try {
        {
            DiskManager disk(path.string());
            BufferPoolManager pool(16, disk);
            Page* metadata = pool.newPage(metadataPageId);
            require(metadata != nullptr, "failed to allocate heap metadata page");
            require(pool.unpinPage(metadataPageId, true), "failed to unpin heap metadata page");

            HeapFile heap(disk, pool, metadataPageId);
            for (std::size_t i = 0; i < kRecords; ++i) {
                (void)heap.insert(record);
            }
            const auto ids = heap.scan();
            require(ids.size() == kRecords, "scan count mismatch after metadata overflow");
            require(heap.read(ids.front()) == record, "first record mismatch before reopen");
            require(heap.read(ids.back()) == record, "last record mismatch before reopen");
        }

        {
            DiskManager disk(path.string());
            BufferPoolManager pool(16, disk);
            HeapFile heap(disk, pool, metadataPageId);
            const auto ids = heap.scan();
            require(ids.size() == kRecords, "scan count mismatch after metadata-directory reopen");
            require(heap.read(ids.front()) == record, "first record mismatch after reopen");
            require(heap.read(ids.back()) == record, "last record mismatch after reopen");
        }

        std::filesystem::remove(path, error);

        // Legacy V1 metadata must remain readable and migrate on the first
        // insert that allocates another data page.
        const auto legacyPath = std::filesystem::temp_directory_path() /
            "forgedb_heap_metadata_directory_legacy.db";
        std::filesystem::remove(legacyPath, error);
        PageId legacyMetadataId{};
        {
            DiskManager disk(legacyPath.string());
            BufferPoolManager pool(16, disk);
            Page* metadata = pool.newPage(legacyMetadataId);
            require(metadata != nullptr, "failed to allocate legacy metadata page");
            PageId dataPageId{};
            Page* dataPage = pool.newPage(dataPageId);
            require(dataPage != nullptr, "failed to allocate legacy data page");
            HeapPage heapPage(*dataPage);
            (void)heapPage.insert(record);
            require(pool.unpinPage(dataPageId, true), "failed to unpin legacy data page");

            std::vector<std::uint8_t> legacyBytes;
            Serializer::writeUInt64(legacyBytes, 0x464F524745484541ULL);
            Serializer::writeUInt32(legacyBytes, 1);
            Serializer::writeUInt64(legacyBytes, dataPageId.value());
            std::fill(metadata->data().begin(), metadata->data().end(), Page::Byte{0});
            std::copy(legacyBytes.begin(), legacyBytes.end(), metadata->data().begin());
            require(pool.unpinPage(legacyMetadataId, true), "failed to persist legacy metadata");
        }
        {
            DiskManager disk(legacyPath.string());
            BufferPoolManager pool(16, disk);
            HeapFile heap(disk, pool, legacyMetadataId);
            require(heap.scan().size() == 1, "legacy V1 metadata could not be read");
            (void)heap.insert(record); // forces a new page and V1 -> V2 migration
            require(heap.scan().size() == 2, "legacy metadata migration lost a record");
        }
        {
            DiskManager disk(legacyPath.string());
            BufferPoolManager pool(16, disk);
            HeapFile heap(disk, pool, legacyMetadataId);
            const auto ids = heap.scan();
            require(ids.size() == 2, "migrated legacy metadata did not survive reopen");
            require(heap.read(ids.front()) == record && heap.read(ids.back()) == record,
                "legacy records changed after metadata migration and reopen");
        }
        std::filesystem::remove(legacyPath, error);

        std::cout << "Heap metadata-directory overflow regression passed\n";
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << "Heap metadata-directory overflow regression failed: "
                  << exception.what() << '\n';
        std::filesystem::remove(path, error);
        return 1;
    }
}
