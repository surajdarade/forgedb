#include <cstdint>
#include "forgedb/buffer/buffer_pool_manager.h"
#include "forgedb/index/persistent_b_plus_tree.h"
#include "forgedb/storage/disk_manager.h"

int main() {
    forgedb::DiskManager disk("forgedb_index.db");
    forgedb::BufferPoolManager bufferPool(32, disk);
    forgedb::PersistentBPlusTree index(bufferPool);

    index.insert(
        forgedb::IndexKey{std::int32_t{42}},
        forgedb::RecordId{forgedb::PageId{100}, 0}
    );

    const auto result = index.lookup(
        forgedb::IndexKey{std::int32_t{42}}
    );

    (void)result;
    bufferPool.flushAllPages();
}
