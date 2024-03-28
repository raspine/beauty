#include "request_handler.hpp"

#include <iostream>
#include <string>

#include "beauty_common.hpp"
#include "file_handler.hpp"
#include "header.hpp"
#include "mime_types.hpp"
#include "reply.hpp"
#include "request.hpp"

namespace http {
namespace server {

namespace {

void defaultFileNotFoundHandler(Reply &rep) {
    rep.stockReply(Reply::not_found);
}

void defaultAddFileHeaderHandler(std::vector<Header> &headers) {}

bool startsWith(const std::string &s, const std::string &sv) {
    return s.rfind(sv, 0) == 0;
}

}

RequestHandler::RequestHandler(IFileHandler *fileHandler)
    : fileHandler_(fileHandler),
      fileNotFoundCb_(defaultFileNotFoundHandler),
      addFileHeaderCallback_(defaultAddFileHeaderHandler) {}

void RequestHandler::addRequestHandler(const requestHandlerCallback &cb) {
    requestHandlers_.push_back(cb);
}

void RequestHandler::setFileNotFoundHandler(const fileNotFoundHandlerCallback &cb) {
    fileNotFoundCb_ = cb;
}

void RequestHandler::addFileHeaderHandler(const addFileHeaderCallback &cb) {
    addFileHeaderCallback_ = cb;
}

void RequestHandler::handleRequest(unsigned connectionId,
                                   const Request &req,
                                   std::vector<char> &content,
                                   Reply &rep) {
    // initiate filePath with requestPath
    rep.filePath_ = req.requestPath_;

    // if path ends in slash (i.e. is a directory) then add "index.html"
    if (req.method_ == "GET" && rep.filePath_[rep.filePath_.size() - 1] == '/') {
        rep.filePath_ += "index.html";
    }

    for (const auto &requestHandler_ : requestHandlers_) {
        requestHandler_(req, rep);
        if (rep.returnToClient_) {
            return;
        }
    }

    if (fileHandler_ != nullptr) {
        if (req.method_ == "POST" && rep.multiPartParser_.parseHeader(req)) {
            rep.status_ = Reply::ok;
            rep.isMultiPart_ = true;
            handlePartialWrite(connectionId, req, content, rep);
            return;
        } else if (req.method_ == "GET") {
            if (openAndReadFile(connectionId, req, rep)) {
                return;
            }
        }
    }

    fileNotFoundCb_(rep);
}

void RequestHandler::handlePartialRead(unsigned connectionId, Reply &rep) {
    size_t nrReadBytes = readFromFile(connectionId, rep);

    if (nrReadBytes < rep.maxContentSize_) {
        rep.finalPart_ = true;
        fileHandler_->closeFile(connectionId);
    }
}

void RequestHandler::handlePartialWrite(unsigned connectionId,
                                        const Request &req,
                                        std::vector<char> &content,
                                        Reply &rep) {
    std::deque<MultiPartParser::ContentPart> parts;
    MultiPartParser::result_type result = rep.multiPartParser_.parse(req, content, parts);
    std::cout << "result: " << result << std::endl;

    if (result == MultiPartParser::result_type::bad) {
        rep.stockReply(Reply::status_type::bad_request);
        return;
    }

    writeFileParts(connectionId, req, rep, parts);

    if (result == MultiPartParser::result_type::done) {
        rep.multiPartParser_.flush(content, parts);
        writeFileParts(connectionId, req, rep, parts);
    }

    // done with content unless there's 'bad' messages that should be return to
    // client
    if (rep.status_ == Reply::status_type::ok) {
        rep.content_.clear();
    }
}

void RequestHandler::closeFile(unsigned connectionId) {
    if (fileHandler_ != nullptr) {
        fileHandler_->closeFile(connectionId);
    }
}

bool RequestHandler::openAndReadFile(unsigned connectionId, const Request &req, Reply &rep) {
    // open the file to send back
    size_t contentSize = fileHandler_->openFileForRead(connectionId, rep.filePath_);
    if (contentSize > 0) {
        // determine the file extension.
        std::string extension;
        // if directory, then extension is provided by index.html
        if (req.requestPath_[req.requestPath_.size() - 1] == '/') {
            extension = "html";
        } else {
            std::size_t lastSlashPos = req.requestPath_.find_last_of("/");
            std::size_t lastDotPos = req.requestPath_.find_last_of(".");
            if (lastDotPos != std::string::npos && lastDotPos > lastSlashPos) {
                extension = req.requestPath_.substr(lastDotPos + 1);
            }
        }

        // fill initial content
        rep.replyPartial_ = contentSize > rep.maxContentSize_;
        rep.status_ = Reply::ok;
        readFromFile(connectionId, rep);
        if (!rep.replyPartial_) {
            // all data fits in initial content
            fileHandler_->closeFile(connectionId);
        }

        rep.defaultHeaders_.resize(2);
        rep.defaultHeaders_[0].name_ = "Content-Length";
        rep.defaultHeaders_[0].value_ = std::to_string(contentSize);
        rep.defaultHeaders_[1].name_ = "Content-Type";
        rep.defaultHeaders_[1].value_ = mime_types::extensionToType(extension);
        addFileHeaderCallback_(rep.addedHeaders_);
        return true;
    }
    return false;
}

size_t RequestHandler::readFromFile(unsigned connectionId, Reply &rep) {
    rep.content_.resize(rep.maxContentSize_);
    int nrReadBytes =
        fileHandler_->readFile(connectionId, rep.content_.data(), rep.content_.size());
    rep.content_.resize(nrReadBytes);
    return nrReadBytes;
}

void RequestHandler::writeFileParts(unsigned connectionId,
                                    const Request &req,
                                    Reply &rep,
                                    std::deque<MultiPartParser::ContentPart> &parts) {
    std::cout << "parts size: " << parts.size() << std::endl;
    // It seems that most clients first deliver a "headerOnly" part of the multipart
    // asking for confirmation and then in successive request deliver the part
    // data. If so, we handle this nicely here by giving the client an
    // headerOnly response of the openFileForWrite() response. However this
    // requires "peaking" the last part as the MultiPartParser delivers parts
    // one request too late.
    const std::deque<MultiPartParser::ContentPart> &peakParts = rep.multiPartParser_.peakLastPart();
    for (auto &part : peakParts) {
        if (part.headerOnly_ && !part.filename_.empty()) {
            std::cout << "header only handled with peak\n";
            std::string filePath = req.requestPath_ + peakParts[0].filename_;
            std::string err;
            rep.status_ = fileHandler_->openFileForWrite(connectionId, filePath, err);
            if (rep.status_ != Reply::status_type::ok &&
                rep.status_ != Reply::status_type::created) {
                rep.content_.insert(rep.content_.begin(), err.begin(), err.end());
                return;
            }
            return;
        }
    }

    for (auto &part : parts) {
        std::string err;
        // if 'headerOnly' it as already been handled above
        if (!part.headerOnly_) {
            std::cout << "part contain data\n";
            if (!part.filename_.empty()) {
                std::cout << "part contain filename\n";
                std::string filePath = req.requestPath_ + part.filename_;
                rep.status_ = fileHandler_->openFileForWrite(connectionId, filePath, err);
                if (rep.status_ != Reply::status_type::ok &&
                    rep.status_ != Reply::status_type::created) {
                    rep.content_.insert(rep.content_.begin(), err.begin(), err.end());
                    return;
                }
            }
            size_t size = part.end_ - part.start_;
            rep.status_ = fileHandler_->writeFile(connectionId, &(*part.start_), size, err);
            if (rep.status_ != Reply::status_type::ok &&
                rep.status_ != Reply::status_type::created) {
                fileHandler_->closeFile(connectionId);
                rep.content_.insert(rep.content_.begin(), err.begin(), err.end());
                return;
            }
            if (part.foundEnd_) {
                std::cout << "foundEnd\n";
                fileHandler_->closeFile(connectionId);
            }
        }
    }
    std::cout << "parts handled\n";
}

}  // namespace server
}  // namespace http
