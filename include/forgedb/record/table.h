#pragma once

#include <span>
#include <cstdint>

#include "forgedb/record/record_id.h"
#include "forgedb/record/tuple.h"
#include "forgedb/schema/schema.h"
#include "forgedb/storage/heap_file.h"

namespace forgedb {

class Table {
public:
    Table(
        Schema schema,
        HeapFile& heapFile
    );

    [[nodiscard]] const Schema& schema() const noexcept;

    [[nodiscard]] RecordId insert(
        const Tuple& tuple
    );

    [[nodiscard]] Tuple get(
        RecordId recordId
    );

    void update(
        RecordId recordId,
        const Tuple& tuple
    );

    void remove(
        RecordId recordId
    );

private:
    Schema schema_;
    HeapFile& heapFile_;
};

} // namespace forgedb