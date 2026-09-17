#include "forgedb/index/persistent_b_plus_tree.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

#include "forgedb/buffer/buffer_pool_manager.h"
#include "forgedb/common/serializer.h"
#include "forgedb/index/b_plus_tree_internal_page.h"
#include "forgedb/index/b_plus_tree_leaf_page.h"
#include "forgedb/index/b_plus_tree_metadata.h"
#include "forgedb/index/index_key.h"
#include "forgedb/storage/page.h"

namespace forgedb {

namespace {

std::span<const std::uint8_t> pageData(const Page& page) {
    return std::span<const std::uint8_t>(
        page.data().data(),
        page.data().size()
    );
}

std::span<std::uint8_t> pageData(Page& page) {
    return std::span<std::uint8_t>(
        page.data().data(),
        page.data().size()
    );
}

} // namespace

PersistentBPlusTree::PersistentBPlusTree(
    BufferPoolManager& bufferPoolManager,
    std::optional<PageId> metadataPageId
)
    : bufferPoolManager_(bufferPoolManager) {

    if (metadataPageId.has_value()) {
        metadataPageId_ = *metadataPageId;
        loadMetadata();
        return;
    }

    PageId newMetadataPage = bufferPoolManager_.newPage();

    metadataPageId_ = newMetadataPage;

    initializeMetadataPage();

    persistMetadata();
}

void PersistentBPlusTree::initializeMetadataPage() {
    Page* page = bufferPoolManager_.fetchPage(metadataPageId_);

    if (page == nullptr) {
        throw std::runtime_error(
            "PersistentBPlusTree: failed to fetch metadata page"
        );
    }

    auto data = pageData(*page);

    std::fill(data.begin(), data.end(), std::uint8_t{0});

    std::size_t offset = kMetadataMagicOffset;

    Serializer::writeUInt64(
        data,
        offset,
        kBPlusTreeMetadataMagic
    );

    Serializer::writeUInt32(
        data,
        offset,
        kBPlusTreeMetadataVersion
    );

    Serializer::writeUInt64(
        data,
        offset,
        0
    );

    Serializer::writeUInt64(
        data,
        offset,
        0
    );

    if (!bufferPoolManager_.unpinPage(metadataPageId_, true)) {
        throw std::runtime_error(
            "PersistentBPlusTree: failed to unpin metadata page"
        );
    }
}

void PersistentBPlusTree::loadMetadata() {
    Page* page = bufferPoolManager_.fetchPage(metadataPageId_);

    if (page == nullptr) {
        throw std::runtime_error(
            "PersistentBPlusTree: failed to fetch metadata page"
        );
    }

    auto data = pageData(*page);

    std::size_t offset = kMetadataMagicOffset;

    const std::uint64_t magic =
        Serializer::readUInt64(data, offset);

    const std::uint32_t version =
        Serializer::readUInt32(data, offset);

    if (magic != kBPlusTreeMetadataMagic) {
        bufferPoolManager_.unpinPage(metadataPageId_, false);

        throw std::runtime_error(
            "PersistentBPlusTree: invalid metadata magic"
        );
    }

    if (version != kBPlusTreeMetadataVersion) {
        bufferPoolManager_.unpinPage(metadataPageId_, false);

        throw std::runtime_error(
            "PersistentBPlusTree: unsupported metadata version"
        );
    }

    const std::uint64_t root =
        Serializer::readUInt64(data, offset);

    size_ =
        static_cast<std::size_t>(
            Serializer::readUInt64(data, offset)
        );

    if (root == 0) {
        rootPageId_.reset();
    } else {
        rootPageId_ = PageId{root};
    }

    if (!bufferPoolManager_.unpinPage(metadataPageId_, false)) {
        throw std::runtime_error(
            "PersistentBPlusTree: failed to unpin metadata page"
        );
    }
}

void PersistentBPlusTree::persistMetadata() {
    Page* page = bufferPoolManager_.fetchPage(metadataPageId_);

    if (page == nullptr) {
        throw std::runtime_error(
            "PersistentBPlusTree: failed to fetch metadata page"
        );
    }

    auto data = pageData(*page);

    std::size_t offset = kMetadataMagicOffset;

    Serializer::writeUInt64(
        data,
        offset,
        kBPlusTreeMetadataMagic
    );

    Serializer::writeUInt32(
        data,
        offset,
        kBPlusTreeMetadataVersion
    );

    Serializer::writeUInt64(
        data,
        offset,
        rootPageId_.has_value()
            ? rootPageId_->value()
            : 0
    );

    Serializer::writeUInt64(
        data,
        offset,
        static_cast<std::uint64_t>(size_)
    );

    if (!bufferPoolManager_.unpinPage(metadataPageId_, true)) {
        throw std::runtime_error(
            "PersistentBPlusTree: failed to unpin metadata page"
        );
    }
}

PageId PersistentBPlusTree::createLeafRoot() {
    PageId pageId = bufferPoolManager_.newPage();

    Page* page = bufferPoolManager_.fetchPage(pageId);

    if (page == nullptr) {
        throw std::runtime_error(
            "PersistentBPlusTree: failed to fetch new root"
        );
    }

    BPlusTreeLeafPage leaf(*page);

    leaf.setPageType(IndexPageType::Leaf);
    leaf.setParentPageId(PageId{});
    leaf.setNextPageId(PageId{});
    leaf.setEntryCount(0);

    if (!bufferPoolManager_.unpinPage(pageId, true)) {
        throw std::runtime_error(
            "PersistentBPlusTree: failed to unpin new root"
        );
    }

    return pageId;
}

PageId PersistentBPlusTree::findLeafPage(
    const IndexKey& key
) const {
    if (!rootPageId_.has_value()) {
        throw std::runtime_error(
            "PersistentBPlusTree: tree has no root"
        );
    }

    PageId currentPageId = *rootPageId_;

    while (true) {
        Page* page =
            bufferPoolManager_.fetchPage(currentPageId);

        if (page == nullptr) {
            throw std::runtime_error(
                "PersistentBPlusTree: failed to fetch tree page"
            );
        }

        IndexPage indexPage(*page);

        const IndexPageType type =
            indexPage.pageType();

        if (type == IndexPageType::Leaf) {
            bufferPoolManager_.unpinPage(
                currentPageId,
                false
            );

            return currentPageId;
        }

        if (type != IndexPageType::Internal) {
            bufferPoolManager_.unpinPage(
                currentPageId,
                false
            );

            throw std::runtime_error(
                "PersistentBPlusTree: invalid index page type"
            );
        }

        BPlusTreeInternalPage internal(*page);

        const PageId child =
            internal.lookupChild(key);

        if (!bufferPoolManager_.unpinPage(
                currentPageId,
                false
            )) {
            throw std::runtime_error(
                "PersistentBPlusTree: failed to unpin internal page"
            );
        }

        currentPageId = child;
    }
}

bool PersistentBPlusTree::insert(
    const IndexKey& key,
    RecordId recordId
) {
    if (!rootPageId_.has_value()) {
        rootPageId_ = createLeafRoot();
    }

    const bool inserted =
        insertIntoLeaf(key, recordId);

    if (!inserted) {
        return false;
    }

    ++size_;

    persistMetadata();

    return true;
}

bool PersistentBPlusTree::insertIntoLeaf(
    const IndexKey& key,
    RecordId recordId
) {
    const PageId leafPageId =
        findLeafPage(key);

    Page* page =
        bufferPoolManager_.fetchPage(leafPageId);

    if (page == nullptr) {
        throw std::runtime_error(
            "PersistentBPlusTree: failed to fetch leaf"
        );
    }

    BPlusTreeLeafPage leaf(*page);

    const auto existing =
        leaf.lookup(key);

    for (const RecordId& existingId : existing) {
        if (existingId == recordId) {
            bufferPoolManager_.unpinPage(
                leafPageId,
                false
            );

            return false;
        }
    }

    const bool inserted =
        leaf.insert(key, recordId);

    if (!inserted) {
        if (!bufferPoolManager_.unpinPage(
                leafPageId,
                false
            )) {
            throw std::runtime_error(
                "PersistentBPlusTree: failed to unpin leaf"
            );
        }

        return false;
    }

    const bool needsSplit =
        leaf.freeSpace() == 0;

    if (!needsSplit) {
        if (!bufferPoolManager_.unpinPage(
                leafPageId,
                true
            )) {
            throw std::runtime_error(
                "PersistentBPlusTree: failed to unpin leaf"
            );
        }

        return true;
    }

    if (!bufferPoolManager_.unpinPage(
            leafPageId,
            true
        )) {
        throw std::runtime_error(
            "PersistentBPlusTree: failed to unpin leaf"
        );
    }

    const SplitResult split =
        splitLeaf(leafPageId);

    insertIntoParent(
        leafPageId,
        split.separator,
        split.rightPageId
    );

    return true;
}

PersistentBPlusTree::SplitResult
PersistentBPlusTree::splitLeaf(
    PageId leafPageId
) {
    Page* page =
        bufferPoolManager_.fetchPage(leafPageId);

    if (page == nullptr) {
        throw std::runtime_error(
            "PersistentBPlusTree: failed to fetch leaf for split"
        );
    }

    BPlusTreeLeafPage leaf(*page);

    const std::size_t count =
        leaf.size();

    if (count < 2) {
        bufferPoolManager_.unpinPage(
            leafPageId,
            false
        );

        throw std::runtime_error(
            "PersistentBPlusTree: cannot split leaf with fewer than two entries"
        );
    }

    std::vector<BPlusTreeLeafPage::Entry> entries;
    entries.reserve(count);

    for (std::size_t i = 0; i < count; ++i) {
        entries.push_back({
            leaf.keyAt(i),
            leaf.recordIdAt(i)
        });
    }

    const PageId oldNext =
        leaf.nextPageId();

    const PageId parent =
        leaf.parentPageId();

    const std::size_t middle =
        count / 2;

    std::vector<BPlusTreeLeafPage::Entry> leftEntries(
        entries.begin(),
        entries.begin() + static_cast<std::ptrdiff_t>(middle)
    );

    std::vector<BPlusTreeLeafPage::Entry> rightEntries(
        entries.begin() + static_cast<std::ptrdiff_t>(middle),
        entries.end()
    );

    const PageId rightPageId =
        bufferPoolManager_.newPage();

    Page* rightPage =
        bufferPoolManager_.fetchPage(rightPageId);

    if (rightPage == nullptr) {
        bufferPoolManager_.unpinPage(
            leafPageId,
            false
        );

        throw std::runtime_error(
            "PersistentBPlusTree: failed to fetch right leaf"
        );
    }

    BPlusTreeLeafPage rightLeaf(*rightPage);

    rightLeaf.setPageType(IndexPageType::Leaf);
    rightLeaf.setParentPageId(parent);
    rightLeaf.setNextPageId(oldNext);

    for (const auto& entry : rightEntries) {
        if (!rightLeaf.insert(
                entry.key,
                entry.recordId
            )) {
            bufferPoolManager_.unpinPage(
                rightPageId,
                false
            );

            bufferPoolManager_.unpinPage(
                leafPageId,
                false
            );

            throw std::runtime_error(
                "PersistentBPlusTree: right leaf overflow during split"
            );
        }
    }

    if (!leaf.remove(
            leaf.keyAt(0),
            leaf.recordIdAt(0)
        )) {
        // This should never be reached because we rebuild
        // the left side below.
    }

    // Rebuild the original leaf from scratch.
    //
    // The page abstraction currently exposes removal of individual
    // entries rather than bulk replacement, so remove everything.
    for (std::size_t i = leaf.size(); i > 0; --i) {
        const std::size_t index = i - 1;

        leaf.remove(
            leaf.keyAt(index),
            leaf.recordIdAt(index)
        );
    }

    for (const auto& entry : leftEntries) {
        if (!leaf.insert(
                entry.key,
                entry.recordId
            )) {
            bufferPoolManager_.unpinPage(
                rightPageId,
                true
            );

            bufferPoolManager_.unpinPage(
                leafPageId,
                false
            );

            throw std::runtime_error(
                "PersistentBPlusTree: left leaf overflow during split"
            );
        }
    }

    leaf.setNextPageId(rightPageId);

    if (!bufferPoolManager_.unpinPage(
            rightPageId,
            true
        )) {
        throw std::runtime_error(
            "PersistentBPlusTree: failed to unpin right leaf"
        );
    }

    if (!bufferPoolManager_.unpinPage(
            leafPageId,
            true
        )) {
        throw std::runtime_error(
            "PersistentBPlusTree: failed to unpin left leaf"
        );
    }

    return SplitResult{
        rightPageId,
        rightEntries.front().key
    };
}

void PersistentBPlusTree::insertIntoParent(
    PageId leftPageId,
    const IndexKey& separator,
    PageId rightPageId
) {
    Page* leftPage =
        bufferPoolManager_.fetchPage(leftPageId);

    if (leftPage == nullptr) {
        throw std::runtime_error(
            "PersistentBPlusTree: failed to fetch left child"
        );
    }

    IndexPage leftIndex(*leftPage);

    const PageId parentPageId =
        leftIndex.parentPageId();

    if (!bufferPoolManager_.unpinPage(
            leftPageId,
            false
        )) {
        throw std::runtime_error(
            "PersistentBPlusTree: failed to unpin left child"
        );
    }

    if (parentPageId.value() == 0) {
        createNewRoot(
            leftPageId,
            separator,
            rightPageId
        );

        return;
    }

    Page* parentPage =
        bufferPoolManager_.fetchPage(parentPageId);

    if (parentPage == nullptr) {
        throw std::runtime_error(
            "PersistentBPlusTree: failed to fetch parent"
        );
    }

    BPlusTreeInternalPage parent(*parentPage);

    std::size_t childIndex = 0;

    while (
        childIndex < parent.childCount() &&
        parent.childAt(childIndex) != leftPageId
    ) {
        ++childIndex;
    }

    if (childIndex == parent.childCount()) {
        bufferPoolManager_.unpinPage(
            parentPageId,
            false
        );

        throw std::runtime_error(
            "PersistentBPlusTree: left child not found in parent"
        );
    }

    parent.insertChild(
        childIndex + 1,
        separator,
        rightPageId
    );

    const bool needsSplit =
        parent.freeSpace() == 0;

    if (!needsSplit) {
        if (!bufferPoolManager_.unpinPage(
                parentPageId,
                true
            )) {
            throw std::runtime_error(
                "PersistentBPlusTree: failed to unpin parent"
            );
        }

        // Update the right child's parent pointer.
        Page* rightPage =
            bufferPoolManager_.fetchPage(rightPageId);

        if (rightPage == nullptr) {
            throw std::runtime_error(
                "PersistentBPlusTree: failed to fetch right child"
            );
        }

        IndexPage rightIndex(*rightPage);
        rightIndex.setParentPageId(parentPageId);

        if (!bufferPoolManager_.unpinPage(
                rightPageId,
                true
            )) {
            throw std::runtime_error(
                "PersistentBPlusTree: failed to unpin right child"
            );
        }

        return;
    }

    if (!bufferPoolManager_.unpinPage(
            parentPageId,
            true
        )) {
        throw std::runtime_error(
            "PersistentBPlusTree: failed to unpin parent"
        );
    }

    const SplitResult split =
        splitInternal(parentPageId);

    insertIntoParent(
        parentPageId,
        split.separator,
        split.rightPageId
    );
}

PersistentBPlusTree::SplitResult
PersistentBPlusTree::splitInternal(
    PageId internalPageId
) {
    Page* page =
        bufferPoolManager_.fetchPage(internalPageId);

    if (page == nullptr) {
        throw std::runtime_error(
            "PersistentBPlusTree: failed to fetch internal page"
        );
    }

    BPlusTreeInternalPage internal(*page);

    const std::size_t keyCount =
        internal.size();

    if (keyCount < 2) {
        bufferPoolManager_.unpinPage(
            internalPageId,
            false
        );

        throw std::runtime_error(
            "PersistentBPlusTree: cannot split internal page"
        );
    }

    std::vector<IndexKey> keys;
    std::vector<PageId> children;

    keys.reserve(keyCount);
    children.reserve(internal.childCount());

    for (std::size_t i = 0; i < keyCount; ++i) {
        keys.push_back(internal.keyAt(i));
    }

    for (std::size_t i = 0; i < internal.childCount(); ++i) {
        children.push_back(internal.childAt(i));
    }

    const std::size_t middle =
        keyCount / 2;

    const IndexKey separator =
        keys[middle];

    std::vector<IndexKey> leftKeys(
        keys.begin(),
        keys.begin() + static_cast<std::ptrdiff_t>(middle)
    );

    std::vector<PageId> leftChildren(
        children.begin(),
        children.begin() +
            static_cast<std::ptrdiff_t>(middle + 1)
    );

    std::vector<IndexKey> rightKeys(
        keys.begin() + static_cast<std::ptrdiff_t>(middle + 1),
        keys.end()
    );

    std::vector<PageId> rightChildren(
        children.begin() +
            static_cast<std::ptrdiff_t>(middle + 1),
        children.end()
    );

    const PageId parent =
        internal.parentPageId();

    const PageId rightPageId =
        bufferPoolManager_.newPage();

    Page* rightPage =
        bufferPoolManager_.fetchPage(rightPageId);

    if (rightPage == nullptr) {
        bufferPoolManager_.unpinPage(
            internalPageId,
            false
        );

        throw std::runtime_error(
            "PersistentBPlusTree: failed to fetch right internal page"
        );
    }

    BPlusTreeInternalPage rightInternal(*rightPage);

    rightInternal.setPageType(
        IndexPageType::Internal
    );

    rightInternal.setParentPageId(parent);
    rightInternal.setFirstChild(rightChildren.front());

    for (std::size_t i = 0; i < rightKeys.size(); ++i) {
        rightInternal.insertChild(
            i + 1,
            rightKeys[i],
            rightChildren[i + 1]
        );
    }

    // Rebuild left side.
    for (std::size_t i = internal.childCount(); i > 0; --i) {
        internal.removeChild(i - 1);
    }

    internal.setFirstChild(leftChildren.front());

    for (std::size_t i = 0; i < leftKeys.size(); ++i) {
        internal.insertChild(
            i + 1,
            leftKeys[i],
            leftChildren[i + 1]
        );
    }

    if (!bufferPoolManager_.unpinPage(
            rightPageId,
            true
        )) {
        throw std::runtime_error(
            "PersistentBPlusTree: failed to unpin right internal page"
        );
    }

    if (!bufferPoolManager_.unpinPage(
            internalPageId,
            true
        )) {
        throw std::runtime_error(
            "PersistentBPlusTree: failed to unpin left internal page"
        );
    }

    // Children moved to the new right internal node must now point
    // to the new parent.
    for (const PageId childPageId : rightChildren) {
        Page* childPage =
            bufferPoolManager_.fetchPage(childPageId);

        if (childPage == nullptr) {
            throw std::runtime_error(
                "PersistentBPlusTree: failed to fetch child after split"
            );
        }

        IndexPage child(*childPage);
        child.setParentPageId(rightPageId);

        if (!bufferPoolManager_.unpinPage(
                childPageId,
                true
            )) {
            throw std::runtime_error(
                "PersistentBPlusTree: failed to unpin child after split"
            );
        }
    }

    return SplitResult{
        rightPageId,
        separator
    };
}

void PersistentBPlusTree::createNewRoot(
    PageId leftPageId,
    const IndexKey& separator,
    PageId rightPageId
) {
    const PageId newRootPageId =
        bufferPoolManager_.newPage();

    Page* rootPage =
        bufferPoolManager_.fetchPage(newRootPageId);

    if (rootPage == nullptr) {
        throw std::runtime_error(
            "PersistentBPlusTree: failed to fetch new root"
        );
    }

    BPlusTreeInternalPage root(*rootPage);

    root.setPageType(
        IndexPageType::Internal
    );

    root.setParentPageId(PageId{});

    root.setFirstChild(leftPageId);

    root.insertChild(
        1,
        separator,
        rightPageId
    );

    if (!bufferPoolManager_.unpinPage(
            newRootPageId,
            true
        )) {
        throw std::runtime_error(
            "PersistentBPlusTree: failed to unpin new root"
        );
    }

    Page* leftPage =
        bufferPoolManager_.fetchPage(leftPageId);

    if (leftPage == nullptr) {
        throw std::runtime_error(
            "PersistentBPlusTree: failed to fetch left child"
        );
    }

    IndexPage left(*leftPage);
    left.setParentPageId(newRootPageId);

    if (!bufferPoolManager_.unpinPage(
            leftPageId,
            true
        )) {
        throw std::runtime_error(
            "PersistentBPlusTree: failed to unpin left child"
        );
    }

    Page* rightPage =
        bufferPoolManager_.fetchPage(rightPageId);

    if (rightPage == nullptr) {
        throw std::runtime_error(
            "PersistentBPlusTree: failed to fetch right child"
        );
    }

    IndexPage right(*rightPage);
    right.setParentPageId(newRootPageId);

    if (!bufferPoolManager_.unpinPage(
            rightPageId,
            true
        )) {
        throw std::runtime_error(
            "PersistentBPlusTree: failed to unpin right child"
        );
    }

    rootPageId_ = newRootPageId;

    persistMetadata();
}

std::vector<RecordId> PersistentBPlusTree::lookup(
    const IndexKey& key
) const {
    if (!rootPageId_.has_value()) {
        return {};
    }

    const PageId leafPageId =
        findLeafPage(key);

    Page* page =
        bufferPoolManager_.fetchPage(leafPageId);

    if (page == nullptr) {
        throw std::runtime_error(
            "PersistentBPlusTree: failed to fetch leaf"
        );
    }

    BPlusTreeLeafPage leaf(*page);

    std::vector<RecordId> result =
        leaf.lookup(key);

    if (!bufferPoolManager_.unpinPage(
            leafPageId,
            false
        )) {
        throw std::runtime_error(
            "PersistentBPlusTree: failed to unpin leaf"
        );
    }

    // Duplicate keys may cross a leaf boundary.
    PageId nextPageId =
        leaf.nextPageId();

    while (nextPageId.value() != 0) {
        Page* nextPage =
            bufferPoolManager_.fetchPage(nextPageId);

        if (nextPage == nullptr) {
            throw std::runtime_error(
                "PersistentBPlusTree: failed to fetch next leaf"
            );
        }

        BPlusTreeLeafPage nextLeaf(*nextPage);

        const auto matches =
            nextLeaf.lookup(key);

        if (matches.empty()) {
            const bool pastKey =
                nextLeaf.size() > 0 &&
                key < nextLeaf.keyAt(0);

            if (pastKey) {
                bufferPoolManager_.unpinPage(
                    nextPageId,
                    false
                );

                break;
            }
        }

        result.insert(
            result.end(),
            matches.begin(),
            matches.end()
        );

        const PageId next =
            nextLeaf.nextPageId();

        if (!bufferPoolManager_.unpinPage(
                nextPageId,
                false
            )) {
            throw std::runtime_error(
                "PersistentBPlusTree: failed to unpin next leaf"
            );
        }

        nextPageId = next;
    }

    return result;
}

std::vector<RecordId> PersistentBPlusTree::scan(
    const IndexKey& lower,
    const IndexKey& upper
) const {
    if (upper < lower) {
        throw std::invalid_argument(
            "PersistentBPlusTree: invalid scan range"
        );
    }

    if (!rootPageId_.has_value()) {
        return {};
    }

    const PageId leafPageId =
        findLeafPage(lower);

    PageId currentPageId =
        leafPageId;

    std::vector<RecordId> result;

    while (currentPageId.value() != 0) {
        Page* page =
            bufferPoolManager_.fetchPage(currentPageId);

        if (page == nullptr) {
            throw std::runtime_error(
                "PersistentBPlusTree: failed to fetch leaf"
            );
        }

        BPlusTreeLeafPage leaf(*page);

        const std::size_t start =
            leaf.lowerBound(lower);

        for (std::size_t i = start; i < leaf.size(); ++i) {
            const IndexKey currentKey =
                leaf.keyAt(i);

            if (upper < currentKey) {
                if (!bufferPoolManager_.unpinPage(
                        currentPageId,
                        false
                    )) {
                    throw std::runtime_error(
                        "PersistentBPlusTree: failed to unpin leaf"
                    );
                }

                return result;
            }

            result.push_back(
                leaf.recordIdAt(i)
            );
        }

        const PageId next =
            leaf.nextPageId();

        if (!bufferPoolManager_.unpinPage(
                currentPageId,
                false
            )) {
            throw std::runtime_error(
                "PersistentBPlusTree: failed to unpin leaf"
            );
        }

        currentPageId = next;
    }

    return result;
}

bool PersistentBPlusTree::contains(
    const IndexKey& key
) const {
    return !lookup(key).empty();
}

bool PersistentBPlusTree::remove(
    const IndexKey&,
    RecordId
) {
    throw std::logic_error(
        "PersistentBPlusTree::remove is not implemented yet"
    );
}

std::size_t PersistentBPlusTree::size() const noexcept {
    return size_;
}

PageId PersistentBPlusTree::rootPageId() const noexcept {
    return rootPageId_.value_or(PageId{});
}

PageId PersistentBPlusTree::metadataPageId() const noexcept {
    return metadataPageId_;
}

std::size_t PersistentBPlusTree::minimumLeafEntries(
    std::size_t entryCount
) noexcept {
    return (entryCount + 1) / 2;
}

std::size_t PersistentBPlusTree::minimumInternalChildren(
    std::size_t childCount
) noexcept {
    return (childCount + 1) / 2;
}

} // namespace forgedb