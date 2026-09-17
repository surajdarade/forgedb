#pragma once

#include <cstddef>
#include <optional>
#include <vector>

#include "forgedb/index/index.h"
#include "forgedb/common/page_id.h"
#include "forgedb/index/b_plus_tree_leaf_page.h"

namespace forgedb {

class BufferPoolManager;
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

    SplitResult splitInternalWithInsertion(
        PageId internalPageId,
        const IndexKey& separator,
        PageId rightChild,
        PageId leftChild
    );

    void rebalanceLeaf(PageId leafPageId);
    void rebalanceInternal(PageId internalPageId);

    [[nodiscard]] std::vector<BPlusTreeLeafPage::Entry> leafEntries(PageId pageId) const;
    [[nodiscard]] std::vector<PageId> childrenFor(PageId pageId) const;
    [[nodiscard]] std::vector<IndexKey> keysFor(PageId pageId) const;
    [[nodiscard]] std::vector<IndexKey> keysForChildren(const std::vector<PageId>& children) const;
    void rewriteInternalFromChildren(PageId pageId, const std::vector<PageId>& children);
    void setParent(PageId childPageId, PageId parentPageId);
    [[nodiscard]] IndexKey firstKey(PageId pageId) const;
    void repairSeparators(PageId pageId);
    void collapseEmptyRoot();

    void createNewRoot(
        PageId leftPageId,
        const IndexKey& separator,
        PageId rightPageId
    );

};

} // namespace forgedb