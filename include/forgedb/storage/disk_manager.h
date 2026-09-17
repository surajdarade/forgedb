#pragma once

#include <cstddef>
#include <fstream>
#include <string>

#include "forgedb/common/page_id.h"
#include "forgedb/storage/page.h"

namespace forgedb {

class DiskManager {
public:
    explicit DiskManager(const std::string& filePath);

    ~DiskManager();

    DiskManager(const DiskManager&) = delete;
    DiskManager& operator=(const DiskManager&) = delete;

    DiskManager(DiskManager&&) = delete;
    DiskManager& operator=(DiskManager&&) = delete;

    [[nodiscard]] PageId allocatePage();

    void readPage(PageId pageId, Page& page);

    void writePage(PageId pageId, const Page& page);

    [[nodiscard]] std::size_t pageCount() const;

    void flush();

private:
    void createDatabaseFile();

    [[nodiscard]] std::size_t fileSize() const;

    std::fstream file_;
    std::string filePath_;
};

} // namespace forgedb