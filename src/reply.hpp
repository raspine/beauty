#pragma once
#include "environment.hpp"

#include <asio.hpp>
#include <string>
#include <vector>

#include "header.hpp"
#include "multipart_parser.hpp"

namespace http {
namespace server {

class RequestHandler;

class Reply {
    friend class RequestHandler;
    friend class Connection;

   public:
    Reply(const Reply&) = delete;
    Reply& operator=(const Reply&) = delete;

    Reply(size_t maxContentSize);
    virtual ~Reply() = default;

    enum status_type {
        ok = 200,
        created = 201,
        accepted = 202,
        no_content = 204,
        multiple_choices = 300,
        moved_permanently = 301,
        moved_temporarily = 302,
        not_modified = 304,
        bad_request = 400,
        unauthorized = 401,
        forbidden = 403,
        not_found = 404,
        internal_server_error = 500,
        not_implemented = 501,
        bad_gateway = 502,
        service_unavailable = 503
    };

    // Content to be sent in the reply.
    std::vector<char> content_;

    // Helper to provide standard replies.
    void stockReply(status_type status);

    // File path to open.
    std::string filePath_;

    void send(status_type status);
    void send(status_type status, const std::string& contentType);
    void sendPtr(status_type status, const std::string& contentType, const char* data, size_t size);
    void addHeader(const std::string& name, const std::string& val);

   private:
    // Headers to be included in the reply.
    status_type status_;
    std::vector<Header> defaultHeaders_;
    std::vector<Header> addedHeaders_;

    bool returnToClient_ = false;
    const char* contentPtr_ = nullptr;
    size_t contentSize_;

    // Keep track when replying with successive write buffers.
    size_t maxContentSize_;
    bool replyPartial_ = false;
    bool finalPart_ = false;

    // Keep track of the number of body bytes received in request body.
    int noBodyBytesReceived_ = -1;

    // Keep track if the body is a multi-part upload.
    bool isMultiPart_ = false;

    // Keep track of the last opened file in multi-part transfers.
    std::string lastOpenFileForWriteId_;
    unsigned multiPartCounter_ = 0;

    // Parser to handle multipart uploads.
    MultiPartParser multiPartParser_;

    // Convert the reply into a vector of buffers. The buffers do not own the
    // underlying memory blocks, therefore the reply object must remain valid
    // and not be changed until the write operation has completed.
    std::vector<asio::const_buffer> headerToBuffers();
    std::vector<asio::const_buffer> contentToBuffers();
};

}  // namespace server
}  // namespace http
