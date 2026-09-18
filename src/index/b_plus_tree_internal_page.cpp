#include "forgedb/index/b_plus_tree_internal_page.h"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#include "forgedb/storage/serializer.h"

namespace forgedb {

namespace {

enum class KeyTypeTag : std::uint8_t {
    Int32 = 0,
    Int64 = 1,
    Float = 2,
    Double = 3,
    Varchar = 4
};

KeyTypeTag keyTypeTag(const IndexKey& key)
{
    switch (key.type()) {
    case DataType::Int32:
        return KeyTypeTag::Int32;

    case DataType::Int64:
        return KeyTypeTag::Int64;

    case DataType::Float:
        return KeyTypeTag::Float;

    case DataType::Double:
        return KeyTypeTag::Double;

    case DataType::Varchar:
        return KeyTypeTag::Varchar;

    case DataType::Boolean:
        throw std::invalid_argument(
            "BPlusTreeInternalPage: BOOLEAN is not supported"
        );
    }

    throw std::logic_error(
        "BPlusTreeInternalPage: unknown key type"
    );
}

void serializeKey(
    std::vector<std::uint8_t>& buffer,
    const IndexKey& key)
{
    const auto tag = keyTypeTag(key);

    Serializer::writeUInt8(
        buffer,
        static_cast<std::uint8_t>(tag)
    );

    switch (tag) {
    case KeyTypeTag::Int32:
        Serializer::writeInt32(
            buffer,
            std::get<std::int32_t>(
                key.storage()
            )
        );
        break;

    case KeyTypeTag::Int64:
        Serializer::writeInt64(
            buffer,
            std::get<std::int64_t>(
                key.storage()
            )
        );
        break;

    case KeyTypeTag::Float:
        Serializer::writeFloat(
            buffer,
            std::get<float>(
                key.storage()
            )
        );
        break;

    case KeyTypeTag::Double:
        Serializer::writeDouble(
            buffer,
            std::get<double>(
                key.storage()
            )
        );
        break;

    case KeyTypeTag::Varchar:
        Serializer::writeString(
            buffer,
            std::get<std::string>(
                key.storage()
            )
        );
        break;
    }
}

IndexKey deserializeKey(
    std::span<const std::uint8_t> data,
    std::size_t& offset)
{
    const auto tag = static_cast<KeyTypeTag>(
        Serializer::readUInt8(
            data,
            offset
        )
    );

    switch (tag) {
    case KeyTypeTag::Int32:
        return IndexKey{
            Serializer::readInt32(
                data,
                offset
            )
        };

    case KeyTypeTag::Int64:
        return IndexKey{
            Serializer::readInt64(
                data,
                offset
            )
        };

    case KeyTypeTag::Float:
        return IndexKey{
            Serializer::readFloat(
                data,
                offset
            )
        };

    case KeyTypeTag::Double:
        return IndexKey{
            Serializer::readDouble(
                data,
                offset
            )
        };

    case KeyTypeTag::Varchar:
        return IndexKey{
            Serializer::readString(
                data,
                offset
            )
        };

    default:
        throw std::runtime_error(
            "BPlusTreeInternalPage: invalid key type"
        );
    }
}

} // namespace

BPlusTreeInternalPage::BPlusTreeInternalPage(Page& page)
    : IndexPage(page)
{
    if (pageType() != IndexPageType::Internal) {
        setPageType(IndexPageType::Internal);
        setParentPageId(PageId{});
        setEntryCount(0);
    }
}

std::size_t BPlusTreeInternalPage::size() const noexcept
{
    return entryCount();
}

std::size_t BPlusTreeInternalPage::childCount() const noexcept
{
    return size() + 1;
}

IndexKey BPlusTreeInternalPage::keyAt(
    std::size_t index) const
{
    if (index >= size()) {
        throw std::out_of_range(
            "BPlusTreeInternalPage: key index out of range"
        );
    }

    const auto keys = readKeys();

    return keys[index];
}

PageId BPlusTreeInternalPage::childAt(
    std::size_t index) const
{
    if (index >= childCount()) {
        throw std::out_of_range(
            "BPlusTreeInternalPage: child index out of range"
        );
    }

    const auto children = readChildren();

    return children[index];
}

void BPlusTreeInternalPage::setKey(
    std::size_t index,
    const IndexKey& key)
{
    if (index >= size()) {
        throw std::out_of_range(
            "BPlusTreeInternalPage: key index out of range"
        );
    }

    auto keys = readKeys();
    auto children = readChildren();

    keys[index] = key;

    rewrite(keys, children);
}

void BPlusTreeInternalPage::setChildAt(
    std::size_t index,
    PageId childPageId)
{
    if (index >= childCount()) {
        throw std::out_of_range(
            "BPlusTreeInternalPage: child index out of range"
        );
    }

    auto keys = readKeys();
    auto children = readChildren();

    children[index] = childPageId;

    rewrite(keys, children);
}

void BPlusTreeInternalPage::setFirstChild(
    PageId childPageId)
{
    setChildAt(0, childPageId);
}

PageId BPlusTreeInternalPage::lookupChild(
    const IndexKey& key) const
{
    const auto keys = readKeys();
    const auto children = readChildren();

    if (children.empty()) {
        throw std::logic_error(
            "BPlusTreeInternalPage: internal page has no children"
        );
    }

    const auto iterator =
        std::upper_bound(
            keys.begin(),
            keys.end(),
            key
        );

    const std::size_t childIndex =
        static_cast<std::size_t>(
            iterator - keys.begin()
        );

    return children[childIndex];
}

void BPlusTreeInternalPage::insertChild(
    std::size_t childIndex,
    const IndexKey& separator,
    PageId childPageId)
{
    if (childIndex == 0 ||
        childIndex > childCount()) {
        throw std::out_of_range(
            "BPlusTreeInternalPage: invalid child insertion index"
        );
    }

    auto keys = readKeys();
    auto children = readChildren();

    keys.insert(
        keys.begin() +
        static_cast<std::ptrdiff_t>(
            childIndex - 1
        ),
        separator
    );

    children.insert(
        children.begin() +
        static_cast<std::ptrdiff_t>(
            childIndex
        ),
        childPageId
    );

    rewrite(keys, children);
}

void BPlusTreeInternalPage::removeChild(
    std::size_t childIndex)
{
    if (childIndex >= childCount()) {
        throw std::out_of_range(
            "BPlusTreeInternalPage: child index out of range"
        );
    }

    if (childCount() <= 1) {
        throw std::logic_error(
            "BPlusTreeInternalPage: cannot remove final child"
        );
    }

    auto keys = readKeys();
    auto children = readChildren();

    if (childIndex == 0) {
        children.erase(children.begin());
        keys.erase(keys.begin());
    }
    else {
        children.erase(
            children.begin() +
            static_cast<std::ptrdiff_t>(
                childIndex
            )
        );

        keys.erase(
            keys.begin() +
            static_cast<std::ptrdiff_t>(
                childIndex - 1
            )
        );
    }

    rewrite(keys, children);
}

std::size_t BPlusTreeInternalPage::freeSpace() const noexcept
{
    const auto keys = readKeys();

    std::size_t used = kPayloadOffset;

    for (const IndexKey& key : keys) {
        used += serializedKeySize(key);
        used += sizeof(std::uint64_t);
    }

    if (used >= kPageSize) {
        return 0;
    }

    return kPageSize - used;
}

std::vector<IndexKey>
BPlusTreeInternalPage::readKeys() const
{
    std::vector<IndexKey> keys;
    keys.reserve(size());

    std::size_t offset = kPayloadOffset;

    const auto data =
        std::span<const std::uint8_t>(
            page_.data().data(),
            page_.data().size()
        );

    // Skip Child0.
    Serializer::readUInt64(data, offset);

    for (std::size_t i = 0; i < size(); ++i) {
        keys.push_back(
            deserializeKey(data, offset)
        );

        // Skip Child(i + 1).
        Serializer::readUInt64(data, offset);
    }

    return keys;
}

std::vector<PageId>
BPlusTreeInternalPage::readChildren() const
{
    std::vector<PageId> children;
    children.reserve(childCount());

    std::size_t offset = kPayloadOffset;

    const auto data =
        std::span<const std::uint8_t>(
            page_.data().data(),
            page_.data().size()
        );

    for (std::size_t i = 0; i < childCount(); ++i) {
        children.push_back(
            PageId{
                Serializer::readUInt64(
                    data,
                    offset
                )
            }
        );

        if (i < size()) {
            deserializeKey(data, offset);
        }
    }

    return children;
}

void BPlusTreeInternalPage::rewrite(
    const std::vector<IndexKey>& keys,
    const std::vector<PageId>& children)
{
    if (children.size() != keys.size() + 1) {
        throw std::logic_error(
            "BPlusTreeInternalPage: invalid key/child count"
        );
    }

    std::vector<std::uint8_t> buffer;

    buffer.reserve(kPageSize - kPayloadOffset);

    // First child.
    Serializer::writeUInt64(
        buffer,
        children.front().value()
    );

    for (std::size_t i = 0; i < keys.size(); ++i) {
        serializeKey(
            buffer,
            keys[i]
        );

        Serializer::writeUInt64(
            buffer,
            children[i + 1].value()
        );
    }

    if (kPayloadOffset + buffer.size() > kPageSize) {
        throw std::overflow_error(
            "BPlusTreeInternalPage: insufficient page space"
        );
    }

    std::fill(
        page_.data().begin() + kPayloadOffset,
        page_.data().end(),
        Page::Byte{0}
    );

    std::copy(
        buffer.begin(),
        buffer.end(),
        page_.data().begin() + kPayloadOffset
    );

    setEntryCount(keys.size());
}

std::vector<IndexKey> BPlusTreeInternalPage::keys() const {
    return readKeys();
}

std::vector<PageId> BPlusTreeInternalPage::children() const {
    return readChildren();
}

std::size_t BPlusTreeInternalPage::serializedKeySize(
    const IndexKey& key)
{
    switch (key.type()) {
    case DataType::Int32:
        return sizeof(std::uint8_t) +
               sizeof(std::int32_t);

    case DataType::Int64:
        return sizeof(std::uint8_t) +
               sizeof(std::int64_t);

    case DataType::Float:
        return sizeof(std::uint8_t) +
               sizeof(float);

    case DataType::Double:
        return sizeof(std::uint8_t) +
               sizeof(double);

    case DataType::Varchar:
        return sizeof(std::uint8_t) +
               sizeof(std::uint32_t) +
               std::get<std::string>(
                   key.storage()
               ).size();

    case DataType::Boolean:
        throw std::invalid_argument(
            "BPlusTreeInternalPage: BOOLEAN is not supported"
        );
    }

    throw std::logic_error(
        "BPlusTreeInternalPage: unknown key type"
    );
}

std::size_t BPlusTreeInternalPage::serializedSize(
    const std::vector<IndexKey>& keys)
{
    std::size_t size = kPayloadOffset;

    for (const IndexKey& key : keys) {
        size += serializedKeySize(key);
        size += sizeof(std::uint64_t);
    }

    return size;
}

} // namespace forgedb