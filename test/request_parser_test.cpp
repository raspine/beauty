#include "request_parser.hpp"

#include <catch2/catch_test_macros.hpp>
#include <iostream>
#include <tuple>

#include "request.hpp"

using namespace http::server;

namespace {

struct RequestFixture {
    RequestFixture(size_t maxContentSize) : request(maxContentSize) {}
    RequestParser::result_type parse(const std::string &text) {
        RequestParser parser;

        RequestParser::result_type result;
        std::tie(result, std::ignore) =
            parser.parse(request, text.c_str(), text.c_str() + text.size());

        return result;
    }
    Request request;
};

std::vector<char> convertToCharVec(const std::string &s) {
    std::vector<char> ret(s.begin(), s.end());
    return ret;
}

}  // namespace

TEST_CASE("parse GET request", "[request_parser]") {
    RequestFixture fixture(1024);
    SECTION("should return false for misspelling") {
        const char *text = "GET /uri HTTTP/0.9\r\n\r\n";
        auto result = fixture.parse(text);

        REQUIRE(result == RequestParser::bad);
    }
    SECTION("should parse GET HTTP/0.9") {
        const char *text = "GET /uri HTTP/0.9\r\n\r\n";
        auto result = fixture.parse(text);

        REQUIRE(result == RequestParser::good_complete);

        REQUIRE(fixture.request.method_ == "GET");
        REQUIRE(fixture.request.uri_ == "/uri");
        REQUIRE(fixture.request.httpVersionMajor_ == 0);
        REQUIRE(fixture.request.httpVersionMinor_ == 9);
    }
    SECTION("should parse GET HTTP/1.0") {
        const char *text = "GET /uri HTTP/1.0\r\n\r\n";
        auto result = fixture.parse(text);

        REQUIRE(result == RequestParser::good_complete);

        REQUIRE(fixture.request.method_ == "GET");
        REQUIRE(fixture.request.uri_ == "/uri");
        REQUIRE(fixture.request.httpVersionMajor_ == 1);
        REQUIRE(fixture.request.httpVersionMinor_ == 0);
        REQUIRE(fixture.request.keepAlive_ == false);
    }
    SECTION("should parse GET HTTP/1.1") {
        const char *text = "GET /uri HTTP/1.1\r\n\r\n";
        auto result = fixture.parse(text);

        REQUIRE(result == RequestParser::good_complete);

        REQUIRE(fixture.request.method_ == "GET");
        REQUIRE(fixture.request.uri_ == "/uri");
        REQUIRE(fixture.request.httpVersionMajor_ == 1);
        REQUIRE(fixture.request.httpVersionMinor_ == 1);
        REQUIRE(fixture.request.keepAlive_);
    }
    SECTION("should parse uri with query params") {
        const char *text = "GET /uri?arg1=test&arg1=%20%21&arg3=test\r\n\r\n";
        auto result = fixture.parse(text);

        REQUIRE(result == RequestParser::good_complete);

        REQUIRE(fixture.request.method_ == "GET");
        REQUIRE(fixture.request.uri_ == "/uri?arg1=test&arg1=%20%21&arg3=test");
    }
}

