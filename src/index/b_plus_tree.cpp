#include "forgedb/index/b_plus_tree.h"

#include <algorithm>
#include <iterator>
#include <stdexcept>
#include <utility>

namespace forgedb {

BPlusTree::BPlusTree(std::size_t nodeCapacity)
    : nodeCapacity_(nodeCapacity)
{
    if (nodeCapacity_ < 2) {
        throw std::invalid_argument(
            "BPlusTree: node capacity must be at least 2"
        );
    }
}

bool BPlusTree::insert(
    const IndexKey& key,
    RecordId recordId)
{
    if (root_ == nullptr) {
        root_ = std::make_unique<Node>(true);
    }

    Node* leaf = findLeaf(key);

    if (leaf == nullptr) {
        throw std::logic_error(
            "BPlusTree: failed to locate leaf"
        );
    }

    if (!insertIntoLeaf(
            *leaf,
            key,
            recordId)) {
        return false;
    }

    ++size_;

    if (leaf->entries.size() > nodeCapacity_) {
        splitLeaf(*leaf);
    }

    return true;
}

bool BPlusTree::remove(
    const IndexKey& key,
    RecordId recordId)
{
    Node* leaf = findLeaf(key);

    if (leaf == nullptr) {
        return false;
    }

    if (!removeFromLeaf(
            *leaf,
            key,
            recordId)) {
        return false;
    }

    --size_;

    // Separator keys are derived from the first key
    // of each child. Refresh them after deletion.
    if (root_ != nullptr && !root_->leaf) {
        refreshSeparators(*root_);
    }

    return true;
}

std::vector<RecordId> BPlusTree::lookup(
    const IndexKey& key) const
{
    std::vector<RecordId> result;

    const Node* leaf = findLeaf(key);

    if (leaf == nullptr) {
        return result;
    }

    for (const Entry& entry : leaf->entries) {
        if (entry.key == key) {
            result.push_back(entry.recordId);
            continue;
        }

        if (key < entry.key) {
            break;
        }
    }

    return result;
}

std::vector<RecordId> BPlusTree::scan(
    const IndexKey& lowerBound,
    const IndexKey& upperBound) const
{
    if (upperBound < lowerBound) {
        throw std::invalid_argument(
            "BPlusTree: invalid scan range"
        );
    }

    std::vector<RecordId> result;

    const Node* leaf =
        findLeaf(lowerBound);

    while (leaf != nullptr) {
        for (const Entry& entry : leaf->entries) {
            if (entry.key < lowerBound) {
                continue;
            }

            if (upperBound < entry.key) {
                return result;
            }

            result.push_back(entry.recordId);
        }

        leaf = leaf->next;
    }

    return result;
}

bool BPlusTree::contains(
    const IndexKey& key) const
{
    const Node* leaf = findLeaf(key);

    if (leaf == nullptr) {
        return false;
    }

    for (const Entry& entry : leaf->entries) {
        if (entry.key == key) {
            return true;
        }

        if (key < entry.key) {
            return false;
        }
    }

    return false;
}

std::size_t BPlusTree::size() const noexcept
{
    return size_;
}

BPlusTree::Node*
BPlusTree::findLeaf(
    const IndexKey& key) noexcept
{
    if (root_ == nullptr) {
        return nullptr;
    }

    Node* node = root_.get();

    while (!node->leaf) {
        const auto iterator =
            std::upper_bound(
                node->keys.begin(),
                node->keys.end(),
                key,
                [](const IndexKey& value,
                   const IndexKey& separator) {
                    return value < separator;
                }
            );

        const std::size_t childIndex =
            static_cast<std::size_t>(
                iterator - node->keys.begin()
            );

        if (childIndex >= node->children.size()) {
            return nullptr;
        }

        node = node->children[childIndex].get();
    }

    return node;
}

const BPlusTree::Node*
BPlusTree::findLeaf(
    const IndexKey& key) const noexcept
{
    if (root_ == nullptr) {
        return nullptr;
    }

    const Node* node = root_.get();

    while (!node->leaf) {
        const auto iterator =
            std::upper_bound(
                node->keys.begin(),
                node->keys.end(),
                key,
                [](const IndexKey& value,
                   const IndexKey& separator) {
                    return value < separator;
                }
            );

        const std::size_t childIndex =
            static_cast<std::size_t>(
                iterator - node->keys.begin()
            );

        if (childIndex >= node->children.size()) {
            return nullptr;
        }

        node = node->children[childIndex].get();
    }

    return node;
}

bool BPlusTree::insertIntoLeaf(
    Node& leaf,
    const IndexKey& key,
    RecordId recordId)
{
    const Entry entry{
        key,
        recordId
    };

    const auto iterator =
        std::lower_bound(
            leaf.entries.begin(),
            leaf.entries.end(),
            entry,
            [](const Entry& lhs,
               const Entry& rhs) {
                if (lhs.key != rhs.key) {
                    return lhs.key < rhs.key;
                }

                return lhs.recordId < rhs.recordId;
            }
        );

    if (iterator != leaf.entries.end() &&
        entriesEqual(*iterator, entry)) {
        return false;
    }

    leaf.entries.insert(
        iterator,
        entry
    );

    return true;
}

void BPlusTree::splitLeaf(Node& leaf)
{
    auto newLeaf =
        std::make_unique<Node>(true);

    Node* newLeafPtr = newLeaf.get();

    newLeaf->parent = leaf.parent;

    const std::size_t midpoint =
        leaf.entries.size() / 2;

    newLeaf->entries.insert(
        newLeaf->entries.end(),
        std::make_move_iterator(
            leaf.entries.begin() +
            static_cast<std::ptrdiff_t>(midpoint)
        ),
        std::make_move_iterator(
            leaf.entries.end()
        )
    );

    leaf.entries.erase(
        leaf.entries.begin() +
        static_cast<std::ptrdiff_t>(midpoint),
        leaf.entries.end()
    );

    // Maintain the linked list of leaves.
    newLeaf->next = leaf.next;
    leaf.next = newLeafPtr;

    const IndexKey separator =
        newLeaf->entries.front().key;

    insertIntoParent(
        leaf,
        separator,
        std::move(newLeaf)
    );
}

void BPlusTree::insertIntoParent(
    Node& left,
    const IndexKey& separator,
    std::unique_ptr<Node> right)
{
    if (left.parent == nullptr) {
        auto newRoot =
            std::make_unique<Node>(false);

        Node* newRootPtr = newRoot.get();

        std::unique_ptr<Node> leftOwner =
            std::move(root_);

        leftOwner->parent = newRootPtr;
        right->parent = newRootPtr;

        newRoot->keys.push_back(separator);

        newRoot->children.push_back(
            std::move(leftOwner)
        );

        newRoot->children.push_back(
            std::move(right)
        );

        root_ = std::move(newRoot);

        return;
    }

    Node* parent = left.parent;

    auto iterator =
        std::find_if(
            parent->children.begin(),
            parent->children.end(),
            [&left](const std::unique_ptr<Node>& child) {
                return child.get() == &left;
            }
        );

    if (iterator == parent->children.end()) {
        throw std::logic_error(
            "BPlusTree: parent does not contain child"
        );
    }

    const std::size_t childIndex =
        static_cast<std::size_t>(
            iterator - parent->children.begin()
        );

    parent->keys.insert(
        parent->keys.begin() +
        static_cast<std::ptrdiff_t>(childIndex),
        separator
    );

    right->parent = parent;

    parent->children.insert(
        parent->children.begin() +
        static_cast<std::ptrdiff_t>(childIndex + 1),
        std::move(right)
    );

    if (parent->keys.size() > nodeCapacity_) {
        splitInternal(*parent);
    }
}

void BPlusTree::splitInternal(Node& node)
{
    auto right =
        std::make_unique<Node>(false);

    Node* rightPtr = right.get();

    right->parent = node.parent;

    const std::size_t midpoint =
        node.keys.size() / 2;

    // The middle key is promoted to the parent.
    const IndexKey promotedKey =
        node.keys[midpoint];

    // Keys after the promoted key belong to the right node.
    right->keys.insert(
        right->keys.end(),
        std::make_move_iterator(
            node.keys.begin() +
            static_cast<std::ptrdiff_t>(midpoint + 1)
        ),
        std::make_move_iterator(
            node.keys.end()
        )
    );

    node.keys.erase(
        node.keys.begin() +
        static_cast<std::ptrdiff_t>(midpoint),
        node.keys.end()
    );

    // An internal node with N keys has N + 1 children.
    const std::size_t childSplit =
        midpoint + 1;

    for (std::size_t i = childSplit;
         i < node.children.size();
         ++i) {

        node.children[i]->parent = rightPtr;

        right->children.push_back(
            std::move(node.children[i])
        );
    }

    node.children.erase(
        node.children.begin() +
        static_cast<std::ptrdiff_t>(childSplit),
        node.children.end()
    );

    insertIntoParent(
        node,
        promotedKey,
        std::move(right)
    );
}

bool BPlusTree::removeFromLeaf(
    Node& leaf,
    const IndexKey& key,
    RecordId recordId)
{
    const Entry target{
        key,
        recordId
    };

    const auto iterator =
        std::lower_bound(
            leaf.entries.begin(),
            leaf.entries.end(),
            target,
            [](const Entry& lhs,
               const Entry& rhs) {
                if (lhs.key != rhs.key) {
                    return lhs.key < rhs.key;
                }

                return lhs.recordId < rhs.recordId;
            }
        );

    if (iterator == leaf.entries.end() ||
        !entriesEqual(*iterator, target)) {
        return false;
    }

    leaf.entries.erase(iterator);

    return true;
}

void BPlusTree::refreshSeparators(Node& node)
{
    if (node.leaf) {
        return;
    }

    for (auto& child : node.children) {
        refreshSeparators(*child);
    }

    node.keys.clear();

    if (node.children.size() < 2) {
        return;
    }

    for (std::size_t i = 1;
         i < node.children.size();
         ++i) {

        Node* child = node.children[i].get();

        while (!child->leaf) {
            if (child->children.empty()) {
                break;
            }

            child = child->children.front().get();
        }

        if (!child->entries.empty()) {
            node.keys.push_back(
                child->entries.front().key
            );
        }
    }
}

bool BPlusTree::entriesEqual(
    const Entry& lhs,
    const Entry& rhs)
{
    return lhs.key == rhs.key &&
           lhs.recordId == rhs.recordId;
}

} // namespace forgedb