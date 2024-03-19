#include "multipart_parser.hpp"

#include <catch2/catch_test_macros.hpp>
#include <iostream>
#include <tuple>

#include "request.hpp"

using namespace http::server;

namespace {

std::vector<char> convertToCharVec(const std::string &s) {
    std::vector<char> ret(s.begin(), s.end());
    return ret;
}

}  // namespace

TEST_CASE("parse header", "[multipart_parser]") {
    MultiPartParser parser;
    Request request(1024);

    SECTION("should return OK when boundary is last") {
        request.headers_.push_back({"From", "user@example.com"});
        request.headers_.push_back(
            {"Content-Type",
             "multipart/form-data; boundary=----WebKitFormBoundarylSu7ajtLodoq9XHE"});
        request.headers_.push_back({"Content-Length", "281"});
        REQUIRE(parser.parseHeader(request));
    }
    SECTION("should return OK when boundary is first") {
        request.headers_.push_back({"From", "user@example.com"});
        request.headers_.push_back(
            {"Content-Type",
             "boundary=----WebKitFormBoundarylSu7ajtLodoq9XHE; multipart/form-data;"});
        request.headers_.push_back({"Content-Length", "281"});
        REQUIRE(parser.parseHeader(request));
    }
}

TEST_CASE("parse single part content", "[multipart_parser]") {
    MultiPartParser parser;
    Request request(1024);
    request.headers_.push_back({"From", "user@example.com"});
    request.headers_.push_back(
        {"Content-Type", "multipart/form-data; boundary=----WebKitFormBoundarylSu7ajtLodoq9XHE"});
    request.headers_.push_back({"Content-Length", "281"});
    REQUIRE(parser.parseHeader(request));  // make sure boundary is set

    const std::string body =
        "------WebKitFormBoundarylSu7ajtLodoq9XHE\r\n"
        "Content-Disposition: form-data; name=\"file1\"; filename=\"testfile01.txt\"\r\n"
        "Content-Type: text/plain\r\n"
        "\r\n"
        "This body is a bit tricky as it contains some ------WebKitFormBoundary chars, but not "
        "all,.\r\n"
        "\r\n"
        "------WebKitFormBoundarylSu7ajtLodoq9XHE--\r\n";
    request.content_ = convertToCharVec(body);

    SECTION("should return done for single parts") {
        // getting back the initial empty result
        std::deque<MultiPartParser::ContentPart> result;
        REQUIRE(parser.parse(request, result) == MultiPartParser::result_type::done);
        REQUIRE(result.size() == 0);

        // result==done so call flush
        parser.flush(request, result);
        REQUIRE(result.size() == 1);
        REQUIRE(result[0].filename_ == "testfile01.txt");
        REQUIRE(*result[0].start_ == 'T');
        REQUIRE(*(result[0].end_ - 1) == '.');
    }
}

TEST_CASE("parse multi part content", "[multipart_parser]") {
    MultiPartParser parser;
    Request request(1024);
    request.headers_.push_back({"From", "user@example.com"});
    request.headers_.push_back(
        {"Content-Type", "multipart/form-data; boundary=----WebKitFormBoundarylSu7ajtLodoq9XHE"});
    request.headers_.push_back({"Content-Length", "358"});
    REQUIRE(parser.parseHeader(request));  // make sure boundary is set

    const std::string body =
        "------WebKitFormBoundarylSu7ajtLodoq9XHE\r\n"
        "Content-Disposition: form-data; name=\"file1\"; filename=\"testfile01.txt\"\r\n"
        "Content-Type: text/plain\r\n"
        "\r\n"
        "First part.\r\n"
        "\r\n"
        "------WebKitFormBoundarylSu7ajtLodoq9XHE\r\n"
        "Content-Disposition: form-data; name=\"file1\"; filename=\"testfile02.txt\"\r\n"
        "Content-Type: text/plain\r\n"
        "\r\n"
        "Second part!\r\n"
        "\r\n"
        "------WebKitFormBoundarylSu7ajtLodoq9XHE--\r\n";
    request.content_ = convertToCharVec(body);

    SECTION("should return done for multiple parts") {
        std::deque<MultiPartParser::ContentPart> result;
        // getting back the initial empty result
        REQUIRE(parser.parse(request, result) == MultiPartParser::result_type::done);
        REQUIRE(result.size() == 0);

        // result==done so call flush
        parser.flush(request, result);
        REQUIRE(result.size() == 2);
        REQUIRE(result[0].filename_ == "testfile01.txt");
        REQUIRE(*result[0].start_ == 'F');
        REQUIRE(*(result[0].end_ - 1) == '.');

        REQUIRE(result[1].filename_ == "testfile02.txt");
        REQUIRE(*result[1].start_ == 'S');
        REQUIRE(*(result[1].end_ - 1) == '!');
    }
}

