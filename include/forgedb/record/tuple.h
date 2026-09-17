#pragma once

#include <cstddef>
#include <vector>

#include "forgedb/record/record_id.h"
#include "forgedb/record/value.h"
#include "forgedb/schema/schema.h"

namespace forgedb {

class Tuple {
public:
    Tuple(
        std::vector<Value> values,
        RecordId recordId = {}
    );

    [[nodiscard]] std::size_t size() const noexcept;

    [[nodiscard]] const Value& getValue(
        std::size_t index
    ) const;

    [[nodiscard]] Value& getValue(
        std::size_t index
    );

    [[nodiscard]] const std::vector<Value>& values() const noexcept;

    [[nodiscard]] RecordId recordId() const noexcept;

    void setRecordId(RecordId recordId) noexcept;

    [[nodiscard]] bool matchesSchema(
        const Schema& schema
    ) const noexcept;

private:
    std::vector<Value> values_;
    RecordId recordId_;
};

} // namespace forgedb