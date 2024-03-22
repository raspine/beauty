#pragma once

#include <string>

#include "reply.hpp"

namespace http {
namespace server {

class IFileHandler {
   public:
    IFileHandler() = default;
    virtual ~IFileHandler() = default;

    virtual size_t openFileForRead(unsigned id, const std::string& path) = 0;
    virtual void closeFile(unsigned id) = 0;
    virtual int readFile(unsigned id, char* buf, size_t maxSize) = 0;
    virtual Reply::status_type openFileForWrite(unsigned id,
                                                const std::string& path,
                                                std::string& err) = 0;
    virtual Reply::status_type writeFile(unsigned id,
                                         const char* buf,
                                         size_t size,
                                         std::string& err) = 0;
};

}  // namespace server
}  // namespace http
