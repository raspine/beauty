#include "file_handler.hpp"

#include <limits>

namespace http {
namespace server {

FileHandler::FileHandler(const std::string &docRoot) : docRoot_(docRoot) {}

size_t FileHandler::openFileForRead(unsigned id, const std::string &path) {
    std::string fullPath = docRoot_ + path;
    std::ifstream &is = openReadFiles_[id];
    is.open(fullPath.c_str(), std::ios::in | std::ios::binary);
    is.ignore(std::numeric_limits<std::streamsize>::max());
    size_t fileSize = is.gcount();
    is.clear();  //  since ignore will have set eof.
    is.seekg(0, std::ios_base::beg);
    if (is.is_open()) {
        return fileSize;
    }
    openReadFiles_.erase(id);
    return 0;
}

Reply::status_type FileHandler::openFileForWrite(unsigned id,
                                                 const std::string &path,
                                                 std::string &err) {
    // TODO: error handling
    std::string fullPath = docRoot_ + path;
    std::ofstream &os = openWriteFiles_[id];
    os.open(fullPath.c_str(), std::ios::out | std::ios::binary);
    return Reply::status_type::ok;
}

void FileHandler::closeFile(unsigned id) {
    openReadFiles_[id].close();
    openReadFiles_.erase(id);
    openWriteFiles_[id].close();
    openWriteFiles_.erase(id);
}

int FileHandler::readFile(unsigned id, char *buf, size_t maxSize) {
    openReadFiles_[id].read(buf, maxSize);
    return openReadFiles_[id].gcount();
}

Reply::status_type FileHandler::writeFile(unsigned id,
                                          const char *buf,
                                          size_t size,
                                          std::string &err) {
    // TODO: error handling
    openWriteFiles_[id].write(buf, size);
    return Reply::status_type::ok;
}

}  // namespace server
}  // namespace http
