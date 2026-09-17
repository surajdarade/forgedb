#include "forgedb/database/database.h"
#include "forgedb/storage/serializer.h"
#include <algorithm>
#include <stdexcept>

namespace forgedb {
namespace { constexpr std::uint64_t kCatalogMagic = 0x464F524745434154ULL; }

Database::Database(std::string path, std::size_t pool, std::string logPath)
    : path_(std::move(path)),
      diskManager_(std::make_unique<DiskManager>(path_)),
      bufferPoolManager_(std::make_unique<BufferPoolManager>(pool, *diskManager_)),
      writeQueue_(1),
      transactionManager_(path_ + ".wal"),
      logger_(std::move(logPath)) {
    loadCatalog();
}

std::unique_ptr<Database> Database::open(const std::string& path, std::size_t pool, std::string logPath) {
    return std::unique_ptr<Database>(new Database(path, pool, std::move(logPath)));
}

Database::~Database() { try { close(); } catch (...) {} }

void Database::loadCatalog() {
    Page* p = bufferPoolManager_->fetchPage(PageId{0});
    if (!p) throw std::runtime_error("Database: catalog page unavailable");
    auto data = std::span<const std::uint8_t>(p->data().data(), p->data().size());
    try {
        std::size_t off = 0;
        if (Serializer::readUInt64(data, off) != kCatalogMagic) {
            bufferPoolManager_->unpinPage(PageId{0}, false);
            return;
        }
        const auto count = Serializer::readUInt32(data, off);
        for (std::uint32_t i = 0; i < count; ++i) {
            std::string name = Serializer::readString(data, off);
            PageId metadata{Serializer::readUInt64(data, off)};
            const auto columnCount = Serializer::readUInt32(data, off);
            std::vector<Column> columns;
            columns.reserve(columnCount);
            for (std::uint32_t c = 0; c < columnCount; ++c) {
                std::string columnName = Serializer::readString(data, off);
                const auto type = static_cast<DataType>(Serializer::readUInt8(data, off));
                const auto length = static_cast<std::size_t>(Serializer::readUInt32(data, off));
                columns.emplace_back(std::move(columnName), type, length);
            }
            auto entry = std::make_unique<TableEntry>(name, Schema{std::move(columns)}, metadata);
            entry->heap = std::make_unique<HeapFile>(*diskManager_, *bufferPoolManager_, metadata);
            entry->table = std::make_unique<Table>(entry->schema, *entry->heap);
            tables_.emplace(name, std::move(entry));
        }
    } catch (...) {
        bufferPoolManager_->unpinPage(PageId{0}, false);
        throw;
    }
    bufferPoolManager_->unpinPage(PageId{0}, false);
}

void Database::persistCatalog() {
    Page* p = bufferPoolManager_->fetchPage(PageId{0});
    if (!p) throw std::runtime_error("Database: catalog page unavailable");
    try {
        std::vector<std::uint8_t> bytes;
        Serializer::writeUInt64(bytes, kCatalogMagic);
        Serializer::writeUInt32(bytes, static_cast<std::uint32_t>(tables_.size()));
        for (const auto& [name, entry] : tables_) {
            Serializer::writeString(bytes, name);
            Serializer::writeUInt64(bytes, entry->metadataPage.value());
            Serializer::writeUInt32(bytes, static_cast<std::uint32_t>(entry->schema.columnCount()));
            for (const auto& column : entry->schema.columns()) {
                Serializer::writeString(bytes, column.name());
                Serializer::writeUInt8(bytes, static_cast<std::uint8_t>(column.type()));
                Serializer::writeUInt32(bytes, static_cast<std::uint32_t>(column.length()));
            }
        }
        if (bytes.size() > p->data().size()) throw std::length_error("Database: catalog page is full");
        std::fill(p->data().begin(), p->data().end(), 0);
        std::copy(bytes.begin(), bytes.end(), p->data().begin());
        bufferPoolManager_->unpinPage(PageId{0}, true);
    } catch (...) {
        bufferPoolManager_->unpinPage(PageId{0}, false);
        throw;
    }
}

Table& Database::createTable(std::string name, Schema schema) {
    ExclusiveLockGuard lock(catalogLock_);
    if (name.empty() || tables_.contains(name)) throw std::invalid_argument("Database: invalid or duplicate table name");
    PageId metadata;
    Page* page = bufferPoolManager_->newPage(metadata);
    if (!page) throw std::runtime_error("Database: unable to allocate table metadata");
    std::fill(page->data().begin(), page->data().end(), 0);
    bufferPoolManager_->unpinPage(metadata, true);
    auto entry = std::make_unique<TableEntry>(name, std::move(schema), metadata);
    entry->heap = std::make_unique<HeapFile>(*diskManager_, *bufferPoolManager_, metadata);
    entry->table = std::make_unique<Table>(entry->schema, *entry->heap);
    Table* result = entry->table.get();
    tables_.emplace(name, std::move(entry));
    persistCatalog();
    logger_.info("Created table " + name);
    return *result;
}

Table& Database::openTable(const std::string& name) {
    SharedLockGuard lock(catalogLock_);
    auto it = tables_.find(name);
    if (it == tables_.end()) throw std::out_of_range("Database: table not found");
    return *it->second->table;
}

bool Database::hasTable(const std::string& name) const {
    SharedLockGuard lock(const_cast<ReadWriteLock&>(catalogLock_));
    return tables_.contains(name);
}

void Database::dropTable(const std::string& name) {
    ExclusiveLockGuard lock(catalogLock_);
    tables_.erase(name);
    persistCatalog();
}

std::vector<std::string> Database::tableNames() const {
    SharedLockGuard lock(const_cast<ReadWriteLock&>(catalogLock_));
    std::vector<std::string> names;
    for (const auto& [name, entry] : tables_) { (void)entry; names.push_back(name); }
    std::sort(names.begin(), names.end());
    return names;
}

std::vector<Tuple> Database::select(const std::string& name, const Query& query) {
    return QueryEngine::select(openTable(name), query);
}

void Database::checkpoint() {
    bufferPoolManager_->flushAllPages();
    diskManager_->flush();
    logger_.flush();
}

void Database::close() {
    if (closed_) return;
    writeQueue_.drain();
    checkpoint();
    closed_ = true;
}

} // namespace forgedb