TEST_CASE("parse empty part content", "[multipart_parser]") {
    MultiPartParser parser;
    Request request(1024);
    request.headers_.push_back({"From", "user@example.com"});
    request.headers_.push_back(
        {"Content-Type", "multipart/form-data; boundary=----WebKitFormBoundarylSu7ajtLodoq9XHE"});
    request.headers_.push_back({"Content-Length", "184"});
    REQUIRE(parser.parseHeader(request));  // make sure boundary is set

    const std::string body =
        "------WebKitFormBoundarylSu7ajtLodoq9XHE\r\n"
        "Content-Disposition: form-data; name=\"file1\"; filename=\"empty.txt\"\r\n"
        "Content-Type: text/plain\r\n"
        "\r\n"
        "\r\n"
        "\r\n"
        "------WebKitFormBoundarylSu7ajtLodoq9XHE--\r\n";
    request.content_ = convertToCharVec(body);

    SECTION("should return size = 0") {
        std::deque<MultiPartParser::ContentPart> result;
        // getting back the initial empty result
        REQUIRE(parser.parse(request, result) == MultiPartParser::result_type::done);
        REQUIRE(result.size() == 0);

        // result==done so call flush
        parser.flush(request, result);
        REQUIRE(result.size() == 1);
        REQUIRE(result[0].filename_ == "empty.txt");
        REQUIRE(result[0].start_ != nullptr);
        REQUIRE(result[0].end_ == result[0].start_);
    }
}

TEST_CASE("content start and end in consecutive buffers", "[multipart_parser]") {
    MultiPartParser parser;
    Request request(150);
    request.headers_.push_back({"From", "user@example.com"});
    request.headers_.push_back(
        {"Content-Type", "multipart/form-data; boundary=----WebKitFormBoundarylSu7ajtLodoq9XHE"});
    request.headers_.push_back({"Content-Length", "281"});
    REQUIRE(parser.parseHeader(request));  // make sure boundary is set

    const std::string body1 =
        "------WebKitFormBoundarylSu7ajtLodoq9XHE\r\nContent-Disposition: form-data; "
        "name=\"file1\"; filename=\"testfile01.txt\"\r\nContent-Type: text/plain\r\n\r\nThis bo";
    const std::string body2 =
        "dy is a bit tricky as it contains some ------WebKitFormBoundary chars, but not "
        "all.\r\n\r\n------WebKitFormBoundarylSu7ajtLodoq9XHE--\r\n";
    std::deque<MultiPartParser::ContentPart> result;

    // getting back the initial empty result
    request.content_ = convertToCharVec(body1);
    REQUIRE(parser.parse(request, result) == MultiPartParser::result_type::indeterminate);
    REQUIRE(result.size() == 0);

    request.content_ = convertToCharVec(body2);
    REQUIRE(parser.parse(request, result) == MultiPartParser::result_type::done);
    REQUIRE(result.size() == 1);
    REQUIRE(result[0].filename_ == "testfile01.txt");
    REQUIRE(*result[0].start_ == 'T');
    REQUIRE(*(result[0].end_ - 1) == 'o');

    // result==done so call flush
    parser.flush(request, result);
    REQUIRE(result.size() == 1);
    REQUIRE(result[0].filename_ == "");
    REQUIRE(*result[0].start_ == 'd');
    REQUIRE(*(result[0].end_ - 1) == '.');
}

