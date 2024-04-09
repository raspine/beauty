#include "server.hpp"

#include <signal.h>

#include <chrono>
#include <iostream>
#include <utility>

using namespace std::literals::chrono_literals;

namespace http {
namespace server {

Server::Server(asio::io_context &ioContext,
               uint16_t port,
               IFileHandler *fileHandler,
               HttpPersistence options,
               size_t maxContentSize)
    : acceptor_(ioContext, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port)),
      connectionManager_(options),
      requestHandler_(fileHandler),
      timer_(ioContext),
      maxContentSize_(maxContentSize) {
    if (maxContentSize < 1024) {
        std::cout << "maxContentSize must be equal or larger than 1024 bytes" << std::endl;
        return;
    }
    doAccept();
    doTick();
}

Server::Server(asio::io_context &ioContext,
               const std::string &address,
               const std::string &port,
               IFileHandler *fileHandler,
               HttpPersistence options,
               size_t maxContentSize)
    : acceptor_(ioContext),
      connectionManager_(options),
      requestHandler_(fileHandler),
      timer_(ioContext),
      maxContentSize_(maxContentSize) {
    // Register to handle the signals that indicate when the server should exit.
    // It is safe to register for the same signal multiple times in a program,
    // provided all registration for the specified signal is made through Asio.
    signals_ = std::make_shared<asio::signal_set>(ioContext);
    signals_->add(SIGINT);
    signals_->add(SIGTERM);
#if defined(SIGQUIT)
    signals_->add(SIGQUIT);
#endif  // defined(SIGQUIT)

    if (maxContentSize < 1024) {
        std::cout << "maxContentSize must be equal or larger than 1024 bytes" << std::endl;
        return;
    }
    doAwaitStop();

    // Open the acceptor with the option to reuse the address (i.e.
    // SO_REUSEADDR).
    asio::ip::tcp::resolver resolver(ioContext);
    asio::ip::tcp::endpoint endpoint = *resolver.resolve(address, port).begin();
    acceptor_.open(endpoint.protocol());
    acceptor_.set_option(asio::ip::tcp::acceptor::reuse_address(true));
    acceptor_.bind(endpoint);
    acceptor_.listen();

    doAccept();
    doTick();
}

uint16_t Server::getBindedPort() const {
    return acceptor_.local_endpoint().port();
}

void Server::addRequestHandler(const handlerCallback &cb) {
    requestHandler_.addRequestHandler(cb);
}

void Server::setFileNotFoundHandler(const handlerCallback &cb) {
    requestHandler_.setFileNotFoundHandler(cb);
}

void Server::doAccept() {
    acceptor_.async_accept([this](std::error_code ec, asio::ip::tcp::socket socket) {
        // Check whether the server was stopped by a signal before this
        // completion handler had a chance to run.
        if (!acceptor_.is_open()) {
            return;
        }

        if (!ec) {
            connectionManager_.start(std::make_shared<Connection>(std::move(socket),
                                                                  connectionManager_,
                                                                  requestHandler_,
                                                                  connectionId_++,
                                                                  maxContentSize_));
        } else {
            std::cout << "doAccept: " << ec.message() << ':' << ec.value() << std::endl;
        }

        doAccept();
    });
}

void Server::doAwaitStop() {
    signals_->async_wait([this](std::error_code /*ec*/, int /*signo*/) {
        timer_.cancel();
        acceptor_.close();
        connectionManager_.stopAll();
    });
}

void Server::doTick() {
    timer_.expires_after(1s);
    timer_.async_wait([this](std::error_code ec) {
        if (!ec) {
            connectionManager_.tick();

            doTick();
        }
    });
}

}  // namespace server
}  // namespace http
