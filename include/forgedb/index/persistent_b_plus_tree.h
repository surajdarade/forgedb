#pragma once

#include <cstddef>
#include <optional>
#include <vector>

#include "forgedb/index/index.h"
#include "forgedb/common/page_id.h"

namespace forgedb {

class BufferPoolManager;
class BPlusTreeLeafPage;
class BPlusTreeInternalPage;
class IndexKey;

class PersistentBPlusTree final : public Index {
public:
    explicit PersistentBPlusTree(
        BufferPoolManager& bufferPoolManager,
        std::optional<PageId> metadataPageId = std::nullopt
    );

    bool insert(
        const IndexKey& key,
        RecordId recordId
    ) override;

    bool remove(
        const IndexKey& key,
        RecordId recordId
    ) override;

    std::vector<RecordId> lookup(
        const IndexKey& key
    ) const override;

    std::vector<RecordId> scan(
        const IndexKey& lower,
        const IndexKey& upper
    ) const override;

    bool contains(
        const IndexKey& key
    ) const override;

    [[nodiscard]] std::size_t size() const noexcept override;

    [[nodiscard]] PageId rootPageId() const noexcept;

    [[nodiscard]] PageId metadataPageId() const noexcept;

private:
    struct SplitResult {
        PageId rightPageId;
        IndexKey separator;
    };

    BufferPoolManager& bufferPoolManager_;

    PageId metadataPageId_{};
    std::optional<PageId> rootPageId_;

    std::size_t size_{0};

private:
    void initializeMetadataPage();

    void loadMetadata();

    void persistMetadata();

    PageId createLeafRoot();

    PageId findLeafPage(
        const IndexKey& key
    ) const;

    bool insertIntoLeaf(
        const IndexKey& key,
        RecordId recordId
    );

    SplitResult splitLeaf(
        PageId leafPageId
    );

    void insertIntoParent(
        PageId leftPageId,
        const IndexKey& separator,
        PageId rightPageId
    );

    SplitResult splitInternal(
        PageId internalPageId
    );

    void createNewRoot(
        PageId leftPageId,
        const IndexKey& separator,
        PageId rightPageId
    );

    [[nodiscard]] static std::size_t minimumLeafEntries(
        std::size_t entryCount
    ) noexcept;

    [[nodiscard]] static std::size_t minimumInternalChildren(
        std::size_t childCount
    ) noexcept;
};

} // namespace forgedb