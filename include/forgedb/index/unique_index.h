#pragma once
#include <memory>
#include "forgedb/index/index.h"
namespace forgedb {
class UniqueIndex final : public Index {
public:
    explicit UniqueIndex(std::unique_ptr<Index> delegate);
    bool insert(const IndexKey&, RecordId) override;
    bool remove(const IndexKey&, RecordId) override;
    std::vector<RecordId> lookup(const IndexKey&) const override;
    std::vector<RecordId> scan(const IndexKey&, const IndexKey&) const override;
    bool contains(const IndexKey&) const override;
    std::size_t size() const noexcept override;
private:
    std::unique_ptr<Index> delegate_;
};
}
