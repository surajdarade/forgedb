#include "forgedb/storage/disk_manager.h"

#include <filesystem>
#include <stdexcept>
#include <vector>

namespace forgedb {

DiskManager::DiskManager(const std::string& filePath)
    : filePath_(filePath)
{
    createDatabaseFile();

    file_.open(
        filePath_,
        std::ios::in |
        std::ios::out |
        std::ios::binary
    );

    if (!file_.is_open()) {
        throw std::runtime_error(
            "DiskManager: failed to open database file"
        );
    }
}

DiskManager::~DiskManager()
{
    flush();

    if (file_.is_open()) {
        file_.close();
    }
}

void DiskManager::createDatabaseFile()
{
    if (std::filesystem::exists(filePath_)) {
        return;
    }

    std::ofstream file(
        filePath_,
        std::ios::binary |
        std::ios::out
    );

    if (!file.is_open()) {
        throw std::runtime_error(
            "DiskManager: failed to create database file"
        );
    }

    // Reserve Page 0 for database metadata.
    const std::vector<Page::Byte> metadataPage(kPageSize, 0);

    file.write(
        reinterpret_cast<const char*>(metadataPage.data()),
        static_cast<std::streamsize>(metadataPage.size())
    );

    if (!file) {
        throw std::runtime_error(
            "DiskManager: failed to initialize database file"
        );
    }
}

PageId DiskManager::allocatePage()
{
    const PageId pageId{
        static_cast<PageId::ValueType>(pageCount())
    };

    Page emptyPage{pageId};

    writePage(pageId, emptyPage);

    return pageId;
}

void DiskManager::readPage(PageId pageId, Page& page)
{
    const std::size_t offset =
        pageId.value() * kPageSize;

    if (offset + kPageSize > fileSize()) {
        throw std::out_of_range(
            "DiskManager: page does not exist"
        );
    }

    file_.clear();

    file_.seekg(
        static_cast<std::streamoff>(offset),
        std::ios::beg
    );

    if (!file_) {
        throw std::runtime_error(
            "DiskManager: failed to seek while reading page"
        );
    }

    file_.read(
        reinterpret_cast<char*>(page.data().data()),
        static_cast<std::streamsize>(kPageSize)
    );

    if (!file_) {
        throw std::runtime_error(
            "DiskManager: failed to read page"
        );
    }

    page.setId(pageId);
}

void DiskManager::writePage(
    PageId pageId,
    const Page& page)
{
    const std::size_t offset =
        pageId.value() * kPageSize;

    file_.clear();

    file_.seekp(
        static_cast<std::streamoff>(offset),
        std::ios::beg
    );

    if (!file_) {
        throw std::runtime_error(
            "DiskManager: failed to seek while writing page"
        );
    }

    file_.write(
        reinterpret_cast<const char*>(page.data().data()),
        static_cast<std::streamsize>(kPageSize)
    );

    if (!file_) {
        throw std::runtime_error(
            "DiskManager: failed to write page"
        );
    }

    file_.flush();
}

std::size_t DiskManager::pageCount() const
{
    return fileSize() / kPageSize;
}

std::size_t DiskManager::fileSize() const
{
    std::error_code error;

    const auto size = std::filesystem::file_size(
        filePath_,
        error
    );

    if (error) {
        throw std::runtime_error(
            "DiskManager: failed to determine file size"
        );
    }

    return static_cast<std::size_t>(size);
}

void DiskManager::flush()
{
    if (file_.is_open()) {
        file_.flush();

        if (!file_) {
            throw std::runtime_error(
                "DiskManager: failed to flush database file"
            );
        }
    }
}

} // namespace forgedb