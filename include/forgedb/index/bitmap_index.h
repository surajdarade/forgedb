#pragma once
#include <cstddef>
#include <cstdint>
#include <map>
#include <unordered_map>
#include <vector>
#include "forgedb/index/index.h"
namespace forgedb {
// Sparse bitmap keyed by 64-bit index value; each bitmap stores 64-bit page ids
// and 32-bit slots without truncating the page identifier.
class BitmapIndex final : public Index {
public:
    BitmapIndex() = default;
    bool insert(const IndexKey&, RecordId) override;
    bool remove(const IndexKey&, RecordId) override;
    std::vector<RecordId> lookup(const IndexKey&) const override;
    std::vector<RecordId> scan(const IndexKey&, const IndexKey&) const override;
    bool contains(const IndexKey&) const override;
    std::size_t size() const noexcept override;
private:
    using Bitmap = std::map<std::uint64_t, std::vector<std::uint64_t>>;
    std::map<std::uint64_t, Bitmap> values_;
    std::size_t size_{0};
    static std::uint64_t keyValue(const IndexKey&);
    static void setBit(Bitmap&, PageId, std::uint32_t);
    static bool clearBit(Bitmap&, PageId, std::uint32_t);
    static bool testBit(const Bitmap&, PageId, std::uint32_t);
    static std::vector<RecordId> decode(const Bitmap&);
};
} // namespace forgedb
