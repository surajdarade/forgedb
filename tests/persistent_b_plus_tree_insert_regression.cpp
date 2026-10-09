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

int main() {
    const auto path = std::filesystem::temp_directory_path() /
        "forgedb_persistent_b_plus_tree_insert_regression.db";
    std::error_code error;
    std::filesystem::remove(path, error);

    constexpr std::int32_t kRows = 50000;
    constexpr std::size_t kPoolPages = 16;
    PageId metadataPageId{};
    PageId stringMetadataPageId{};

    try {
        {
            DiskManager disk(path.string());
            BufferPoolManager pool(kPoolPages, disk);
            PersistentBPlusTree tree(pool);
            metadataPageId = tree.metadataPageId();

            for (std::int32_t key = 0; key < kRows; ++key) {
                const RecordId id{PageId{static_cast<std::uint64_t>(key) + 1}, 0};
                if (!tree.insert(IndexKey{key}, id)) {
                    throw std::runtime_error("unexpected duplicate during ascending insert");
                }
            }

            const RecordId duplicateId{PageId{2001}, 0};
            if (tree.insert(IndexKey{2000}, duplicateId)) {
                throw std::runtime_error("exact duplicate was accepted");
            }

            // Exercise the non-append path after the rightmost-leaf cache is warm.
            if (!tree.insert(IndexKey{123}, RecordId{PageId{900001}, 1})) {
                throw std::runtime_error("out-of-order distinct RecordId was rejected");
            }
            // Insert out of order within the rightmost leaf, which invalidates
            // its cached serialized tail offset before the next maximum append.
            if (!tree.insert(IndexKey{49950}, RecordId{PageId{900004}, 5})) {
                throw std::runtime_error("out-of-order insert into rightmost leaf was rejected");
            }
            // Exercise out-of-order inserts on the left side, including leaf
            // splits, while preserving the cached rightmost insertion target.
            for (std::int32_t key = -1; key >= -100; --key) {
                if (!tree.insert(IndexKey{key}, RecordId{
                        PageId{static_cast<std::uint64_t>(900100 - key)}, 3})) {
                    throw std::runtime_error("out-of-order lower key was rejected");
                }
            }
            const RecordId oldMaximumId{PageId{900002}, 2};
            if (!tree.insert(IndexKey{50001}, oldMaximumId)) {
                throw std::runtime_error("new maximum key was rejected");
            }
            if (!tree.remove(IndexKey{50001}, oldMaximumId)) {
                throw std::runtime_error("maximum-key removal failed");
            }
            if (!tree.insert(IndexKey{50002}, RecordId{PageId{900003}, 2})) {
                throw std::runtime_error("insert after maximum-key removal failed");
            }

            if (tree.size() != static_cast<std::size_t>(kRows) + 103) {
                throw std::runtime_error("unexpected in-memory tree size");
            }

            // Variable-length keys exercise the serialized tail scanner's
            // varchar-length handling on the append fast path.
            PersistentBPlusTree stringTree(pool);
            stringMetadataPageId = stringTree.metadataPageId();
            for (std::int32_t i = 0; i < 100; ++i) {
                const auto key = std::string("key-") + std::to_string(100000 + i);
                if (!stringTree.insert(IndexKey{key}, RecordId{
                        PageId{static_cast<std::uint64_t>(910000 + i)}, 4})) {
                    throw std::runtime_error("varchar append insert was rejected");
                }
            }
        }

        {
            DiskManager disk(path.string());
            BufferPoolManager pool(kPoolPages, disk);
            PersistentBPlusTree tree(pool, metadataPageId);

            const std::size_t expectedSize = static_cast<std::size_t>(kRows) + 103;
            if (tree.size() != expectedSize) {
                throw std::runtime_error("persisted tree size mismatch after reopen");
            }

            for (std::int32_t key = 0; key < kRows; ++key) {
                if (tree.lookup(IndexKey{key}).empty()) {
                    throw std::runtime_error("missing key after reopen: " + std::to_string(key));
                }
            }

            if (tree.lookup(IndexKey{123}).size() != 2) {
                throw std::runtime_error("duplicate-key lookup did not return both RecordIds");
            }
            if (tree.lookup(IndexKey{49950}).size() != 2) {
                throw std::runtime_error("rightmost-leaf out-of-order insertion missing after reopen");
            }
            if (!tree.lookup(IndexKey{50001}).empty()) {
                throw std::runtime_error("removed maximum key reappeared after reopen");
            }
            if (!tree.lookup(IndexKey{50000}).empty()) {
                throw std::runtime_error("missing key between populated ranges was reported present");
            }
            if (!tree.lookup(IndexKey{-101}).empty()) {
                throw std::runtime_error("lookup below the minimum key was reported present");
            }
            if (!tree.lookup(IndexKey{50003}).empty()) {
                throw std::runtime_error("lookup above the maximum key was reported present");
            }
            if (tree.lookup(IndexKey{50002}).size() != 1) {
                throw std::runtime_error("new maximum after removal missing after reopen");
            }
            for (std::int32_t key = -1; key >= -100; --key) {
                if (tree.lookup(IndexKey{key}).size() != 1) {
                    throw std::runtime_error("out-of-order lower key missing after reopen");
                }
            }

            const auto all = tree.scan(IndexKey{-100}, IndexKey{50002});
            if (all.size() != expectedSize) {
                throw std::runtime_error("range scan count mismatch after reopen");
            }

            PersistentBPlusTree stringTree(pool, stringMetadataPageId);
            if (stringTree.size() != 100) {
                throw std::runtime_error("varchar tree size mismatch after reopen");
            }
            for (std::int32_t i = 0; i < 100; ++i) {
                const auto key = std::string("key-") + std::to_string(100000 + i);
                if (stringTree.lookup(IndexKey{key}).size() != 1) {
                    throw std::runtime_error("varchar key missing after reopen");
                }
            }
            if (!stringTree.lookup(IndexKey{std::string("key-100050x")}).empty()) {
                throw std::runtime_error("missing varchar key was reported present");
            }
        }

        std::filesystem::remove(path, error);
        std::cout << "Persistent B+ tree insertion regression passed (" << kRows
                  << " ascending keys, split/reopen, duplicate and out-of-order checks, "
                  << kPoolPages << " buffer frames).\n";
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << "Persistent B+ tree insertion regression failed: "
                  << exception.what() << '\n';
        std::filesystem::remove(path, error);
        return 1;
    }
}
