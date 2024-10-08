#pragma once

#include <string>
#include <vector>

#include "reply.hpp"
#include "request.hpp"

namespace beauty {

class MockRequestHandler {
   public:
    MockRequestHandler(std::vector<char>& body);
    virtual ~MockRequestHandler() = default;

    void handleRequest(const Request& req, Reply& rep);

    void setReturnToClient(bool ret);
    void setMockedReply(Reply::status_type status, const std::string& content);
    int getNoCalls();
    Request getReceivedRequest() const;
    Reply& getReceivedReply();

   private:
    int noCalls_ = 0;
    bool returnToClient_ = false;
    Reply::status_type status_ = Reply::status_type::internal_server_error;
    std::string mockedContent_;
    Request receivedRequest_;
    Reply receivedReply_;
};

}  // namespace beauty