TEST_CASE("parse POST request", "[request_parser]") {
    RequestFixture fixture(1024);
    SECTION("should parse POST HTTP/1.1") {
        const char *text = "POST /uri HTTP/1.1\r\n\r\n";
        auto result = fixture.parse(text);

        REQUIRE(result == RequestParser::good_complete);

        REQUIRE(fixture.request.method_ == "POST");
        REQUIRE(fixture.request.uri_ == "/uri");
        REQUIRE(fixture.request.httpVersionMajor_ == 1);
        REQUIRE(fixture.request.httpVersionMinor_ == 1);
        REQUIRE(fixture.request.keepAlive_);
    }
    SECTION("should parse POST HTTP/1.1 with header field") {
        const char *text =
            "POST /uri HTTP/1.1\r\n"
            "X-Custom-Header: header value\r\n"
            "\r\n";
        auto result = fixture.parse(text);

        REQUIRE(result == RequestParser::good_complete);

        REQUIRE(fixture.request.headers_.size() == 1);
        REQUIRE(fixture.request.headers_[0].name_ == "X-Custom-Header");
        REQUIRE(fixture.request.headers_[0].value_ == "header value");
    }
    SECTION("should parse POST HTTP/1.1 with body") {
        const char *text =
            "POST /uri.cgi HTTP/1.1\r\n"
            "From: user@example.com\r\n"
            "User-Agent: Mozilla/5.0 (Windows NT 6.1; WOW64; rv:18.0) Gecko/20100101 "
            "Firefox/18.0\r\n"
            "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n"
            "Accept-Encoding: gzip, deflate\r\n"
            "Content-Type: application/x-www-form-urlencoded\r\n"
            "Content-Length: 31\r\n"
            "\r\n"
            "arg1=test;arg1=%20%21;arg3=test";

        auto result = fixture.parse(text);

        REQUIRE(result == RequestParser::good_complete);

        REQUIRE(fixture.request.headers_.size() == 6);
        REQUIRE(fixture.request.headers_[0].name_ == "From");
        REQUIRE(fixture.request.headers_[0].value_ == "user@example.com");
        REQUIRE(fixture.request.headers_[1].name_ == "User-Agent");
        REQUIRE(fixture.request.headers_[1].value_ ==
                "Mozilla/5.0 (Windows NT 6.1; WOW64; rv:18.0) Gecko/20100101 Firefox/18.0");
        REQUIRE(fixture.request.headers_[2].name_ == "Accept");
        REQUIRE(fixture.request.headers_[2].value_ ==
                "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8");
        REQUIRE(fixture.request.headers_[3].name_ == "Accept-Encoding");
        REQUIRE(fixture.request.headers_[3].value_ == "gzip, deflate");
        REQUIRE(fixture.request.headers_[4].name_ == "Content-Type");
        REQUIRE(fixture.request.headers_[4].value_ == "application/x-www-form-urlencoded");
        REQUIRE(fixture.request.headers_[5].name_ == "Content-Length");
        REQUIRE(fixture.request.headers_[5].value_ == "31");
        REQUIRE(fixture.request.content_ == convertToCharVec("arg1=test;arg1=%20%21;arg3=test"));
    }
    SECTION("should parse POST HTTP/1.1 with chunked body") {
        const char *text =
            "POST /uri.cgi HTTP/1.1\r\n"
            "Content-Type: text/plain\r\n"
            "Transfer-Encoding: chunked\r\n"
            "\r\n"
            "24\r\n"
            "This is the data in the first chunk \r\n"
            "1B\r\n"
            "and this is the second one \r\n"
            "3\r\n"
            "con\r\n"
            "9\r\n"
            "sequence\0\r\n"
            "0\r\n\r\n";

        auto result = fixture.parse(text);
        REQUIRE(result == RequestParser::indeterminate);

        REQUIRE(fixture.request.headers_.size() == 2);
        REQUIRE(fixture.request.content_ ==
                convertToCharVec("This is the data in the first chunk and "
                                 "this is the second one consequence"));
    }
    SECTION("should parse POST HTTP/1.1 with chunked extension") {
        const char *text =
            "POST /uri.cgi HTTP/1.1\r\n"
            "Content-Type: text/plain\r\n"
            "Transfer-Encoding: chunked\r\n"
            "\r\n"
            "24\r\n"
            "This is the data in the first chunk \r\n"
            "1B; secondname=secondvalue\r\n"
            "and this is the second one\0\r\n"
            "0\r\n"
            "Trailer: value\r\n"
            "\r\n";

        auto result = fixture.parse(text);
        REQUIRE(result == RequestParser::indeterminate);

        REQUIRE(fixture.request.headers_.size() == 2);
        REQUIRE(fixture.request.content_ ==
                convertToCharVec("This is the data in the first chunk and "
                                 "this is the second one"));
    }
}

TEST_CASE("parse POST request partially", "[request_parser]") {
    RequestFixture fixture(50);
    const char *text =
        "POST / HTTP/1.1\r\n"
        "From: user@example.com\r\n"
        "User-Agent: Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) "
        "Chrome/122.0.0.0 Safari/537.36\r\n"
        "Accept: */*\r\n"
        "Accept-Encoding: gzip, deflate\r\n"
        "Content-Type: multipart/form-data; "
        "boundary=----WebKitFormBoundarylSu7ajtLodoq9XHE\r\n"
        "Content-Length: 281\r\n"
        "\r\n"
        "------WebKitFormBoundarylSu7ajtLodoq9XHE\r\n"
        "Content-Disposition: form-data; name=\"file1\"; filename=\"testfile01.txt\"\r\n"
        "Content-Type: text/plain\r\n"
        "\r\n"
        "This body is longer than 50 characters. So we expect request_parser to return "
        "good_partial.\r\n"
        "\r\n"
        "------WebKitFormBoundarylSu7ajtLodoq9XHE--\r\n";

    auto result = fixture.parse(text);
    REQUIRE(result == RequestParser::good_part);
    REQUIRE(fixture.request.content_ ==
            convertToCharVec("------WebKitFormBoundarylSu7ajtLodoq9XHE\r\nContent-"));
}
