#pragma once

#include <cstddef>
#include <vector>

#include "forgedb/index/index_key.h"
#include "forgedb/record/record_id.h"

namespace forgedb {

class Index {
public:
    virtual ~Index() = default;

    Index(const Index&) = delete;
    Index& operator=(const Index&) = delete;

    Index(Index&&) = delete;
    Index& operator=(Index&&) = delete;

    [[nodiscard]] virtual bool insert(
        const IndexKey& key,
        RecordId recordId
    ) = 0;

    [[nodiscard]] virtual bool remove(
        const IndexKey& key,
        RecordId recordId
    ) = 0;

    [[nodiscard]] virtual std::vector<RecordId> lookup(
        const IndexKey& key
    ) const = 0;

    [[nodiscard]] virtual std::vector<RecordId> scan(
        const IndexKey& lowerBound,
        const IndexKey& upperBound
    ) const = 0;

    [[nodiscard]] virtual bool contains(
        const IndexKey& key
    ) const = 0;

    [[nodiscard]] virtual std::size_t size() const noexcept = 0;

protected:
    Index() = default;
};

} // namespace forgedb