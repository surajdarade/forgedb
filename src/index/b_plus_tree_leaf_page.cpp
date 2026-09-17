#include "forgedb/index/b_plus_tree_leaf_page.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

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
        break;
    }

    throw std::invalid_argument(
        "BPlusTreeLeafPage: BOOLEAN is not supported"
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
            std::get<std::int32_t>(key.storage())
        );
        break;

    case KeyTypeTag::Int64:
        Serializer::writeInt64(
            buffer,
            std::get<std::int64_t>(key.storage())
        );
        break;

    case KeyTypeTag::Float:
        Serializer::writeFloat(
            buffer,
            std::get<float>(key.storage())
        );
        break;

    case KeyTypeTag::Double:
        Serializer::writeDouble(
            buffer,
            std::get<double>(key.storage())
        );
        break;

    case KeyTypeTag::Varchar:
        Serializer::writeString(
            buffer,
            std::get<std::string>(key.storage())
        );
        break;
    }
}

IndexKey deserializeKey(
    std::span<const std::uint8_t> data,
    std::size_t& offset)
{
    const auto tag = static_cast<KeyTypeTag>(
        Serializer::readUInt8(data, offset)
    );

    switch (tag) {
    case KeyTypeTag::Int32:
        return IndexKey{
            Serializer::readInt32(data, offset)
        };

    case KeyTypeTag::Int64:
        return IndexKey{
            Serializer::readInt64(data, offset)
        };

    case KeyTypeTag::Float:
        return IndexKey{
            Serializer::readFloat(data, offset)
        };

    case KeyTypeTag::Double:
        return IndexKey{
            Serializer::readDouble(data, offset)
        };

    case KeyTypeTag::Varchar:
        return IndexKey{
            Serializer::readString(data, offset)
        };

    default:
        throw std::runtime_error(
            "BPlusTreeLeafPage: invalid key type"
        );
    }
}

} // namespace

BPlusTreeLeafPage::BPlusTreeLeafPage(Page& page)
    : IndexPage(page)
{
    if (pageType() != IndexPageType::Leaf) {
        setPageType(IndexPageType::Leaf);
        setParentPageId(PageId{});
        setEntryCount(0);
        setNextPageId(PageId{});
    }
}

std::size_t BPlusTreeLeafPage::size() const noexcept
{
    return entryCount();
}

bool BPlusTreeLeafPage::isEmpty() const noexcept
{
    return size() == 0;
}

bool BPlusTreeLeafPage::isFull() const noexcept
{
    return freeSpace() == 0;
}

bool BPlusTreeLeafPage::isUnderflow(
    std::size_t minimumEntries) const noexcept
{
    return size() < minimumEntries;
}

PageId BPlusTreeLeafPage::nextPageId() const noexcept
{
    return PageId{
        readUInt64(kNextPageIdOffset)
    };
}

void BPlusTreeLeafPage::setNextPageId(
    PageId pageId) noexcept
{
    writeUInt64(
        kNextPageIdOffset,
        pageId.value()
    );
}

IndexKey BPlusTreeLeafPage::keyAt(
    std::size_t index) const
{
    if (index >= size()) {
        throw std::out_of_range(
            "BPlusTreeLeafPage: entry index out of range"
        );
    }

    return readEntry(index).key;
}

RecordId BPlusTreeLeafPage::recordIdAt(
    std::size_t index) const
{
    if (index >= size()) {
        throw std::out_of_range(
            "BPlusTreeLeafPage: entry index out of range"
        );
    }

    return readEntry(index).recordId;
}

std::size_t BPlusTreeLeafPage::lowerBound(
    const IndexKey& key) const
{
    const auto entries = readEntries();

    return static_cast<std::size_t>(
        std::lower_bound(
            entries.begin(),
            entries.end(),
            key,
            [](const Entry& entry,
               const IndexKey& value) {
                return entry.key < value;
            }
        ) - entries.begin()
    );
}

std::size_t BPlusTreeLeafPage::upperBound(
    const IndexKey& key) const
{
    const auto entries = readEntries();

    return static_cast<std::size_t>(
        std::upper_bound(
            entries.begin(),
            entries.end(),
            key,
            [](const IndexKey& value,
               const Entry& entry) {
                return value < entry.key;
            }
        ) - entries.begin()
    );
}

std::vector<RecordId> BPlusTreeLeafPage::lookup(
    const IndexKey& key) const
{
    const auto entries = readEntries();

    std::vector<RecordId> result;

    const auto iterator =
        std::lower_bound(
            entries.begin(),
            entries.end(),
            key,
            [](const Entry& entry,
               const IndexKey& value) {
                return entry.key < value;
            }
        );

    for (auto current = iterator;
         current != entries.end();
         ++current) {

        if (current->key == key) {
            result.push_back(current->recordId);
            continue;
        }

        if (key < current->key) {
            break;
        }
    }

    return result;
}

