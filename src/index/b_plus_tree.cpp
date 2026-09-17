#include "forgedb/index/b_plus_tree.h"

#include <algorithm>
#include <iterator>
#include <stdexcept>
#include <utility>

namespace forgedb {



BPlusTree::BPlusTree(std::size_t nodeCapacity)
    : nodeCapacity_(nodeCapacity) {
    if (nodeCapacity_ < 3) {
        throw std::invalid_argument("BPlusTree: node capacity must be at least 3");
    }
}

bool BPlusTree::insert(const IndexKey& key, RecordId recordId) {
    if (!root_) root_ = std::make_unique<Node>(true);
    Node* leaf = findLeaf(key);
    if (!leaf) throw std::logic_error("BPlusTree: failed to locate leaf");
    if (!insertIntoLeaf(*leaf, key, recordId)) return false;
    ++size_;
    if (leaf->entries.size() > nodeCapacity_) splitLeaf(*leaf);
    return true;
}

bool BPlusTree::remove(const IndexKey& key, RecordId recordId) {
    Node* leaf = findLeaf(key);
    if (!leaf || !removeFromLeaf(*leaf, key, recordId)) return false;
    --size_;
    rebalanceAfterDelete(*leaf);
    if (root_ && !root_->leaf) refreshSeparators(*root_);
    return true;
}

std::vector<RecordId> BPlusTree::lookup(const IndexKey& key) const {
    std::vector<RecordId> result;
    const Node* leaf = findLeaf(key);
    if (!leaf) return result;
    for (const Node* current = leaf; current; current = current->next) {
        for (const Entry& entry : current->entries) {
            if (entry.key == key) result.push_back(entry.recordId);
            else if (key < entry.key) return result;
        }
    }
    return result;
}

std::vector<RecordId> BPlusTree::scan(const IndexKey& lower, const IndexKey& upper) const {
    if (upper < lower) throw std::invalid_argument("BPlusTree: invalid scan range");
    std::vector<RecordId> result;
    const Node* leaf = findLeaf(lower);
    while (leaf) {
        for (const Entry& entry : leaf->entries) {
            if (entry.key < lower) continue;
            if (upper < entry.key) return result;
            result.push_back(entry.recordId);
        }
        leaf = leaf->next;
    }
    return result;
}

bool BPlusTree::contains(const IndexKey& key) const { return !lookup(key).empty(); }
std::size_t BPlusTree::size() const noexcept { return size_; }

BPlusTree::Node* BPlusTree::findLeaf(const IndexKey& key) noexcept {
    if (!root_) return nullptr;
    Node* node = root_.get();
    while (!node->leaf) {
        auto it = std::upper_bound(node->keys.begin(), node->keys.end(), key,
            [](const IndexKey& value, const IndexKey& separator) { return value < separator; });
        const std::size_t index = static_cast<std::size_t>(it - node->keys.begin());
        if (index >= node->children.size()) return nullptr;
        node = node->children[index].get();
    }
    return node;
}

const BPlusTree::Node* BPlusTree::findLeaf(const IndexKey& key) const noexcept {
    if (!root_) return nullptr;
    const Node* node = root_.get();
    while (!node->leaf) {
        auto it = std::upper_bound(node->keys.begin(), node->keys.end(), key,
            [](const IndexKey& value, const IndexKey& separator) { return value < separator; });
        const std::size_t index = static_cast<std::size_t>(it - node->keys.begin());
        if (index >= node->children.size()) return nullptr;
        node = node->children[index].get();
    }
    return node;
}

bool BPlusTree::insertIntoLeaf(Node& leaf, const IndexKey& key, RecordId recordId) {
    Entry entry{key, recordId};
    auto it = std::lower_bound(leaf.entries.begin(), leaf.entries.end(), entry, [](const Entry& lhs, const Entry& rhs) { if (lhs.key != rhs.key) return lhs.key < rhs.key; return lhs.recordId < rhs.recordId; });
    if (it != leaf.entries.end() && entriesEqual(*it, entry)) return false;
    leaf.entries.insert(it, std::move(entry));
    return true;
}

void BPlusTree::splitLeaf(Node& leaf) {
    auto right = std::make_unique<Node>(true);
    right->parent = leaf.parent;
    Node* rightPtr = right.get();
    const std::size_t midpoint = leaf.entries.size() / 2;
    right->entries.insert(right->entries.end(),
        std::make_move_iterator(leaf.entries.begin() + static_cast<std::ptrdiff_t>(midpoint)),
        std::make_move_iterator(leaf.entries.end()));
    leaf.entries.erase(leaf.entries.begin() + static_cast<std::ptrdiff_t>(midpoint), leaf.entries.end());
    right->next = leaf.next;
    leaf.next = rightPtr;
    const IndexKey separator = right->entries.front().key;
    insertIntoParent(leaf, separator, std::move(right));
}

void BPlusTree::insertIntoParent(Node& left, const IndexKey& separator, std::unique_ptr<Node> right) {
    if (!left.parent) {
        auto newRoot = std::make_unique<Node>(false);
        Node* rootPtr = newRoot.get();
        std::unique_ptr<Node> leftOwner = std::move(root_);
        leftOwner->parent = rootPtr;
        right->parent = rootPtr;
        newRoot->keys.push_back(separator);
        newRoot->children.push_back(std::move(leftOwner));
        newRoot->children.push_back(std::move(right));
        root_ = std::move(newRoot);
        return;
    }
    Node* parent = left.parent;
    auto it = std::find_if(parent->children.begin(), parent->children.end(),
        [&left](const auto& child) { return child.get() == &left; });
    if (it == parent->children.end()) throw std::logic_error("BPlusTree: parent does not contain child");
    const std::size_t index = static_cast<std::size_t>(it - parent->children.begin());
    parent->keys.insert(parent->keys.begin() + static_cast<std::ptrdiff_t>(index), separator);
    right->parent = parent;
    parent->children.insert(parent->children.begin() + static_cast<std::ptrdiff_t>(index + 1), std::move(right));
    if (parent->keys.size() > nodeCapacity_) splitInternal(*parent);
}

void BPlusTree::splitInternal(Node& node) {
    auto right = std::make_unique<Node>(false);
    Node* rightPtr = right.get();
    right->parent = node.parent;
    const std::size_t midpoint = node.keys.size() / 2;
    const IndexKey promoted = node.keys[midpoint];
    right->keys.insert(right->keys.end(),
        std::make_move_iterator(node.keys.begin() + static_cast<std::ptrdiff_t>(midpoint + 1)),
        std::make_move_iterator(node.keys.end()));
    node.keys.erase(node.keys.begin() + static_cast<std::ptrdiff_t>(midpoint), node.keys.end());
    const std::size_t childSplit = midpoint + 1;
    for (std::size_t i = childSplit; i < node.children.size(); ++i) {
        node.children[i]->parent = rightPtr;
        right->children.push_back(std::move(node.children[i]));
    }
    node.children.erase(node.children.begin() + static_cast<std::ptrdiff_t>(childSplit), node.children.end());
    insertIntoParent(node, promoted, std::move(right));
}

bool BPlusTree::removeFromLeaf(Node& leaf, const IndexKey& key, RecordId recordId) {
    Entry target{key, recordId};
    auto it = std::lower_bound(leaf.entries.begin(), leaf.entries.end(), target, [](const Entry& lhs, const Entry& rhs) { if (lhs.key != rhs.key) return lhs.key < rhs.key; return lhs.recordId < rhs.recordId; });
    if (it == leaf.entries.end() || !entriesEqual(*it, target)) return false;
    leaf.entries.erase(it);
    return true;
}

void BPlusTree::rebalanceAfterDelete(Node& node) {
    if (&node == root_.get()) {
        if (!node.leaf && node.children.size() == 1) {
            std::unique_ptr<Node> newRoot = std::move(node.children.front());
            newRoot->parent = nullptr;
            root_ = std::move(newRoot);
        }
        return;
    }

    const std::size_t minLeaf = (nodeCapacity_ + 1) / 2;
    const std::size_t minInternalChildren = (nodeCapacity_ + 2) / 2;
    const std::size_t occupancy = node.leaf ? node.entries.size() : node.children.size();
    const std::size_t minimum = node.leaf ? minLeaf : minInternalChildren;
    if (occupancy >= minimum) return;

    Node* parent = node.parent;
    auto it = std::find_if(parent->children.begin(), parent->children.end(),
        [&node](const auto& child) { return child.get() == &node; });
    if (it == parent->children.end()) throw std::logic_error("BPlusTree: parent does not contain underflowing node");
    const std::size_t index = static_cast<std::size_t>(it - parent->children.begin());

    if (index > 0) {
        Node* left = parent->children[index - 1].get();
        const std::size_t leftOcc = left->leaf ? left->entries.size() : left->children.size();
        if (leftOcc > minimum) {
            if (node.leaf) {
                node.entries.insert(node.entries.begin(), std::move(left->entries.back()));
                left->entries.pop_back();
            } else {
                auto moved = std::move(left->children.back());
                left->children.pop_back();
                moved->parent = &node;
                node.children.insert(node.children.begin(), std::move(moved));
            }
            refreshSeparators(*root_);
            return;
        }
    }

    if (index + 1 < parent->children.size()) {
        Node* right = parent->children[index + 1].get();
        const std::size_t rightOcc = right->leaf ? right->entries.size() : right->children.size();
        if (rightOcc > minimum) {
            if (node.leaf) {
                node.entries.push_back(std::move(right->entries.front()));
                right->entries.erase(right->entries.begin());
            } else {
                auto moved = std::move(right->children.front());
                right->children.erase(right->children.begin());
                moved->parent = &node;
                node.children.push_back(std::move(moved));
            }
            refreshSeparators(*root_);
            return;
        }
    }

    if (index > 0) {
        Node* left = parent->children[index - 1].get();
        if (node.leaf) {
            left->entries.insert(left->entries.end(),
                std::make_move_iterator(node.entries.begin()),
                std::make_move_iterator(node.entries.end()));
            left->next = node.next;
        } else {
            for (auto& child : node.children) {
                child->parent = left;
                left->children.push_back(std::move(child));
            }
        }
        parent->children.erase(parent->children.begin() + static_cast<std::ptrdiff_t>(index));
    } else {
        Node* right = parent->children[index + 1].get();
        if (node.leaf) {
            node.entries.insert(node.entries.end(),
                std::make_move_iterator(right->entries.begin()),
                std::make_move_iterator(right->entries.end()));
            node.next = right->next;
        } else {
            for (auto& child : right->children) {
                child->parent = &node;
                node.children.push_back(std::move(child));
            }
        }
        parent->children.erase(parent->children.begin() + static_cast<std::ptrdiff_t>(index + 1));
    }

    refreshSeparators(*root_);
    rebalanceAfterDelete(*parent);
}

void BPlusTree::refreshSeparators(Node& node) {
    if (node.leaf) return;
    for (auto& child : node.children) refreshSeparators(*child);
    node.keys.clear();
    for (std::size_t i = 1; i < node.children.size(); ++i) {
        Node* child = node.children[i].get();
        while (!child->leaf && !child->children.empty()) child = child->children.front().get();
        if (child->leaf && !child->entries.empty()) node.keys.push_back(child->entries.front().key);
    }
}

bool BPlusTree::entriesEqual(const Entry& lhs, const Entry& rhs) {
    return lhs.key == rhs.key && lhs.recordId == rhs.recordId;
}

} // namespace forgedb
