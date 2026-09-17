#pragma once
#include <list>
#include <memory>
#include <unordered_map>
#include "forgedb/index/index.h"
namespace forgedb {
class CachedIndex final : public Index {
public:
    CachedIndex(std::unique_ptr<Index> delegate, std::size_t capacity = 128);
    bool insert(const IndexKey&, RecordId) override;
    bool remove(const IndexKey&, RecordId) override;
    std::vector<RecordId> lookup(const IndexKey&) const override;
    std::vector<RecordId> scan(const IndexKey&, const IndexKey&) const override;
    bool contains(const IndexKey&) const override;
    std::size_t size() const noexcept override;
private:
    struct Entry { IndexKey key; std::vector<RecordId> values; };
    void touch(std::uint64_t hash) const;
    static std::uint64_t hashKey(const IndexKey&);
    std::unique_ptr<Index> delegate_;
    std::size_t capacity_;
    mutable std::list<std::uint64_t> lru_;
    mutable std::unordered_map<std::uint64_t,std::pair<std::vector<RecordId>,std::list<std::uint64_t>::iterator>> cache_;
};
}