bool BPlusTreeLeafPage::insert(
    const IndexKey& key,
    RecordId recordId)
{
    Entry newEntry{
        key,
        recordId
    };

    auto entries = readEntries();

    const auto iterator =
        std::lower_bound(
            entries.begin(),
            entries.end(),
            newEntry,
            entryLess
        );

    if (iterator != entries.end() &&
        entryEqual(*iterator, newEntry)) {
        return false;
    }

    const std::size_t required =
        serializedEntrySize(newEntry);

    if (required > freeSpace()) {
        throw std::overflow_error(
            "BPlusTreeLeafPage: insufficient free space"
        );
    }

    entries.insert(
        iterator,
        std::move(newEntry)
    );

    rewriteEntries(entries);

    return true;
}

bool BPlusTreeLeafPage::remove(
    const IndexKey& key,
    RecordId recordId)
{
    Entry target{
        key,
        recordId
    };

    auto entries = readEntries();

    const auto iterator =
        std::lower_bound(
            entries.begin(),
            entries.end(),
            target,
            entryLess
        );

    if (iterator == entries.end() ||
        !entryEqual(*iterator, target)) {
        return false;
    }

    entries.erase(iterator);

    rewriteEntries(entries);

    return true;
}

std::size_t BPlusTreeLeafPage::freeSpace() const noexcept
{
    std::size_t used = kHeaderSize;

    const auto entries = readEntries();

    for (const Entry& entry : entries) {
        used += serializedEntrySize(entry);
    }

    return kPageSize - used;
}

std::size_t BPlusTreeLeafPage::entrySize(
    const Entry& entry) const
{
    return serializedEntrySize(entry);
}

BPlusTreeLeafPage::Entry
BPlusTreeLeafPage::readEntry(
    std::size_t index) const
{
    if (index >= size()) {
        throw std::out_of_range(
            "BPlusTreeLeafPage: entry index out of range"
        );
    }

    const auto entries = readEntries();

    return entries[index];
}

void BPlusTreeLeafPage::writeEntry(
    std::size_t offset,
    const Entry& entry)
{
    std::vector<std::uint8_t> buffer;

    serializeKey(
        buffer,
        entry.key
    );

    Serializer::writeUInt64(
        buffer,
        entry.recordId.pageId().value()
    );

    Serializer::writeUInt32(
        buffer,
        entry.recordId.slot()
    );

    if (offset + buffer.size() > kPageSize) {
        throw std::overflow_error(
            "BPlusTreeLeafPage: entry exceeds page size"
        );
    }

    std::memcpy(
        page_.data().data() + offset,
        buffer.data(),
        buffer.size()
    );
}

std::vector<BPlusTreeLeafPage::Entry>
BPlusTreeLeafPage::readEntries() const
{
    std::vector<Entry> entries;
    entries.reserve(size());

    std::size_t offset = kHeaderSize;

    const auto data =
        std::span<const std::uint8_t>(
            page_.data().data(),
            page_.data().size()
        );

    for (std::size_t i = 0; i < size(); ++i) {
        const IndexKey key =
            deserializeKey(data, offset);

        const PageId pageId{
            Serializer::readUInt64(
                data,
                offset
            )
        };

        const std::uint32_t slot =
            Serializer::readUInt32(
                data,
                offset
            );

        entries.push_back(
            Entry{
                key,
                RecordId{
                    pageId,
                    slot
                }
            }
        );
    }

    return entries;
}

void BPlusTreeLeafPage::rewriteEntries(
    const std::vector<Entry>& entries)
{
    std::size_t required = kHeaderSize;

    for (const Entry& entry : entries) {
        required += serializedEntrySize(entry);
    }

    if (required > kPageSize) {
        throw std::overflow_error(
            "BPlusTreeLeafPage: entries exceed page capacity"
        );
    }

    std::fill(
        page_.data().begin() + kHeaderSize,
        page_.data().end(),
        0
    );

    std::size_t offset = kHeaderSize;

    for (const Entry& entry : entries) {
        writeEntry(offset, entry);
        offset += serializedEntrySize(entry);
    }

    setEntryCount(entries.size());
}

bool BPlusTreeLeafPage::entryLess(
    const Entry& lhs,
    const Entry& rhs)
{
    if (lhs.key != rhs.key) {
        return lhs.key < rhs.key;
    }

    return lhs.recordId < rhs.recordId;
}

bool BPlusTreeLeafPage::entryEqual(
    const Entry& lhs,
    const Entry& rhs)
{
    return lhs.key == rhs.key &&
           lhs.recordId == rhs.recordId;
}

std::size_t BPlusTreeLeafPage::serializedKeySize(
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
            "BPlusTreeLeafPage: BOOLEAN is not supported"
        );
    }

    throw std::logic_error(
        "BPlusTreeLeafPage: unknown key type"
    );
}

std::size_t BPlusTreeLeafPage::serializedEntrySize(
    const Entry& entry)
{
    return serializedKeySize(entry.key) +
           sizeof(std::uint64_t) +
           sizeof(std::uint32_t);
}

} // namespace forgedb