#include "forgedb/index/bitmap_index.h"
#include <algorithm>
#include <bit>
#include <stdexcept>
namespace forgedb {
std::uint64_t BitmapIndex::keyValue(const IndexKey& key) {
    switch (key.type()) {
    case DataType::Int32:
        return static_cast<std::uint64_t>(static_cast<std::int64_t>(std::get<std::int32_t>(key.storage()))) ^ (std::uint64_t{1} << 63);
    case DataType::Int64:
        return static_cast<std::uint64_t>(std::get<std::int64_t>(key.storage())) ^ (std::uint64_t{1} << 63);
    default:
        throw std::invalid_argument("BitmapIndex: only INT32/INT64 keys are supported");
    }
}
void BitmapIndex::setBit(Bitmap& bitmap, PageId page, std::uint32_t slot) {
    auto& words = bitmap[page.value()];
    if (words.size() <= slot / 64) words.resize(slot / 64 + 1, 0);
    words[slot / 64] |= std::uint64_t{1} << (slot % 64);
}
bool BitmapIndex::clearBit(Bitmap& bitmap, PageId page, std::uint32_t slot) {
    auto it = bitmap.find(page.value());
    if (it == bitmap.end()) return false;
    auto& words = it->second;
    const auto word = slot / 64;
    if (word >= words.size()) return false;
    words[word] &= ~(std::uint64_t{1} << (slot % 64));
    while (!words.empty() && words.back() == 0) words.pop_back();
    if (words.empty()) bitmap.erase(it);
    return true;
}
bool BitmapIndex::testBit(const Bitmap& bitmap, PageId page, std::uint32_t slot) {
    const auto it = bitmap.find(page.value());
    if (it == bitmap.end() || slot / 64 >= it->second.size()) return false;
    return (it->second[slot / 64] & (std::uint64_t{1} << (slot % 64))) != 0;
}
std::vector<RecordId> BitmapIndex::decode(const Bitmap& bitmap) {
    std::vector<RecordId> result;
    for (const auto& [page, words] : bitmap) {
        for (std::size_t word = 0; word < words.size(); ++word) {
            auto bits = words[word];
            while (bits != 0) {
                const auto bit = std::countr_zero(bits);
                result.emplace_back(PageId{page}, static_cast<std::uint32_t>(word * std::size_t{64} + static_cast<std::size_t>(bit)));
                bits &= bits - 1;
            }
        }
    }
    return result;
}
bool BitmapIndex::insert(const IndexKey& key, RecordId id) {
    auto& bitmap = values_[keyValue(key)];
    if (testBit(bitmap, id.pageId(), id.slot())) return false;
    setBit(bitmap, id.pageId(), id.slot());
    ++size_;
    return true;
}
bool BitmapIndex::remove(const IndexKey& key, RecordId id) {
    auto it = values_.find(keyValue(key));
    if (it == values_.end() || !testBit(it->second, id.pageId(), id.slot())) return false;
    clearBit(it->second, id.pageId(), id.slot());
    if (it->second.empty()) values_.erase(it);
    --size_;
    return true;
}
std::vector<RecordId> BitmapIndex::lookup(const IndexKey& key) const {
    auto it = values_.find(keyValue(key));
    if (it == values_.end()) return {};
    return decode(it->second);
}
std::vector<RecordId> BitmapIndex::scan(const IndexKey& lower, const IndexKey& upper) const {
    const auto lo = keyValue(lower), hi = keyValue(upper);
    if (hi < lo) throw std::invalid_argument("BitmapIndex: invalid range");
    std::vector<RecordId> result;
    for (auto it = values_.lower_bound(lo); it != values_.end() && it->first <= hi; ++it) {
        auto ids = decode(it->second);
        result.insert(result.end(), ids.begin(), ids.end());
    }
    std::sort(result.begin(), result.end());
    return result;
}
bool BitmapIndex::contains(const IndexKey& key) const {
    auto it = values_.find(keyValue(key));
    return it != values_.end() && !it->second.empty();
}
std::size_t BitmapIndex::size() const noexcept { return size_; }
} // namespace forgedb
