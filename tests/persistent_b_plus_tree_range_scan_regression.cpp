#include <cstdint>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

#include "forgedb/buffer/buffer_pool_manager.h"
#include "forgedb/index/index_key.h"
#include "forgedb/index/persistent_b_plus_tree.h"
#include "forgedb/record/record_id.h"
#include "forgedb/storage/disk_manager.h"

using namespace forgedb;

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

}

int main() {
    const auto path = std::filesystem::temp_directory_path() /
        "forgedb_persistent_b_plus_tree_range_scan_regression.db";
    std::error_code error;
    std::filesystem::remove(path, error);

    PageId integerMetadata{};
    PageId stringMetadata{};
    constexpr std::int32_t kIntegerKeys = 5000;
    constexpr std::int32_t kDuplicateCount = 150;

    try {
        {
            DiskManager disk(path.string());
            BufferPoolManager pool(32, disk);
            PersistentBPlusTree tree(pool);
            integerMetadata = tree.metadataPageId();

            for (std::int32_t key = 0; key < kIntegerKeys; ++key) {
                require(tree.insert(IndexKey{key}, RecordId{PageId{static_cast<std::uint64_t>(key) + 1}, 0}),
                    "failed to insert integer key " + std::to_string(key));
            }
            for (std::int32_t i = 0; i < kDuplicateCount; ++i) {
                require(tree.insert(IndexKey{2500}, RecordId{PageId{static_cast<std::uint64_t>(100000 + i)}, 7}),
                    "failed to insert duplicate-key record");
            }

            const auto aroundSplit = tree.scan(IndexKey{2499}, IndexKey{2501});
            require(aroundSplit.size() == static_cast<std::size_t>(kDuplicateCount + 3),
                "inclusive range scan missed keys or duplicate records across leaf pages");

            const auto singleKey = tree.scan(IndexKey{1234}, IndexKey{1234});
            require(singleKey.size() == 1, "equal-bound scan must include its key");

            require(tree.scan(IndexKey{-100}, IndexKey{-1}).empty(),
                "range below minimum should be empty");
            require(tree.scan(IndexKey{6000}, IndexKey{7000}).empty(),
                "range above maximum should be empty");
            bool rejectedReversedRange = false;
            try {
                (void)tree.scan(IndexKey{300}, IndexKey{299});
            } catch (const std::invalid_argument&) {
                rejectedReversedRange = true;
            }
            require(rejectedReversedRange, "reversed range must be rejected");
        }

        {
            DiskManager disk(path.string());
            BufferPoolManager pool(32, disk);
            PersistentBPlusTree tree(pool, integerMetadata);
            const auto aroundSplit = tree.scan(IndexKey{2499}, IndexKey{2501});
            require(aroundSplit.size() == static_cast<std::size_t>(kDuplicateCount + 3),
                "inclusive range scan count changed after reopen");
            const auto all = tree.scan(IndexKey{0}, IndexKey{kIntegerKeys - 1});
            require(all.size() == static_cast<std::size_t>(kIntegerKeys + kDuplicateCount),
                "full integer range count mismatch after reopen");

            PersistentBPlusTree strings(pool);
            stringMetadata = strings.metadataPageId();
            for (std::int32_t i = 0; i < 300; ++i) {
                const auto key = std::string("key-") + (i < 100 ? "0" : "") +
                    (i < 10 ? "0" : "") + std::to_string(i);
                require(strings.insert(IndexKey{key}, RecordId{PageId{static_cast<std::uint64_t>(200000 + i)}, 2}),
                    "failed to insert string key " + key);
            }
            const auto stringRange = strings.scan(IndexKey{std::string("key-100")}, IndexKey{std::string("key-125")});
            require(stringRange.size() == 26, "inclusive varchar range scan count mismatch");
        }

        {
            DiskManager disk(path.string());
            BufferPoolManager pool(32, disk);
            PersistentBPlusTree strings(pool, stringMetadata);
            const auto stringRange = strings.scan(IndexKey{std::string("key-100")}, IndexKey{std::string("key-125")});
            require(stringRange.size() == 26, "varchar range scan changed after reopen");
        }

        std::filesystem::remove(path, error);
        std::cout << "Persistent B+ tree range-scan regression passed\n";
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << "Persistent B+ tree range-scan regression failed: " << exception.what() << '\n';
        std::filesystem::remove(path, error);
        return 1;
    }
}
