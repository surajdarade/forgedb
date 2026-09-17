#pragma once

#include <cstddef>
#include <memory>
#include <vector>

#include "forgedb/index/index.h"

namespace forgedb {

class BPlusTree final : public Index {
public:
    explicit BPlusTree(std::size_t nodeCapacity = 4);

    ~BPlusTree() override = default;

    BPlusTree(const BPlusTree&) = delete;
    BPlusTree& operator=(const BPlusTree&) = delete;

    BPlusTree(BPlusTree&&) = delete;
    BPlusTree& operator=(BPlusTree&&) = delete;

    [[nodiscard]] bool insert(
        const IndexKey& key,
        RecordId recordId
    ) override;

    [[nodiscard]] bool remove(
        const IndexKey& key,
        RecordId recordId
    ) override;

    [[nodiscard]] std::vector<RecordId> lookup(
        const IndexKey& key
    ) const override;

    [[nodiscard]] std::vector<RecordId> scan(
        const IndexKey& lowerBound,
        const IndexKey& upperBound
    ) const override;

    [[nodiscard]] bool contains(
        const IndexKey& key
    ) const override;

    [[nodiscard]] std::size_t size() const noexcept override;

private:
    struct Entry {
        IndexKey key;
        RecordId recordId;
    };

    struct Node {
        explicit Node(bool isLeaf)
            : leaf(isLeaf)
        {
        }

        bool leaf;

        // Internal nodes:
        //
        // keys[i] separates children[i] and children[i + 1].
        std::vector<IndexKey> keys;

        // Internal node ownership.
        std::vector<std::unique_ptr<Node>> children;

        // Leaf node data.
        std::vector<Entry> entries;

        // Non-owning pointer used only for leaf-level range scans.
        Node* next{nullptr};

        // Non-owning parent pointer.
        Node* parent{nullptr};
    };

    [[nodiscard]] Node* findLeaf(
        const IndexKey& key
    ) noexcept;

    [[nodiscard]] const Node* findLeaf(
        const IndexKey& key
    ) const noexcept;

    [[nodiscard]] bool insertIntoLeaf(
        Node& leaf,
        const IndexKey& key,
        RecordId recordId
    );

    void splitLeaf(Node& leaf);

    void insertIntoParent(
        Node& left,
        const IndexKey& separator,
        std::unique_ptr<Node> right
    );

    void splitInternal(Node& node);

    [[nodiscard]] bool removeFromLeaf(
        Node& leaf,
        const IndexKey& key,
        RecordId recordId
    );

    void refreshSeparators(Node& node);

    [[nodiscard]] static bool entriesEqual(
        const Entry& lhs,
        const Entry& rhs
    );

    std::size_t nodeCapacity_;
    std::size_t size_{0};

    std::unique_ptr<Node> root_;
};

} // namespace forgedb