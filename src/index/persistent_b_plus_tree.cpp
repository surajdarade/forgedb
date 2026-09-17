#include "forgedb/index/persistent_b_plus_tree.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <utility>

#include "forgedb/buffer/buffer_pool_manager.h"
#include "forgedb/index/b_plus_tree_internal_page.h"
#include "forgedb/index/b_plus_tree_leaf_page.h"
#include "forgedb/index/b_plus_tree_metadata.h"
#include "forgedb/index/index_key.h"
#include "forgedb/storage/page.h"

namespace forgedb {

namespace {

void writeU32(Page& page, std::size_t offset, std::uint32_t value) {
    for (std::size_t i = 0; i < 4; ++i)
        page.data()[offset + i] = static_cast<std::uint8_t>(value >> (i * 8));
}

void writeU64(Page& page, std::size_t offset, std::uint64_t value) {
    for (std::size_t i = 0; i < 8; ++i)
        page.data()[offset + i] = static_cast<std::uint8_t>(value >> (i * 8));
}

std::uint32_t readU32(const Page& page, std::size_t offset) {
    std::uint32_t value = 0;
    for (std::size_t i = 0; i < 4; ++i)
        value |= static_cast<std::uint32_t>(page.data()[offset + i]) << (i * 8);
    return value;
}

std::uint64_t readU64(const Page& page, std::size_t offset) {
    std::uint64_t value = 0;
    for (std::size_t i = 0; i < 8; ++i)
        value |= static_cast<std::uint64_t>(page.data()[offset + i]) << (i * 8);
    return value;
}

bool isNullPage(PageId id) { return id.value() == 0; }

} // namespace

PersistentBPlusTree::PersistentBPlusTree(
    BufferPoolManager& bufferPoolManager,
    std::optional<PageId> metadataPageId)
    : bufferPoolManager_(bufferPoolManager) {
    if (metadataPageId) {
        metadataPageId_ = *metadataPageId;
        loadMetadata();
        return;
    }

    PageId pageId;
    Page* page = bufferPoolManager_.newPage(pageId);
    if (!page) throw std::runtime_error("PersistentBPlusTree: unable to allocate metadata page");
    metadataPageId_ = pageId;
    std::fill(page->data().begin(), page->data().end(), 0);
    writeU64(*page, kMetadataMagicOffset, kBPlusTreeMetadataMagic);
    writeU32(*page, kMetadataVersionOffset, kBPlusTreeMetadataVersion);
    writeU64(*page, kMetadataRootPageIdOffset, 0);
    writeU64(*page, kMetadataSizeOffset, 0);
    if (!bufferPoolManager_.unpinPage(pageId, true)) throw std::runtime_error("PersistentBPlusTree: metadata unpin failed");
}

void PersistentBPlusTree::loadMetadata() {
    Page* page = bufferPoolManager_.fetchPage(metadataPageId_);
    if (!page) throw std::runtime_error("PersistentBPlusTree: metadata page unavailable");
    const auto magic = readU64(*page, kMetadataMagicOffset);
    const auto version = readU32(*page, kMetadataVersionOffset);
    if (magic != kBPlusTreeMetadataMagic || version != kBPlusTreeMetadataVersion) {
        bufferPoolManager_.unpinPage(metadataPageId_, false);
        throw std::runtime_error("PersistentBPlusTree: invalid metadata page");
    }
    const auto root = readU64(*page, kMetadataRootPageIdOffset);
    const auto count = readU64(*page, kMetadataSizeOffset);
    rootPageId_ = root == 0 ? std::optional<PageId>{} : std::optional<PageId>{PageId{root}};
    size_ = static_cast<std::size_t>(count);
    if (!bufferPoolManager_.unpinPage(metadataPageId_, false)) throw std::runtime_error("PersistentBPlusTree: metadata unpin failed");
}

void PersistentBPlusTree::persistMetadata() {
    Page* page = bufferPoolManager_.fetchPage(metadataPageId_);
    if (!page) throw std::runtime_error("PersistentBPlusTree: metadata page unavailable");
    writeU64(*page, kMetadataMagicOffset, kBPlusTreeMetadataMagic);
    writeU32(*page, kMetadataVersionOffset, kBPlusTreeMetadataVersion);
    writeU64(*page, kMetadataRootPageIdOffset, rootPageId_ ? rootPageId_->value() : 0);
    writeU64(*page, kMetadataSizeOffset, static_cast<std::uint64_t>(size_));
    if (!bufferPoolManager_.unpinPage(metadataPageId_, true)) throw std::runtime_error("PersistentBPlusTree: metadata unpin failed");
}

PageId PersistentBPlusTree::createLeafRoot() {
    PageId id;
    Page* page = bufferPoolManager_.newPage(id);
    if (!page) throw std::runtime_error("PersistentBPlusTree: unable to allocate root");
    BPlusTreeLeafPage leaf(*page);
    leaf.setPageType(IndexPageType::Leaf);
    leaf.setParentPageId(PageId{});
    leaf.setNextPageId(PageId{});
    leaf.rewrite({});
    if (!bufferPoolManager_.unpinPage(id, true)) throw std::runtime_error("PersistentBPlusTree: root unpin failed");
    return id;
}

PageId PersistentBPlusTree::findLeafPage(const IndexKey& key) const {
    if (!rootPageId_) throw std::runtime_error("PersistentBPlusTree: tree has no root");
    PageId current = *rootPageId_;
    while (true) {
        Page* page = bufferPoolManager_.fetchPage(current);
        if (!page) throw std::runtime_error("PersistentBPlusTree: page fetch failed");
        IndexPage index(*page);
        if (index.pageType() == IndexPageType::Leaf) {
            if (!bufferPoolManager_.unpinPage(current, false)) throw std::runtime_error("PersistentBPlusTree: unpin failed");
            return current;
        }
        if (index.pageType() != IndexPageType::Internal) {
            bufferPoolManager_.unpinPage(current, false);
            throw std::runtime_error("PersistentBPlusTree: corrupt page type");
        }
        BPlusTreeInternalPage internal(*page);
        const PageId next = internal.lookupChild(key);
        if (!bufferPoolManager_.unpinPage(current, false)) throw std::runtime_error("PersistentBPlusTree: unpin failed");
        current = next;
    }
}

bool PersistentBPlusTree::insert(const IndexKey& key, RecordId recordId) {
    if (!rootPageId_) rootPageId_ = createLeafRoot();
    if (!insertIntoLeaf(key, recordId)) return false;
    ++size_;
    persistMetadata();
    return true;
}

bool PersistentBPlusTree::insertIntoLeaf(const IndexKey& key, RecordId recordId) {
    const PageId leafId = findLeafPage(key);
    Page* page = bufferPoolManager_.fetchPage(leafId);
    if (!page) throw std::runtime_error("PersistentBPlusTree: leaf fetch failed");
    BPlusTreeLeafPage leaf(*page);
    auto entries = leaf.entries();
    auto it = std::lower_bound(entries.begin(), entries.end(), BPlusTreeLeafPage::Entry{key, recordId},
        [](const auto& a, const auto& b) { if (a.key != b.key) return a.key < b.key; return a.recordId < b.recordId; });
    if (it != entries.end() && it->key == key && it->recordId == recordId) {
        bufferPoolManager_.unpinPage(leafId, false);
        return false;
    }
    entries.insert(it, {key, recordId});
    try {
        leaf.rewrite(entries);
        if (!bufferPoolManager_.unpinPage(leafId, true)) throw std::runtime_error("PersistentBPlusTree: leaf unpin failed");
        return true;
    } catch (const std::overflow_error&) {
        if (!bufferPoolManager_.unpinPage(leafId, false)) throw std::runtime_error("PersistentBPlusTree: leaf unpin failed");
        const auto split = splitLeaf(leafId);
        insertIntoParent(leafId, split.separator, split.rightPageId);
        // Re-find after split and insert the key into the correct side.
        const PageId target = findLeafPage(key);
        Page* targetPage = bufferPoolManager_.fetchPage(target);
        if (!targetPage) throw std::runtime_error("PersistentBPlusTree: target leaf fetch failed");
        BPlusTreeLeafPage targetLeaf(*targetPage);
        const bool ok = targetLeaf.insert(key, recordId);
        if (!bufferPoolManager_.unpinPage(target, ok)) throw std::runtime_error("PersistentBPlusTree: target leaf unpin failed");
        return ok;
    }
}

PersistentBPlusTree::SplitResult PersistentBPlusTree::splitLeaf(PageId leafPageId) {
    Page* page = bufferPoolManager_.fetchPage(leafPageId);
    if (!page) throw std::runtime_error("PersistentBPlusTree: split leaf fetch failed");
    BPlusTreeLeafPage leaf(*page);
    auto entries = leaf.entries();
    if (entries.size() < 2) {
        bufferPoolManager_.unpinPage(leafPageId, false);
        throw std::runtime_error("PersistentBPlusTree: cannot split leaf");
    }
    const std::size_t middle = entries.size() / 2;
    std::vector<BPlusTreeLeafPage::Entry> left(entries.begin(), entries.begin() + static_cast<std::ptrdiff_t>(middle));
    std::vector<BPlusTreeLeafPage::Entry> right(entries.begin() + static_cast<std::ptrdiff_t>(middle), entries.end());
    const PageId oldNext = leaf.nextPageId();
    const PageId parent = leaf.parentPageId();

    PageId rightId;
    Page* rightPage = bufferPoolManager_.newPage(rightId);
    if (!rightPage) {
        bufferPoolManager_.unpinPage(leafPageId, false);
        throw std::runtime_error("PersistentBPlusTree: right leaf allocation failed");
    }
    BPlusTreeLeafPage rightLeaf(*rightPage);
    rightLeaf.setPageType(IndexPageType::Leaf);
    rightLeaf.setParentPageId(parent);
    rightLeaf.setNextPageId(oldNext);
    rightLeaf.rewrite(right);
    leaf.rewrite(left);
    leaf.setNextPageId(rightId);
    if (!bufferPoolManager_.unpinPage(rightId, true) || !bufferPoolManager_.unpinPage(leafPageId, true))
        throw std::runtime_error("PersistentBPlusTree: split leaf unpin failed");
    return {rightId, right.front().key};
}

void PersistentBPlusTree::insertIntoParent(PageId leftPageId, const IndexKey& separator, PageId rightPageId) {
    Page* leftPage = bufferPoolManager_.fetchPage(leftPageId);
    if (!leftPage) throw std::runtime_error("PersistentBPlusTree: left page fetch failed");
    IndexPage left(*leftPage);
    const PageId parentId = left.parentPageId();
    if (!bufferPoolManager_.unpinPage(leftPageId, false)) throw std::runtime_error("PersistentBPlusTree: left unpin failed");
    if (isNullPage(parentId)) {
        createNewRoot(leftPageId, separator, rightPageId);
        return;
    }

    Page* parentPage = bufferPoolManager_.fetchPage(parentId);
    if (!parentPage) throw std::runtime_error("PersistentBPlusTree: parent fetch failed");
    BPlusTreeInternalPage parent(*parentPage);
    auto children = parent.children();
    auto keys = parent.keys();
    auto it = std::find(children.begin(), children.end(), leftPageId);
    if (it == children.end()) {
        bufferPoolManager_.unpinPage(parentId, false);
        throw std::runtime_error("PersistentBPlusTree: child missing from parent");
    }
    const std::size_t childIndex = static_cast<std::size_t>(it - children.begin());
    keys.insert(keys.begin() + static_cast<std::ptrdiff_t>(childIndex), separator);
    children.insert(children.begin() + static_cast<std::ptrdiff_t>(childIndex + 1), rightPageId);
    try {
        parent.rewrite(keys, children);
        if (!bufferPoolManager_.unpinPage(parentId, true)) throw std::runtime_error("PersistentBPlusTree: parent unpin failed");
    } catch (const std::overflow_error&) {
        bufferPoolManager_.unpinPage(parentId, false);
        // Restore parent with its previous state and split the logical contents.
        auto split = splitInternalWithInsertion(parentId, separator, rightPageId, leftPageId);
        insertIntoParent(parentId, split.separator, split.rightPageId);
        return;
    }
    Page* rightPage = bufferPoolManager_.fetchPage(rightPageId);
    if (!rightPage) throw std::runtime_error("PersistentBPlusTree: right page fetch failed");
    IndexPage right(*rightPage);
    right.setParentPageId(parentId);
    if (!bufferPoolManager_.unpinPage(rightPageId, true)) throw std::runtime_error("PersistentBPlusTree: right unpin failed");
}

PersistentBPlusTree::SplitResult PersistentBPlusTree::splitInternal(PageId internalPageId) {
    Page* page = bufferPoolManager_.fetchPage(internalPageId);
    if (!page) throw std::runtime_error("PersistentBPlusTree: internal fetch failed");
    BPlusTreeInternalPage internal(*page);
    const auto keys = internal.keys();
    const auto children = internal.children();
    if (keys.size() < 2) {
        bufferPoolManager_.unpinPage(internalPageId, false);
        throw std::runtime_error("PersistentBPlusTree: cannot split internal page");
    }
    const std::size_t middle = keys.size() / 2;
    const IndexKey separator = keys[middle];
    std::vector<IndexKey> leftKeys(keys.begin(), keys.begin() + static_cast<std::ptrdiff_t>(middle));
    std::vector<PageId> leftChildren(children.begin(), children.begin() + static_cast<std::ptrdiff_t>(middle + 1));
    std::vector<IndexKey> rightKeys(keys.begin() + static_cast<std::ptrdiff_t>(middle + 1), keys.end());
    std::vector<PageId> rightChildren(children.begin() + static_cast<std::ptrdiff_t>(middle + 1), children.end());
    const PageId parent = internal.parentPageId();

    PageId rightId;
    Page* rightPage = bufferPoolManager_.newPage(rightId);
    if (!rightPage) {
        bufferPoolManager_.unpinPage(internalPageId, false);
        throw std::runtime_error("PersistentBPlusTree: right internal allocation failed");
    }
    BPlusTreeInternalPage right(*rightPage);
    right.setPageType(IndexPageType::Internal);
    right.setParentPageId(parent);
    right.rewrite(rightKeys, rightChildren);
    internal.rewrite(leftKeys, leftChildren);
    if (!bufferPoolManager_.unpinPage(rightId, true) || !bufferPoolManager_.unpinPage(internalPageId, true))
        throw std::runtime_error("PersistentBPlusTree: internal split unpin failed");
    for (PageId child : rightChildren) {
        Page* childPage = bufferPoolManager_.fetchPage(child);
        if (!childPage) throw std::runtime_error("PersistentBPlusTree: child fetch failed");
        IndexPage childIndex(*childPage);
        childIndex.setParentPageId(rightId);
        if (!bufferPoolManager_.unpinPage(child, true)) throw std::runtime_error("PersistentBPlusTree: child unpin failed");
    }
    return {rightId, separator};
}

PersistentBPlusTree::SplitResult PersistentBPlusTree::splitInternalWithInsertion(
    PageId internalPageId, const IndexKey& separator, PageId rightChild, PageId leftChild) {
    Page* page = bufferPoolManager_.fetchPage(internalPageId);
    if (!page) throw std::runtime_error("PersistentBPlusTree: internal fetch failed");
    BPlusTreeInternalPage internal(*page);
    auto keys = internal.keys();
    auto children = internal.children();
    auto it = std::find(children.begin(), children.end(), leftChild);
    if (it == children.end()) {
        bufferPoolManager_.unpinPage(internalPageId, false);
        throw std::runtime_error("PersistentBPlusTree: split insertion child missing");
    }
    const std::size_t index = static_cast<std::size_t>(it - children.begin());
    keys.insert(keys.begin() + static_cast<std::ptrdiff_t>(index), separator);
    children.insert(children.begin() + static_cast<std::ptrdiff_t>(index + 1), rightChild);
    const std::size_t middle = keys.size() / 2;
    const IndexKey promoted = keys[middle];
    std::vector<IndexKey> leftKeys(keys.begin(), keys.begin() + static_cast<std::ptrdiff_t>(middle));
    std::vector<PageId> leftChildren(children.begin(), children.begin() + static_cast<std::ptrdiff_t>(middle + 1));
    std::vector<IndexKey> rightKeys(keys.begin() + static_cast<std::ptrdiff_t>(middle + 1), keys.end());
    std::vector<PageId> rightChildren(children.begin() + static_cast<std::ptrdiff_t>(middle + 1), children.end());
    const PageId parent = internal.parentPageId();
    PageId rightId;
    Page* rightPage = bufferPoolManager_.newPage(rightId);
    if (!rightPage) {
        bufferPoolManager_.unpinPage(internalPageId, false);
        throw std::runtime_error("PersistentBPlusTree: right internal allocation failed");
    }
    BPlusTreeInternalPage right(*rightPage);
    right.setPageType(IndexPageType::Internal);
    right.setParentPageId(parent);
    right.rewrite(rightKeys, rightChildren);
    internal.rewrite(leftKeys, leftChildren);
    if (!bufferPoolManager_.unpinPage(rightId, true) || !bufferPoolManager_.unpinPage(internalPageId, true))
        throw std::runtime_error("PersistentBPlusTree: internal split unpin failed");
    for (PageId child : rightChildren) {
        Page* childPage = bufferPoolManager_.fetchPage(child);
        if (!childPage) throw std::runtime_error("PersistentBPlusTree: child fetch failed");
        IndexPage childIndex(*childPage);
        childIndex.setParentPageId(rightId);
        if (!bufferPoolManager_.unpinPage(child, true)) throw std::runtime_error("PersistentBPlusTree: child unpin failed");
    }
    return {rightId, promoted};
}

void PersistentBPlusTree::createNewRoot(PageId leftPageId, const IndexKey& separator, PageId rightPageId) {
    PageId rootId;
    Page* page = bufferPoolManager_.newPage(rootId);
    if (!page) throw std::runtime_error("PersistentBPlusTree: root allocation failed");
    BPlusTreeInternalPage root(*page);
    root.setPageType(IndexPageType::Internal);
    root.setParentPageId(PageId{});
    root.rewrite({separator}, {leftPageId, rightPageId});
    if (!bufferPoolManager_.unpinPage(rootId, true)) throw std::runtime_error("PersistentBPlusTree: root unpin failed");
    for (PageId child : {leftPageId, rightPageId}) {
        Page* childPage = bufferPoolManager_.fetchPage(child);
        if (!childPage) throw std::runtime_error("PersistentBPlusTree: child fetch failed");
        IndexPage childIndex(*childPage);
        childIndex.setParentPageId(rootId);
        if (!bufferPoolManager_.unpinPage(child, true)) throw std::runtime_error("PersistentBPlusTree: child unpin failed");
    }
    rootPageId_ = rootId;
    persistMetadata();
}

std::vector<RecordId> PersistentBPlusTree::lookup(const IndexKey& key) const {
    if (!rootPageId_) return {};
    const PageId leafId = findLeafPage(key);
    std::vector<RecordId> result;
    PageId current = leafId;
    while (!isNullPage(current)) {
        Page* page = bufferPoolManager_.fetchPage(current);
        if (!page) throw std::runtime_error("PersistentBPlusTree: leaf fetch failed");
        BPlusTreeLeafPage leaf(*page);
        const auto matches = leaf.lookup(key);
        result.insert(result.end(), matches.begin(), matches.end());
        const PageId next = leaf.nextPageId();
        const bool stop = leaf.size() > 0 && key < leaf.keyAt(leaf.size() - 1) && matches.empty();
        if (!bufferPoolManager_.unpinPage(current, false)) throw std::runtime_error("PersistentBPlusTree: leaf unpin failed");
        if (stop) break;
        current = next;
    }
    return result;
}

std::vector<RecordId> PersistentBPlusTree::scan(const IndexKey& lower, const IndexKey& upper) const {
    if (upper < lower) throw std::invalid_argument("PersistentBPlusTree: invalid range");
    if (!rootPageId_) return {};
    std::vector<RecordId> result;
    PageId current = findLeafPage(lower);
    while (!isNullPage(current)) {
        Page* page = bufferPoolManager_.fetchPage(current);
        if (!page) throw std::runtime_error("PersistentBPlusTree: leaf fetch failed");
        BPlusTreeLeafPage leaf(*page);
        for (std::size_t i = leaf.lowerBound(lower); i < leaf.size(); ++i) {
            const auto key = leaf.keyAt(i);
            if (upper < key) {
                bufferPoolManager_.unpinPage(current, false);
                return result;
            }
            result.push_back(leaf.recordIdAt(i));
        }
        const PageId next = leaf.nextPageId();
        if (!bufferPoolManager_.unpinPage(current, false)) throw std::runtime_error("PersistentBPlusTree: leaf unpin failed");
        current = next;
    }
    return result;
}

bool PersistentBPlusTree::contains(const IndexKey& key) const { return !lookup(key).empty(); }

bool PersistentBPlusTree::remove(const IndexKey& key, RecordId recordId) {
    if (!rootPageId_) return false;
    const PageId leafId = findLeafPage(key);
    Page* page = bufferPoolManager_.fetchPage(leafId);
    if (!page) throw std::runtime_error("PersistentBPlusTree: leaf fetch failed");
    BPlusTreeLeafPage leaf(*page);
    if (!leaf.remove(key, recordId)) {
        bufferPoolManager_.unpinPage(leafId, false);
        return false;
    }
    const bool rootLeaf = leaf.parentPageId().value() == 0;
    const bool underflow = !rootLeaf && leaf.size() < 2;
    if (!bufferPoolManager_.unpinPage(leafId, true)) throw std::runtime_error("PersistentBPlusTree: leaf unpin failed");
    --size_;
    if (underflow) rebalanceLeaf(leafId);
    repairSeparators(*rootPageId_);
    if (size_ == 0) {
        // Keep an empty root leaf so future inserts have a stable root.
        collapseEmptyRoot();
    }
    persistMetadata();
    return true;
}

void PersistentBPlusTree::rebalanceLeaf(PageId leafId) {
    Page* page = bufferPoolManager_.fetchPage(leafId);
    if (!page) throw std::runtime_error("PersistentBPlusTree: leaf fetch failed");
    BPlusTreeLeafPage leaf(*page);
    const PageId parentId = leaf.parentPageId();
    const PageId next = leaf.nextPageId();
    if (isNullPage(parentId)) { bufferPoolManager_.unpinPage(leafId, true); return; }
    bufferPoolManager_.unpinPage(leafId, false);

    Page* parentPage = bufferPoolManager_.fetchPage(parentId);
    if (!parentPage) throw std::runtime_error("PersistentBPlusTree: parent fetch failed");
    BPlusTreeInternalPage parent(*parentPage);
    auto children = parent.children();
    auto it = std::find(children.begin(), children.end(), leafId);
    if (it == children.end()) { bufferPoolManager_.unpinPage(parentId, false); throw std::runtime_error("PersistentBPlusTree: leaf missing from parent"); }
    const std::size_t index = static_cast<std::size_t>(it - children.begin());

    if (index > 0) {
        const PageId leftId = children[index - 1];
        Page* lp = bufferPoolManager_.fetchPage(leftId);
        if (!lp) { bufferPoolManager_.unpinPage(parentId, false); throw std::runtime_error("PersistentBPlusTree: sibling fetch failed"); }
        BPlusTreeLeafPage left(*lp);
        if (left.size() > 2) {
            auto e = left.entries();
            auto moved = e.back(); e.pop_back();
            auto cur = leafEntries(leafId);
            cur.insert(cur.begin(), moved);
            left.rewrite(e);
            leaf.rewrite(cur);
            bufferPoolManager_.unpinPage(leftId, true);
            bufferPoolManager_.unpinPage(parentId, false);
            bufferPoolManager_.unpinPage(leafId, true);
            return;
        }
        bufferPoolManager_.unpinPage(leftId, false);
    }
    if (index + 1 < children.size()) {
        const PageId rightId = children[index + 1];
        Page* rp = bufferPoolManager_.fetchPage(rightId);
        if (!rp) { bufferPoolManager_.unpinPage(parentId, false); throw std::runtime_error("PersistentBPlusTree: sibling fetch failed"); }
        BPlusTreeLeafPage right(*rp);
        if (right.size() > 2) {
            auto e = right.entries(); auto moved = e.front(); e.erase(e.begin());
            auto cur = leaf.entries(); cur.push_back(moved);
            right.rewrite(e); leaf.rewrite(cur);
            bufferPoolManager_.unpinPage(rightId, true);
            bufferPoolManager_.unpinPage(parentId, false);
            bufferPoolManager_.unpinPage(leafId, true);
            return;
        }
        bufferPoolManager_.unpinPage(rightId, false);
    }
    // Merge into left when possible; otherwise merge right into current.
    if (index > 0) {
        const PageId leftId = children[index - 1];
        Page* lp = bufferPoolManager_.fetchPage(leftId); if (!lp) throw std::runtime_error("PersistentBPlusTree: sibling fetch failed");
        BPlusTreeLeafPage left(*lp);
        auto merged = left.entries(); auto cur = leaf.entries(); merged.insert(merged.end(), cur.begin(), cur.end());
        left.rewrite(merged); left.setNextPageId(next);
        parent.removeChild(index);
        bufferPoolManager_.unpinPage(leftId, true);
        bufferPoolManager_.unpinPage(leafId, false);
        bufferPoolManager_.unpinPage(parentId, true);
        bufferPoolManager_.deletePage(leafId);
        rebalanceInternal(parentId);
    } else if (index + 1 < children.size()) {
        const PageId rightId = children[index + 1];
        Page* rp = bufferPoolManager_.fetchPage(rightId); if (!rp) throw std::runtime_error("PersistentBPlusTree: sibling fetch failed");
        BPlusTreeLeafPage right(*rp);
        auto merged = leaf.entries(); auto re = right.entries(); merged.insert(merged.end(), re.begin(), re.end());
        leaf.rewrite(merged); leaf.setNextPageId(right.nextPageId());
        parent.removeChild(index + 1);
        bufferPoolManager_.unpinPage(rightId, false);
        bufferPoolManager_.unpinPage(leafId, true);
        bufferPoolManager_.unpinPage(parentId, true);
        bufferPoolManager_.deletePage(rightId);
        rebalanceInternal(parentId);
    } else {
        bufferPoolManager_.unpinPage(parentId, false);
        bufferPoolManager_.unpinPage(leafId, false);
    }
}

std::vector<BPlusTreeLeafPage::Entry> PersistentBPlusTree::leafEntries(PageId id) const {
    Page* page = bufferPoolManager_.fetchPage(id);
    if (!page) throw std::runtime_error("PersistentBPlusTree: leaf fetch failed");
    BPlusTreeLeafPage leaf(*page);
    auto result = leaf.entries();
    if (!bufferPoolManager_.unpinPage(id, false)) throw std::runtime_error("PersistentBPlusTree: leaf unpin failed");
    return result;
}

void PersistentBPlusTree::rebalanceInternal(PageId internalId) {
    if (internalId == rootPageId_.value_or(PageId{})) {
        Page* page = bufferPoolManager_.fetchPage(internalId); if (!page) throw std::runtime_error("PersistentBPlusTree: root fetch failed");
        BPlusTreeInternalPage root(*page);
        const auto children = root.children();
        if (children.size() == 1) {
            const PageId child = children.front();
            bufferPoolManager_.unpinPage(internalId, true);
            Page* childPage = bufferPoolManager_.fetchPage(child); if (!childPage) throw std::runtime_error("PersistentBPlusTree: child fetch failed");
            IndexPage childIndex(*childPage); childIndex.setParentPageId(PageId{}); bufferPoolManager_.unpinPage(child, true);
            bufferPoolManager_.deletePage(internalId);
            rootPageId_ = child;
        } else bufferPoolManager_.unpinPage(internalId, false);
        return;
    }
    Page* page = bufferPoolManager_.fetchPage(internalId); if (!page) throw std::runtime_error("PersistentBPlusTree: internal fetch failed");
    BPlusTreeInternalPage node(*page);
    if (node.childCount() >= 2) { bufferPoolManager_.unpinPage(internalId, false); return; }
    const PageId parentId = node.parentPageId(); bufferPoolManager_.unpinPage(internalId, false);
    if (isNullPage(parentId)) return;
    Page* pp = bufferPoolManager_.fetchPage(parentId); if (!pp) throw std::runtime_error("PersistentBPlusTree: parent fetch failed");
    BPlusTreeInternalPage parent(*pp); auto children = parent.children();
    auto it = std::find(children.begin(), children.end(), internalId); if (it == children.end()) { bufferPoolManager_.unpinPage(parentId,false); throw std::runtime_error("PersistentBPlusTree: internal child missing"); }
    const std::size_t index = static_cast<std::size_t>(it - children.begin());
    if (index > 0) {
        const PageId leftId = children[index-1]; Page* lp=bufferPoolManager_.fetchPage(leftId); if(!lp) throw std::runtime_error("PersistentBPlusTree: sibling fetch failed"); BPlusTreeInternalPage left(*lp);
        if(left.childCount()>2){ auto lc=left.children(); auto nc=childrenFor(internalId); auto moved=lc.back(); lc.pop_back(); nc.insert(nc.begin(),moved); left.rewrite(keysFor(leftId),lc); rewriteInternalFromChildren(internalId,nc); setParent(moved,internalId); bufferPoolManager_.unpinPage(leftId,true); bufferPoolManager_.unpinPage(parentId,false); return; }
        bufferPoolManager_.unpinPage(leftId,false);
    }
    if(index+1<children.size()){
        const PageId rightId=children[index+1]; Page* rp=bufferPoolManager_.fetchPage(rightId); if(!rp) throw std::runtime_error("PersistentBPlusTree: sibling fetch failed"); BPlusTreeInternalPage right(*rp);
        if(right.childCount()>2){ auto rc=right.children(); auto nc=childrenFor(internalId); auto moved=rc.front(); rc.erase(rc.begin()); nc.push_back(moved); right.rewrite(keysFor(rightId),rc); rewriteInternalFromChildren(internalId,nc); setParent(moved,internalId); bufferPoolManager_.unpinPage(rightId,true); bufferPoolManager_.unpinPage(parentId,false); return; }
        bufferPoolManager_.unpinPage(rightId,false);
    }
    if(index>0){
        const PageId leftId=children[index-1]; Page* lp=bufferPoolManager_.fetchPage(leftId); if(!lp) throw std::runtime_error("PersistentBPlusTree: sibling fetch failed"); BPlusTreeInternalPage left(*lp); auto lc=left.children(); auto nc=childrenFor(internalId); lc.insert(lc.end(),nc.begin(),nc.end()); left.rewrite(keysForChildren(lc),lc); for(PageId c:nc) setParent(c,leftId); parent.removeChild(index); bufferPoolManager_.unpinPage(leftId,true); bufferPoolManager_.unpinPage(internalId,false); bufferPoolManager_.unpinPage(parentId,true); bufferPoolManager_.deletePage(internalId); rebalanceInternal(parentId);
    } else if(index+1<children.size()){
        const PageId rightId=children[index+1]; Page* rp=bufferPoolManager_.fetchPage(rightId); if(!rp) throw std::runtime_error("PersistentBPlusTree: sibling fetch failed"); BPlusTreeInternalPage right(*rp); auto nc=childrenFor(internalId); auto rc=right.children(); nc.insert(nc.end(),rc.begin(),rc.end()); rewriteInternalFromChildren(internalId,nc); for(PageId c:rc) setParent(c,internalId); parent.removeChild(index+1); bufferPoolManager_.unpinPage(rightId,false); bufferPoolManager_.unpinPage(internalId,true); bufferPoolManager_.unpinPage(parentId,true); bufferPoolManager_.deletePage(rightId); rebalanceInternal(parentId);
    } else bufferPoolManager_.unpinPage(parentId,false);
}

std::vector<PageId> PersistentBPlusTree::childrenFor(PageId id) const { Page* p=bufferPoolManager_.fetchPage(id); if(!p) throw std::runtime_error("PersistentBPlusTree: page fetch failed"); BPlusTreeInternalPage n(*p); auto c=n.children(); bufferPoolManager_.unpinPage(id,false); return c; }
std::vector<IndexKey> PersistentBPlusTree::keysFor(PageId id) const { Page* p=bufferPoolManager_.fetchPage(id); if(!p) throw std::runtime_error("PersistentBPlusTree: page fetch failed"); BPlusTreeInternalPage n(*p); auto k=n.keys(); bufferPoolManager_.unpinPage(id,false); return k; }
std::vector<IndexKey> PersistentBPlusTree::keysForChildren(const std::vector<PageId>& children) const { std::vector<IndexKey> keys; for(std::size_t i=1;i<children.size();++i) keys.push_back(firstKey(children[i])); return keys; }
void PersistentBPlusTree::rewriteInternalFromChildren(PageId id,const std::vector<PageId>& children){ auto keys=keysForChildren(children); Page* p=bufferPoolManager_.fetchPage(id); if(!p) throw std::runtime_error("PersistentBPlusTree: page fetch failed"); BPlusTreeInternalPage n(*p); n.rewrite(keys,children); if(!bufferPoolManager_.unpinPage(id,true)) throw std::runtime_error("PersistentBPlusTree: unpin failed"); }
void PersistentBPlusTree::setParent(PageId childId,PageId parentId){ Page* p=bufferPoolManager_.fetchPage(childId); if(!p) throw std::runtime_error("PersistentBPlusTree: page fetch failed"); IndexPage n(*p); n.setParentPageId(parentId); if(!bufferPoolManager_.unpinPage(childId,true)) throw std::runtime_error("PersistentBPlusTree: unpin failed"); }
IndexKey PersistentBPlusTree::firstKey(PageId id) const { Page* p=bufferPoolManager_.fetchPage(id); if(!p) throw std::runtime_error("PersistentBPlusTree: page fetch failed"); IndexPage idx(*p); if(idx.pageType()==IndexPageType::Leaf){ BPlusTreeLeafPage l(*p); if(l.isEmpty()) {bufferPoolManager_.unpinPage(id,false); throw std::runtime_error("PersistentBPlusTree: empty subtree");} auto k=l.keyAt(0); bufferPoolManager_.unpinPage(id,false); return k;} BPlusTreeInternalPage n(*p); auto child=n.childAt(0); bufferPoolManager_.unpinPage(id,false); return firstKey(child); }
void PersistentBPlusTree::repairSeparators(PageId id){ Page* p=bufferPoolManager_.fetchPage(id); if(!p) throw std::runtime_error("PersistentBPlusTree: page fetch failed"); IndexPage idx(*p); if(idx.pageType()==IndexPageType::Leaf){bufferPoolManager_.unpinPage(id,false); return;} BPlusTreeInternalPage n(*p); auto children=n.children(); bufferPoolManager_.unpinPage(id,false); for(PageId c:children) repairSeparators(c); auto keys=keysForChildren(children); Page* q=bufferPoolManager_.fetchPage(id); if(!q) throw std::runtime_error("PersistentBPlusTree: page fetch failed"); BPlusTreeInternalPage fixed(*q); fixed.rewrite(keys,children); bufferPoolManager_.unpinPage(id,true); }
void PersistentBPlusTree::collapseEmptyRoot(){ if(!rootPageId_) return; Page* p=bufferPoolManager_.fetchPage(*rootPageId_); if(!p) throw std::runtime_error("PersistentBPlusTree: root fetch failed"); IndexPage idx(*p); if(idx.pageType()==IndexPageType::Leaf){bufferPoolManager_.unpinPage(*rootPageId_,false); return;} BPlusTreeInternalPage root(*p); if(root.childCount()==1){PageId child=root.childAt(0); bufferPoolManager_.unpinPage(*rootPageId_,false); setParent(child,PageId{}); bufferPoolManager_.deletePage(*rootPageId_); rootPageId_=child;} else bufferPoolManager_.unpinPage(*rootPageId_,false); }

std::size_t PersistentBPlusTree::size() const noexcept { return size_; }
PageId PersistentBPlusTree::rootPageId() const noexcept { return rootPageId_.value_or(PageId{}); }
PageId PersistentBPlusTree::metadataPageId() const noexcept { return metadataPageId_; }

} // namespace forgedb
