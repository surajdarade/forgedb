#include <cstdint>
#include <iostream>

#include "forgedb/buffer/buffer_pool_manager.h"
#include "forgedb/index/persistent_b_plus_tree.h"
#include "forgedb/storage/disk_manager.h"

int main() {
    try {
        forgedb::DiskManager disk("forgedb_index.db");
        forgedb::BufferPoolManager bufferPool(32, disk);
        forgedb::PersistentBPlusTree index(bufferPool);

        const forgedb::IndexKey key{std::int32_t{42}};
        const forgedb::RecordId recordId{
            forgedb::PageId{100},
            0
        };

        index.insert(key, recordId);

        std::cout << "Inserted key 42.\n";

        const auto result = index.lookup(key);

        std::cout << "Lookup returned "
                  << result.size()
                  << " record(s).\n";

        if (!result.empty()) {
            std::cout << "Record found successfully.\n";
        }

        bufferPool.flushAllPages();

        std::cout << "Persistent index example completed successfully.\n";

        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "ForgeDB error: "
                  << e.what()
                  << '\n';

        return 1;
    }
}