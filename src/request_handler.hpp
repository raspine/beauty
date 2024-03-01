#pragma once

#include <string>

#include "file_handler.hpp"

namespace http {
namespace server {

struct Reply;
struct Request;

/// The common handler for all incoming requests.
class RequestHandler {
   public:
    RequestHandler(const RequestHandler &) = delete;
    RequestHandler &operator=(const RequestHandler &) = delete;

    /// Construct with a directory containing files to be served.
    explicit RequestHandler(const std::string &doc_root, IFileHandler &fileHandler);

    /// Handle a request and produce a reply.
    void handleRequest(const Request &req, Reply &rep);

   private:
    /// The directory containing the files to be served.
    std::string docRoot_;
    IFileHandler &fileHandler_;

    /// Perform URL-decoding on a string. Returns false if the encoding was
    /// invalid.
    static bool urlDecode(const std::string &in, std::string &out);
};

}  // namespace server
}  // namespace http