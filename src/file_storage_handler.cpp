#include "file_storage_handler.hpp"

#include <filesystem>
#include <string>

namespace fs = std::filesystem;

namespace http {
namespace server {

FileStorageHandler::FileStorageHandler(const std::string &docRoot) : docRoot_(docRoot) {
    for (const auto &entry : fs::directory_iterator(docRoot))
        std::cout << entry.path() << std::endl;
}

void FileStorageHandler::handleRequest(const Request &req, Reply &rep) {}

}  // namespace server
}  // namespace http

