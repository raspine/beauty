#include "request_decoder.hpp"

#include <catch2/catch_test_macros.hpp>

#include "request.hpp"

using namespace http::server;

// namespace {

// const std::string PostRequstWithFormParams =
//     "POST /uri.cgi HTTP/1.1\r\n"
//     "Content-Type: application/x-www-form-urlencoded\r\n"
//     "Content-Length: 31\r\n"
//     "\r\n"
//     "arg1=test;arg1=%20%21;arg3=test";

// }

TEST_CASE("error handling", "[request_parser]") {
    RequestDecoder reqDecoder;
    Request req(1024);

    SECTION("it should false for invalid path") {
        req.uri_ =
            "GET ../index.html HTTP/1.1\r\nHost: 127.0.0.1\r\nAccept: "
            "*/*\r\nConnection: "
            "close\r\n\r\n";
        REQUIRE(reqDecoder.decodeRequest(req) == false);
    }
}

TEST_CASE("decode GET request", "[request_decoder]") {
    RequestDecoder reqDecoder;
    Request getRequest(1024);
    getRequest.method_ = "GET";
    getRequest.uri_ = "/file.bin?myKey=my%20value";
    getRequest.httpVersionMajor_ = 1;
    getRequest.httpVersionMinor_ = 1;

    SECTION("it should provide correct request path") {
        REQUIRE(reqDecoder.decodeRequest(getRequest) == true);
        REQUIRE(getRequest.requestPath_ == "/file.bin");
    }
    SECTION("it should provide correct query params") {
        REQUIRE(reqDecoder.decodeRequest(getRequest) == true);
        REQUIRE(getRequest.queryParams_.size() == 1);
        REQUIRE(getRequest.queryParams_[0] ==
                std::make_pair<std::string, std::string>("myKey", "my value"));
    }
}

TEST_CASE("decode POST request", "[request_decoder]") {
    RequestDecoder reqDecoder;
    Request postRequest(1024);
    postRequest.method_ = "POST";
    postRequest.uri_ = "/uri.cgi";
    postRequest.httpVersionMajor_ = 1;
    postRequest.httpVersionMinor_ = 1;

    postRequest.headers_.push_back({"Host", "127.0.0.1"});
    postRequest.headers_.push_back({"Accept", "*/*"});
    postRequest.headers_.push_back({"Content-Type", "application/x-www-form-urlencoded"});
    postRequest.headers_.push_back({"Content-Length", "21"});
    postRequest.content_ = {'a', 'r', 'g', '1', '=', 't', 'e', 's', 't', '&', 'a',
                            'r', 'g', '2', '=', '%', '2', '0', '%', '2', '1'};

    SECTION("it should provide correct form params") {
        REQUIRE(reqDecoder.decodeRequest(postRequest) == true);

        REQUIRE(postRequest.formParams_.size() == 2);
        REQUIRE(postRequest.formParams_[0] ==
                std::make_pair<std::string, std::string>("arg1", "test"));
        REQUIRE(postRequest.formParams_[1] ==
                std::make_pair<std::string, std::string>("arg2", " !"));
    }
}
