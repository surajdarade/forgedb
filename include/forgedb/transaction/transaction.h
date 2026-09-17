#pragma once
#include <cstdint>
#include <functional>
#include <atomic>
#include <memory>
#include <unordered_map>
#include <fstream>
#include <string_view>
#include <mutex>
#include <string>
#include <vector>
namespace forgedb {
class TransactionId { public: constexpr TransactionId() noexcept=default; explicit constexpr TransactionId(std::uint64_t v)noexcept:value_(v){} constexpr std::uint64_t value()const noexcept{return value_;} friend constexpr bool operator==(TransactionId a,TransactionId b)noexcept{return a.value_==b.value_;} private:std::uint64_t value_{0}; };
enum class TransactionState { Active, Committed, Aborted };
class Transaction {
public:
    explicit Transaction(TransactionId id):id_(id){}
    TransactionId id()const noexcept{return id_;}
    TransactionState state()const noexcept{return state_;}
    void addUndo(std::function<void()> undo);
    void commit();
    void abort();
private:
    TransactionId id_; TransactionState state_{TransactionState::Active}; std::vector<std::function<void()>> undo_; std::mutex mutex_;
};
class WriteAheadLog {
public:
    explicit WriteAheadLog(std::string path);
    ~WriteAheadLog();
    void append(TransactionId id,std::string_view operation);
    void flush();
private: std::mutex mutex_; std::string path_; std::ofstream* file_{nullptr};
};
class TransactionManager {
public:
    explicit TransactionManager(std::string walPath = {});
    std::shared_ptr<Transaction> begin();
    void commit(const std::shared_ptr<Transaction>&);
    void abort(const std::shared_ptr<Transaction>&);
private:
    std::atomic<std::uint64_t> nextId_{1};
    std::mutex mutex_;
    std::unordered_map<std::uint64_t,std::shared_ptr<Transaction>> active_;
    std::unique_ptr<WriteAheadLog> wal_;
};
}