TEST_CASE("content end in next to last body part", "[multipart_parser]") {
    MultiPartParser parser;
    Request request(75);
    request.headers_.push_back({"From", "user@example.com"});
    request.headers_.push_back(
        {"Content-Type", "multipart/form-data; boundary=----WebKitFormBoundarylSu7ajtLodoq9XHE"});
    request.headers_.push_back({"Content-Length", "336"});
    REQUIRE(parser.parseHeader(request));  // make sure boundary is set

    const std::string body1 =
        "------WebKitFormBoundarylSu7ajtLodoq9XHE\r\nContent-Disposition: form-data; n";
    const std::string body2 =
        "ame=\"file1\"; filename=\"testfile01.txt\"\r\nContent-Type: text/plain\r\n\r\nThis bo";
    const std::string body3 =
        "dy is a bit tricky as it contains some ------WebKitFormBoundary chars, but ";
    const std::string body4 =
        "not all. Note that the closing boundary comes in the last part.\r\n\r\n------We";
    const std::string body5 = "bKitFormBoundarylSu7ajtLodoq9XHE--\r\n";
    std::deque<MultiPartParser::ContentPart> result;

    // getting back the initial empty result
    request.content_ = convertToCharVec(body1);
    REQUIRE(parser.parse(request, result) == MultiPartParser::result_type::indeterminate);
    REQUIRE(result.size() == 0);

    // Note: Whenever no start or end is found, the multipart parser assumes
    // that we're in the middle of a large file and creates a result with
    // start/end to match the entire request.content_ buffer.
    // However as this use case uses a very small maxContentSize (75), this
    // situation happens here for the first body1.
    // For real use cases it must be a requirement to ensure that we at least
    // get the content start in the first content buffer. I.e. maxContentSize
    // > some limit so content start is ensured to be included in "body1".
    // So here we "overlook" this and get result.size() == 1, but really it
    // should be 0. But to get '0', the code would need more logic that
    // in practise doesn't make much sense.
    request.content_ = convertToCharVec(body2);
    REQUIRE(parser.parse(request, result) == MultiPartParser::result_type::indeterminate);
    REQUIRE(result.size() == 1);

    request.content_ = convertToCharVec(body3);
    REQUIRE(parser.parse(request, result) == MultiPartParser::result_type::indeterminate);
    REQUIRE(result.size() == 1);
    REQUIRE(result[0].filename_ == "testfile01.txt");
    REQUIRE(result[0].start_ != nullptr);
    REQUIRE(*result[0].start_ == 'T');
    REQUIRE(*(result[0].end_ - 1) == 'o');

    request.content_ = convertToCharVec(body4);
    REQUIRE(parser.parse(request, result) == MultiPartParser::result_type::indeterminate);
    REQUIRE(result.size() == 1);
    REQUIRE(result[0].filename_ == "");
    REQUIRE(result[0].start_ != nullptr);
    REQUIRE(*result[0].start_ == 'd');
    REQUIRE(*(result[0].end_ - 1) == ' ');

    request.content_ = convertToCharVec(body5);
    REQUIRE(parser.parse(request, result) == MultiPartParser::result_type::done);
    REQUIRE(result.size() == 1);
    REQUIRE(result[0].filename_ == "");
    REQUIRE(result[0].start_ != nullptr);
    REQUIRE(*result[0].start_ == 'n');
    REQUIRE(*(result[0].end_ - 1) == '.');

    // result==done so call flush
    parser.flush(request, result);
    REQUIRE(result.size() == 0);
}

TEST_CASE("content end in previous body part and last part contain content", "[multipart_parser]") {
    MultiPartParser parser;
    Request request(350);
    request.headers_.push_back({"From", "user@example.com"});
    request.headers_.push_back(
        {"Content-Type", "multipart/form-data; boundary=----WebKitFormBoundarylSu7ajtLodoq9XHE"});
    request.headers_.push_back({"Content-Length", "336"});
    REQUIRE(parser.parseHeader(request));  // make sure boundary is set

    const std::string body1 =
        "------WebKitFormBoundarylSu7ajtLodoq9XHE\r\nContent-Disposition: form-data; "
        "name=\"file1\"; filename=\"testfile01.txt\"\r\nContent-Type: text/plain\r\n\r\nFirst "
        "part.\r\n\r\n------We"
        "bKitFormBoundarylSu7ajtLodoq9XHE\r\nContent-Disposition: form-data; name=\"file2\"; "
        "filename=\"testfile02.txt\"\r\nContent-Type: text/plain\r\n\r\nSecond "
        "part!\r\n\r\n------WebKitFor";
    const std::string body2 =
        "mBoundarylSu7ajtLodoq9XHE\r\nContent-Disposition: form-data; name=\"file3\"; "
        "filename=\"testfile03.txt\"\r\n"
        "Content-Type: text/plain\r\n\r\n"
        "Third part!\r\n"
        "\r\n"
        "------WebKitFormBoundarylSu7ajtLodoq9XHE--\r\n";
    std::deque<MultiPartParser::ContentPart> result;

    // getting back the initial empty result
    request.content_ = convertToCharVec(body1);
    REQUIRE(parser.parse(request, result) == MultiPartParser::result_type::indeterminate);
    REQUIRE(result.size() == 0);

    request.content_ = convertToCharVec(body2);
    REQUIRE(parser.parse(request, result) == MultiPartParser::result_type::done);
    REQUIRE(result.size() == 2);
    REQUIRE(result[0].filename_ == "testfile01.txt");
    REQUIRE(*result[0].start_ == 'F');
    REQUIRE(*(result[0].end_ - 1) == '.');
    REQUIRE(result[1].filename_ == "testfile02.txt");
    REQUIRE(*result[1].start_ == 'S');
    REQUIRE(*(result[1].end_ - 1) == '!');

    // result==done so call flush
    parser.flush(request, result);
    REQUIRE(result.size() == 1);
    REQUIRE(result[0].filename_ == "testfile03.txt");
    REQUIRE(*result[0].start_ == 'T');
    REQUIRE(*(result[0].end_ - 1) == '!');
}
