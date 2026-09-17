#include "forgedb/index/cached_index.h"
#include <functional>
#include <stdexcept>
namespace forgedb {
CachedIndex::CachedIndex(std::unique_ptr<Index> d,std::size_t c):delegate_(std::move(d)),capacity_(c){if(!delegate_||c==0)throw std::invalid_argument("CachedIndex: invalid configuration");}
std::uint64_t CachedIndex::hashKey(const IndexKey& k){std::size_t h=static_cast<std::size_t>(k.type());std::visit([&](const auto& v){h^=std::hash<std::decay_t<decltype(v)>>{}(v)+0x9e3779b9+(h<<6)+(h>>2);},k.storage());return static_cast<std::uint64_t>(h);}
void CachedIndex::touch(std::uint64_t h) const {auto it=cache_.find(h);if(it==cache_.end())return;lru_.erase(it->second.second);lru_.push_back(h);it->second.second=std::prev(lru_.end());}
bool CachedIndex::insert(const IndexKey& k,RecordId id){auto ok=delegate_->insert(k,id);if(ok)cache_.erase(hashKey(k));return ok;}
bool CachedIndex::remove(const IndexKey& k,RecordId id){auto ok=delegate_->remove(k,id);if(ok)cache_.erase(hashKey(k));return ok;}
std::vector<RecordId> CachedIndex::lookup(const IndexKey& k)const{auto h=hashKey(k);auto it=cache_.find(h);if(it!=cache_.end()){touch(h);return it->second.first;}auto values=delegate_->lookup(k);if(cache_.size()>=capacity_){auto old=lru_.front();lru_.pop_front();cache_.erase(old);}lru_.push_back(h);cache_[h]={values,std::prev(lru_.end())};return values;}
std::vector<RecordId> CachedIndex::scan(const IndexKey& a,const IndexKey& b)const{return delegate_->scan(a,b);}
bool CachedIndex::contains(const IndexKey& k)const{return !lookup(k).empty();}
std::size_t CachedIndex::size()const noexcept{return delegate_->size();}
}
