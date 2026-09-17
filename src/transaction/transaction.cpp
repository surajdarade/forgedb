#include "forgedb/transaction/transaction.h"
#include <chrono>
#include <stdexcept>
namespace forgedb {
void Transaction::addUndo(std::function<void()> undo){std::lock_guard lock(mutex_);if(state_!=TransactionState::Active)throw std::logic_error("Transaction: not active");undo_.push_back(std::move(undo));}
void Transaction::commit(){std::lock_guard lock(mutex_);if(state_!=TransactionState::Active)throw std::logic_error("Transaction: not active");undo_.clear();state_=TransactionState::Committed;}
void Transaction::abort(){std::vector<std::function<void()>> actions;{std::lock_guard lock(mutex_);if(state_!=TransactionState::Active)return;actions.swap(undo_);state_=TransactionState::Aborted;}for(auto it=actions.rbegin();it!=actions.rend();++it)(*it)();}
WriteAheadLog::~WriteAheadLog(){delete file_;}
WriteAheadLog::WriteAheadLog(std::string path):path_(std::move(path)){if(!path_.empty()){file_=new std::ofstream(path_,std::ios::app);if(!*file_){delete file_;file_=nullptr;throw std::runtime_error("WAL: unable to open log");}}}
void WriteAheadLog::append(TransactionId id,std::string_view operation){std::lock_guard lock(mutex_);if(!file_)return;*file_<<id.value()<<'|'<<operation<<'\n';}
void WriteAheadLog::flush(){std::lock_guard lock(mutex_);if(file_)file_->flush();}
TransactionManager::TransactionManager(std::string walPath){if(!walPath.empty())wal_=std::make_unique<WriteAheadLog>(std::move(walPath));}
std::shared_ptr<Transaction> TransactionManager::begin(){auto tx=std::make_shared<Transaction>(TransactionId{nextId_.fetch_add(1)});{std::lock_guard lock(mutex_);active_[tx->id().value()]=tx;}if(wal_)wal_->append(tx->id(),"BEGIN");return tx;}
void TransactionManager::commit(const std::shared_ptr<Transaction>&tx){if(!tx)throw std::invalid_argument("TransactionManager: null transaction");tx->commit();{std::lock_guard lock(mutex_);active_.erase(tx->id().value());}if(wal_){wal_->append(tx->id(),"COMMIT");wal_->flush();}}
void TransactionManager::abort(const std::shared_ptr<Transaction>&tx){if(!tx)throw std::invalid_argument("TransactionManager: null transaction");tx->abort();{std::lock_guard lock(mutex_);active_.erase(tx->id().value());}if(wal_){wal_->append(tx->id(),"ABORT");wal_->flush();}}
}
