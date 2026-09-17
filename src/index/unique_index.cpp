#include "forgedb/index/unique_index.h"
#include <stdexcept>
namespace forgedb {
UniqueIndex::UniqueIndex(std::unique_ptr<Index> delegate):delegate_(std::move(delegate)){if(!delegate_)throw std::invalid_argument("UniqueIndex: delegate cannot be null");}
bool UniqueIndex::insert(const IndexKey& k,RecordId id){if(delegate_->contains(k))return false;return delegate_->insert(k,id);}
bool UniqueIndex::remove(const IndexKey& k,RecordId id){return delegate_->remove(k,id);}
std::vector<RecordId> UniqueIndex::lookup(const IndexKey& k)const{return delegate_->lookup(k);}
std::vector<RecordId> UniqueIndex::scan(const IndexKey& a,const IndexKey& b)const{return delegate_->scan(a,b);}
bool UniqueIndex::contains(const IndexKey& k)const{return delegate_->contains(k);}
std::size_t UniqueIndex::size()const noexcept{return delegate_->size();}
}
