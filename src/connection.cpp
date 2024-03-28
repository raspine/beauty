#include "connection.hpp"

#include <iostream>
#include <utility>
#include <vector>

#include "connection_manager.hpp"
#include "request_handler.hpp"

namespace http {
namespace server {

Connection::Connection(asio::ip::tcp::socket socket,
                       ConnectionManager &manager,
                       RequestHandler &handler,
                       unsigned connectionId,
                       size_t maxContentSize)
    : socket_(std::move(socket)),
      connectionManager_(manager),
      requestHandler_(handler),
      connectionId_(connectionId),
      maxContentSize_(maxContentSize),
      buffer_(maxContentSize),
      reply_(maxContentSize) {}

void Connection::start() {
    doRead();
}

void Connection::stop() {
    socket_.close();
}

void Connection::doRead() {
    std::cout << "doReadBody\n";
    auto self(shared_from_this());
    // asio uses buffer_.size() to limit amount of read data so must restore
    // size before reading. Note: operation is "cheap" as maxContentSize is
    // already reserved.
    buffer_.resize(maxContentSize_);
    socket_.async_read_some(
        asio::buffer(buffer_), [this, self](std::error_code ec, std::size_t bytesTransferred) {
            if (!ec) {
                buffer_.resize(bytesTransferred);
                std::cout << "Recv buffer size: " << bytesTransferred << std::endl;
                std::cout << "#### Recv start ####" << std::endl;
                for (int i = 0; i < buffer_.size(); ++i) {
                    printf("%c", buffer_[i]);
                }
                puts("#### Recv end ####");
                RequestParser::result_type result = requestParser_.parse(request_, buffer_);

                if (result == RequestParser::good_complete) {
                    if (requestDecoder_.decodeRequest(request_, buffer_)) {
                        requestHandler_.handleRequest(connectionId_, request_, buffer_, reply_);
                        doWriteHeaders();
                    } else {
                        reply_.stockReply(Reply::bad_request);
                        doWriteHeaders();
                    }
                } else if (result == RequestParser::good_part) {
                    if (requestDecoder_.decodeRequest(request_, buffer_)) {
                        reply_.noBodyBytesReceived_ = request_.getNoInitialBodyBytesReceived();
                        std::cout << "Initial received body size: " << reply_.noBodyBytesReceived_
                                  << std::endl;
                        requestHandler_.handleRequest(connectionId_, request_, buffer_, reply_);
                        if (reply_.isMultiPart_) {
                            doWritePartAck();
                        } else {
                            doReadBody();
                        }
                    } else {
                        reply_.stockReply(Reply::bad_request);
                        doWriteHeaders();
                    }
                } else if (result == RequestParser::bad) {
                    reply_.stockReply(Reply::bad_request);
                    doWriteHeaders();
                } else {
                    doRead();
                }
            } else if (ec != asio::error::operation_aborted) {
                std::cout << "doRead: " << ec.message() << ':' << ec.value() << std::endl;
                connectionManager_.stop(shared_from_this());
            }
        });
}

void Connection::doWritePartAck() {
    std::cout << "doWritePartAck: " << reply_.status_ << "\n";
    auto self(shared_from_this());
    asio::async_write(
        socket_, reply_.headerToBuffers(), [this, self](std::error_code ec, std::size_t) {
            if (!ec) {
                doReadBody();
            } else {
                std::cout << "doWritePartAck: " << ec.message() << ':' << ec.value() << std::endl;
                shutdown();
            }
        });
}

void Connection::doReadBody() {
    std::cout << "doReadBody\n";
    buffer_.resize(maxContentSize_);
    auto self(shared_from_this());
    socket_.async_read_some(
        asio::buffer(buffer_), [this, self](std::error_code ec, std::size_t bytesTransferred) {
            if (!ec) {
                buffer_.resize(bytesTransferred);
                std::cout << "Recv buffer size: " << bytesTransferred << std::endl;
                std::cout << "#### Recv start ####" << std::endl;
                for (int i = 0; i < buffer_.size(); ++i) {
                    printf("%c", buffer_[i]);
                }
                std::cout << "#### Recv end ####" << std::endl;
                reply_.noBodyBytesReceived_ += bytesTransferred;
                std::cout << "Received body size: " << reply_.noBodyBytesReceived_ << std::endl;
                requestHandler_.handlePartialWrite(connectionId_, request_, buffer_, reply_);
                if (reply_.status_ != Reply::status_type::ok) {
                    doWriteHeaders();
                } else if (reply_.noBodyBytesReceived_ < request_.getBodySize()) {
                    doReadBody();
                } else {
                    doWriteHeaders();
                }
            } else if (ec != asio::error::operation_aborted) {
                std::cout << "doReadBody: " << ec.message() << ':' << ec.value() << std::endl;
                connectionManager_.stop(shared_from_this());
            }
        });
}

void Connection::doWriteHeaders() {
    std::cout << "doWriteHeaders: " << reply_.status_ << "\n";

    auto self(shared_from_this());
    asio::async_write(
        socket_, reply_.headerToBuffers(), [this, self](std::error_code ec, std::size_t) {
            if (!ec) {
                if (!reply_.content_.empty() || reply_.contentPtr_ != nullptr) {
                    doWriteContent();
                } else {
                    handleWriteCompleted();
                }
            } else {
                std::cout << "doWriteHeaders: " << ec.message() << ':' << ec.value() << std::endl;
                shutdown();
            }
        });
}

void Connection::doWriteContent() {
    std::cout << "doWriteContent\n";
    auto self(shared_from_this());
    asio::async_write(
        socket_, reply_.contentToBuffers(), [this, self](std::error_code ec, std::size_t) {
            if (!ec) {
                if (reply_.replyPartial_) {
                    if (reply_.finalPart_) {
                        handleWriteCompleted();
                    } else {
                        requestHandler_.handlePartialRead(connectionId_, reply_);
                        doWriteContent();
                    }
                } else {
                    handleWriteCompleted();
                }
            } else {
                std::cout << "doWriteContent: " << ec.message() << ':' << ec.value() << std::endl;
                shutdown();
            }
        });
}

void Connection::handleWriteCompleted() {
    std::cout << "handleWriteCompleted\n";
    // TODO: handle keep-alive
    // initiate graceful connection closure.
    std::error_code ignored_ec;
    socket_.shutdown(asio::ip::tcp::socket::shutdown_both, ignored_ec);
    connectionManager_.stop(shared_from_this());
}

void Connection::shutdown() {
    // initiate graceful connection closure.
    std::error_code ignored_ec;
    socket_.shutdown(asio::ip::tcp::socket::shutdown_both, ignored_ec);
    connectionManager_.stop(shared_from_this());
    requestHandler_.closeFile(reply_, connectionId_);
}

}  // namespace server
}  // namespace http
