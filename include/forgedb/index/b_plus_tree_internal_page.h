#pragma once

#include <cstddef>
#include <cstdint>

#include "forgedb/index/index_page.h"
#include "forgedb/index/index_key.h"

namespace forgedb {

class BPlusTreeInternalPage final : public IndexPage {
public:
    explicit BPlusTreeInternalPage(Page& page);

    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] std::size_t childCount() const noexcept;

    [[nodiscard]] IndexKey keyAt(
        std::size_t index
    ) const;

    [[nodiscard]] PageId childAt(
        std::size_t index
    ) const;

    void setKey(
        std::size_t index,
        const IndexKey& key
    );

    void setChildAt(
        std::size_t index,
        PageId childPageId
    );

    void setFirstChild(
        PageId childPageId
    );

    [[nodiscard]] PageId lookupChild(
        const IndexKey& key
    ) const;

    void insertChild(
        std::size_t childIndex,
        const IndexKey& separator,
        PageId childPageId
    );

    void removeChild(
        std::size_t childIndex
    );

    [[nodiscard]] std::size_t freeSpace() const noexcept;

private:
    static constexpr std::size_t kPayloadOffset =
        IndexPage::kHeaderSize;

    [[nodiscard]] std::vector<IndexKey> readKeys() const;

    [[nodiscard]] std::vector<PageId> readChildren() const;

    void rewrite(
        const std::vector<IndexKey>& keys,
        const std::vector<PageId>& children
    );

    [[nodiscard]] static std::size_t serializedKeySize(
        const IndexKey& key
    );

    [[nodiscard]] static std::size_t serializedSize(
        const std::vector<IndexKey>& keys
    );
};

} // namespace forgedb