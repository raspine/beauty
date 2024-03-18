#pragma once

#include <map>
#include <set>
#include <string>

#include "reply.hpp"
#include "request.hpp"

namespace http {
namespace server {

class FileStorageHandler {
   public:
    FileStorageHandler(const std::string &docRoot);
    virtual ~FileStorageHandler() = default;

    void handleRequest(const Request &req, Reply &rep);

   private:
    const std::string docRoot_;
    std::set<std::string> files_;
};

}  // namespace server
}  // namespace http
