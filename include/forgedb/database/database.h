#pragma once
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include <utility>
#include "forgedb/buffer/buffer_pool_manager.h"
#include "forgedb/concurrency/read_write_lock.h"
#include "forgedb/logging/logger.h"
#include "forgedb/query/query.h"
#include "forgedb/record/table.h"
#include "forgedb/storage/disk_manager.h"
#include "forgedb/storage/write_queue.h"
#include "forgedb/transaction/transaction.h"
namespace forgedb {
class Database {
public:
    static std::unique_ptr<Database> open(const std::string& path,std::size_t bufferPoolSize=32,std::string logPath={});
    ~Database();
    Database(const Database&)=delete; Database& operator=(const Database&)=delete;
    Table& createTable(std::string name,Schema schema);
    Table& openTable(const std::string& name);
    bool hasTable(const std::string& name) const;
    void dropTable(const std::string& name);
    std::vector<std::string> tableNames() const;
    std::vector<Tuple> select(const std::string& tableName,const Query& query);
    TransactionManager& transactions() noexcept { return transactionManager_; }
    Logger& logger() noexcept { return logger_; }
    WriteQueue& writeQueue() noexcept { return writeQueue_; }
    void checkpoint();
    void close();
private:
    Database(std::string path,std::size_t bufferPoolSize,std::string logPath);
    struct TableEntry { TableEntry(std::string n, Schema s, PageId m):name(std::move(n)),schema(std::move(s)),metadataPage(m){} std::string name; Schema schema; std::unique_ptr<HeapFile> heap; std::unique_ptr<Table> table; PageId metadataPage; };
    void loadCatalog();
    void persistCatalog();
    std::string path_;
    std::unique_ptr<DiskManager> diskManager_;
    std::unique_ptr<BufferPoolManager> bufferPoolManager_;
    std::unordered_map<std::string,std::unique_ptr<TableEntry>> tables_;
    mutable std::mutex mutex_;
    ReadWriteLock catalogLock_;
    WriteQueue writeQueue_;
    TransactionManager transactionManager_;
    Logger logger_;
    bool closed_{false};
};
}
