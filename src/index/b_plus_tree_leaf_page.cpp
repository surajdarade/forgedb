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


int comparePageKeys(const IndexKey& lhs, const IndexKey& rhs) {
    if (lhs.type() != rhs.type()) {
        return static_cast<int>(lhs.type()) < static_cast<int>(rhs.type()) ? -1 : 1;
    }
    if (lhs == rhs) return 0;
    return lhs < rhs ? -1 : 1;
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
                return comparePageKeys(entry.key, value) < 0;
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
                return comparePageKeys(value, entry.key) < 0;
            }
        ) - entries.begin()
    );
}

std::vector<RecordId> BPlusTreeLeafPage::lookup(
    const IndexKey& key,
    bool* continueToNextPage) const
{
    std::vector<RecordId> result;
    if (continueToNextPage) *continueToNextPage = true;

    // Leaf entries are stored in sorted order as a compact serialized stream.
    // Decode only as far as needed instead of allocating and decoding a vector
    // of every entry for each point lookup. If a greater key is encountered,
    // sorted order proves that neither this leaf nor later leaves can contain
    // another match.
    const auto data = std::span<const std::uint8_t>(
        page_.data().data(), page_.data().size());
    std::size_t offset = kHeaderSize;

    for (std::size_t i = 0; i < size(); ++i) {
        const IndexKey currentKey = deserializeKey(data, offset);
        const PageId pageId{Serializer::readUInt64(data, offset)};
        const std::uint32_t slot = Serializer::readUInt32(data, offset);
        const int comparison = comparePageKeys(currentKey, key);

        if (comparison > 0) {
            if (continueToNextPage) *continueToNextPage = false;
            return result;
        }
        if (comparison == 0) {
            result.emplace_back(RecordId{pageId, slot});
        }
    }

    // All entries in this leaf are <= key. The next leaf may contain the key
    // (including duplicate keys spanning a split), or the target if routing
    // lands at the end of the current key range.
    return result;
}

std::vector<RecordId> BPlusTreeLeafPage::scanRange(
    const IndexKey& lower,
    const IndexKey& upper,
    bool* continueToNextPage) const
{
    std::vector<RecordId> result;
    if (continueToNextPage) *continueToNextPage = true;

    // Walk the serialized payload once. Calling lowerBound() here would first
    // decode the entire leaf into a temporary vector; calling keyAt() in a
    // loop would repeatedly rescan from the beginning of the page.
    const auto data = std::span<const std::uint8_t>(
        page_.data().data(), page_.data().size());
    std::size_t offset = kHeaderSize;

    for (std::size_t i = 0; i < size(); ++i) {
        const IndexKey currentKey = deserializeKey(data, offset);
        const PageId recordPageId{Serializer::readUInt64(data, offset)};
        const std::uint32_t recordSlot = Serializer::readUInt32(data, offset);

        if (comparePageKeys(currentKey, lower) < 0) {
            continue;
        }
        if (comparePageKeys(upper, currentKey) < 0) {
            if (continueToNextPage) *continueToNextPage = false;
            break;
        }
        result.emplace_back(recordPageId, recordSlot);
    }
    return result;
}

BPlusTreeLeafPage::Entry BPlusTreeLeafPage::entryAt(std::size_t index) const {
    return readEntry(index);
}

std::vector<BPlusTreeLeafPage::Entry> BPlusTreeLeafPage::entries() const {
    return readEntries();
}

BPlusTreeLeafPage::Entry BPlusTreeLeafPage::lastEntry() const {
    if (isEmpty()) {
        throw std::out_of_range("BPlusTreeLeafPage: empty leaf has no last entry");
    }
    return lastEntryAndEndOffset().first;
}

void BPlusTreeLeafPage::rewrite(const std::vector<Entry>& entries) {
    rewriteEntries(entries);
}

std::size_t BPlusTreeLeafPage::appendAtEnd(
    const IndexKey& key,
    RecordId recordId,
    std::size_t endOffset)
{
    const Entry entry{key, recordId};
    const std::size_t required = serializedEntrySize(entry);
    if (endOffset < kHeaderSize || endOffset > kPageSize ||
        required > kPageSize - endOffset) {
        throw std::overflow_error(
            "BPlusTreeLeafPage: insufficient free space");
    }

    writeEntry(endOffset, entry);
    setEntryCount(size() + 1);
    return endOffset + required;
}

std::size_t BPlusTreeLeafPage::serializedEndOffset() const {
    return isEmpty() ? kHeaderSize : lastEntryAndEndOffset().second;
}

bool BPlusTreeLeafPage::insert(
    const IndexKey& key,
    RecordId recordId)
{
    Entry newEntry{
        key,
        recordId
    };

    // The common append-heavy workload can update the page in place. Scan the
    // serialized payload once, without allocating/deserializing a vector of
    // every entry, to locate the tail and validate ordering.
    if (!isEmpty()) {
        const auto [last, endOffset] = lastEntryAndEndOffset();
        if (entryEqual(last, newEntry)) {
            return false;
        }
        if (entryLess(last, newEntry)) {
            const std::size_t required = serializedEntrySize(newEntry);
            if (required > kPageSize - endOffset) {
                throw std::overflow_error(
                    "BPlusTreeLeafPage: insufficient free space"
                );
            }
            writeEntry(endOffset, newEntry);
            setEntryCount(size() + 1);
            return true;
        }
    }

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

std::pair<BPlusTreeLeafPage::Entry, std::size_t>
BPlusTreeLeafPage::lastEntryAndEndOffset() const
{
    if (isEmpty()) {
        throw std::out_of_range("BPlusTreeLeafPage: empty leaf has no last entry");
    }

    const auto data = std::span<const std::uint8_t>(
        page_.data().data(), page_.data().size());
    std::size_t offset = kHeaderSize;
    std::size_t lastEntryOffset = offset;

    for (std::size_t i = 0; i < size(); ++i) {
        lastEntryOffset = offset;
        if (offset >= data.size()) {
            throw std::runtime_error("BPlusTreeLeafPage: corrupt entry payload");
        }

        const auto tag = data[offset++];
        switch (tag) {
        case 0: // Int32
        case 2: // Float
            offset += sizeof(std::uint32_t);
            break;
        case 1: // Int64
        case 3: // Double
            offset += sizeof(std::uint64_t);
            break;
        case 4: { // Varchar: uint32 length followed by bytes
            if (offset + sizeof(std::uint32_t) > data.size()) {
                throw std::runtime_error("BPlusTreeLeafPage: corrupt varchar key");
            }
            std::uint32_t length = 0;
            for (std::size_t byte = 0; byte < sizeof(length); ++byte) {
                length |= static_cast<std::uint32_t>(data[offset + byte]) << (byte * 8);
            }
            offset += sizeof(length) + static_cast<std::size_t>(length);
            break;
        }
        default:
            throw std::runtime_error("BPlusTreeLeafPage: invalid key type tag");
        }
        offset += sizeof(std::uint64_t) + sizeof(std::uint32_t);
        if (offset > data.size()) {
            throw std::runtime_error("BPlusTreeLeafPage: corrupt entry payload");
        }
    }

    std::size_t entryOffset = lastEntryOffset;
    Entry last{
        deserializeKey(data, entryOffset),
        RecordId{
            PageId{Serializer::readUInt64(data, entryOffset)},
            Serializer::readUInt32(data, entryOffset)
        }
    };
    return {std::move(last), offset};
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
        Page::Byte{0}
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
        return comparePageKeys(lhs.key, rhs.key) < 0;
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