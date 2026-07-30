#include <gtest/gtest.h>
#include "HttpParser.h"
#include <string>

TEST(HttpParser, ParseValidGetRequest) {

    std::string rawRequest =
    "GET /index.html HTTP/1.1\r\n"
    "Host: localhost:8080\r\n"
    "User-Agent: curl/7.68.0\r\n"
    "Accept: */*\r\n"
    "\r\n";

    auto result = HttpParser::parse(rawRequest);

    ASSERT_TRUE(result.has_value());



    EXPECT_EQ(result->method, "GET");
    EXPECT_EQ(result->path, "/index.html");
    EXPECT_EQ(result->version, "HTTP/1.1");

    EXPECT_EQ(result->headers.at("host"), "localhost:8080");
    EXPECT_EQ(result->headers.at("user-agent"), "curl/7.68.0");
    EXPECT_EQ(result->headers.at("accept"), "*/*");
}

TEST(HttpParser, NormalizesHeaderNameToLowercase)
{
    std::string rawRequest =
    "GET / HTTP/1.1\r\n"
    "CoNtEnT-tYpE: application/json\r\n"
    "\r\n";

    auto result = HttpParser::parse(rawRequest);

    ASSERT_TRUE(result.has_value());

    EXPECT_EQ(
        result->headers.at("content-type"),
        "application/json"
    );

}

TEST(HttpParser, ParsesRequestBody)
{
    const std::string body =
     R"({"username":"idriss","password":"1234"})";

    const std::string rawRequest =
    "POST /login HTTP/1.1\r\n"
    "Content-Type: application/json\r\n"
    "Content-Length: " +
    std::to_string(body.size()) +
    "\r\n"
    "\r\n" +
    body;


    auto result = HttpParser::parse(rawRequest);

    ASSERT_TRUE(result.has_value());

    EXPECT_EQ(result->method, "POST");
    EXPECT_EQ(result->path, "/login");
    EXPECT_EQ(result->version, "HTTP/1.1");

    EXPECT_EQ(
        result->headers.at("content-type"),
        "application/json"
    );

    EXPECT_EQ(
        result->body,
        body
    );
}

TEST(HttpParser, RejectsIncompleteRequestLine)
{
    std::string rawRequest =
    "GET /index.html\r\n"
    "\r\n";

    auto result = HttpParser::parse(rawRequest);

    EXPECT_FALSE(result.has_value());
}

TEST(HttpParser, RejectsMalformedHeader)
{
    std::string rawRequest =
    "GET / HTTP/1.1\r\n"
    "host localhost\r\n"
    "\r\n";

    auto result = HttpParser::parse(rawRequest);

    EXPECT_FALSE(result.has_value());
}

TEST(HttpParser, RejectsBodyShorterThanContentLength)
{
    const std::string body =
      R"({"username":"idriss"})";

    const std::string rawRequest =
    "POST /login HTTP/1.1\r\n"
    "Content-Type: application/json\r\n"
    "Content-Length: " +
    std::to_string(body.size() + 5) +
    "\r\n"
    "\r\n" +
    body;

    auto result = HttpParser::parse(rawRequest);

    EXPECT_FALSE(result.has_value());
}

TEST(HttpParser, RejectsInvalidContentLength)
{
    const std::string rawRequest =
    "POST /login HTTP/1.1\r\n"
    "Content-Length: abc\r\n"
    "\r\n";

    auto result = HttpParser::parse(rawRequest);

    EXPECT_FALSE(result.has_value());

}

TEST(HttpParser, RejectsPartiallyNumericContentLength)
{
    const std::string rawRequest =
        "POST /login HTTP/1.1\r\n"
        "Content-Length: 42abc\r\n"
        "\r\n";

    auto result = HttpParser::parse(rawRequest);

    EXPECT_FALSE(result.has_value());
}


TEST(HttpParser, RejectsNegativeContentLength)
{
    const std::string rawRequest =
        "POST /login HTTP/1.1\r\n"
        "Content-Length: -1\r\n"
        "\r\n";

    auto result = HttpParser::parse(rawRequest);

    EXPECT_FALSE(result.has_value());
}

TEST(HttpParser, RejectsEmptyContentLength)
{
    const std::string rawRequest =
        "POST /login HTTP/1.1\r\n"
        "Content-Length:\r\n"
        "\r\n";

    auto result = HttpParser::parse(rawRequest);

    EXPECT_FALSE(result.has_value());
}

TEST(HttpParser, AcceptsZeroContentLengthWithEmptyBody)
{
    const std::string rawRequest =
        "POST /login HTTP/1.1\r\n"
        "Content-Length: 0\r\n"
        "\r\n";

    auto result = HttpParser::parse(rawRequest);

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->body.empty());
}

TEST(HttpParser, AcceptsWhitespaceAroundContentLengthValue)
{
    const std::string rawRequest =
        "POST /login HTTP/1.1\r\n"
        "Content-Length: \t0 \t\r\n"
        "\r\n";

    auto result = HttpParser::parse(rawRequest);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->headers.at("content-length"), "0");
    EXPECT_TRUE(result->body.empty());
}

TEST(HttpParser, RejectsOversizedContentLength)
{
    const std::string rawRequest =
        "POST /login HTTP/1.1\r\n"
        "Content-Length: 999999999999999999999999999999\r\n"
        "\r\n";

    const auto result = HttpParser::parse(rawRequest);

    EXPECT_FALSE(result.has_value());
}

TEST(HttpParser, RejectsPlusPrefixedContentLength)
{
    const std::string body(42, 'a');

    const std::string rawRequest =
        "POST /login HTTP/1.1\r\n"
        "Content-Length: +42\r\n"
        "\r\n" +
        body;

    const auto result = HttpParser::parse(rawRequest);

    EXPECT_FALSE(result.has_value());
}



TEST(HttpParser, RejectsPathWithoutLeadingSlash)
{
    const std::string rawRequest =
        "GET index.html HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "\r\n";

    const auto result = HttpParser::parse(rawRequest);

    EXPECT_FALSE(result.has_value());
}

TEST(HttpParser, RejectsUnsupportedHttpVersion)
{
    const std::string rawRequest =
        "GET /index.html HTTP/2.0\r\n"
        "Host: localhost\r\n"
        "\r\n";

    const auto result = HttpParser::parse(rawRequest);

    EXPECT_FALSE(result.has_value());
}



TEST(HttpParser, AcceptsUnknownMethod)
{
    const std::string rawRequest =
        "BANANE /index.html HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "\r\n";

    const auto result = HttpParser::parse(rawRequest);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->method, "BANANE");
    EXPECT_EQ(result->path, "/index.html");
    EXPECT_EQ(result->version, "HTTP/1.1");
}

TEST(HttpParser,  RejectsDuplicateContentLength)
{
    const std::string rawRequest =
    "POST /HTTP/1.1\r\n"
    "Host: localhost\r\n"
    "Content-length: 5\r\n"
    "Content-length: 5\r\n"
    "\r\n"
    "abcde";

    const auto  result = HttpParser::parse(rawRequest);

    EXPECT_FALSE(result.has_value());
}

TEST(HttpParser, RejectsDuplicateHeadersCaseInsensitively)
{
    const std::string rawRequest =
    "GET /HTTP/1.1\r\n"
    "X-Request-Id: first\r\n"
    "x-request-id: second\r\n"
    "\r\n";

    const auto result = HttpParser::parse(rawRequest);
}


TEST(HttpParser, RejectsWhitespaceBeforeColon)
{
    const std::string rawRequest =
        "POST / HTTP/1.1\r\n"
        "Content-Length : 5\r\n"
        "\r\n"
        "abcde";

    const auto result = HttpParser::parse(rawRequest);

    EXPECT_FALSE(result.has_value());
}

TEST(HttpParser, RejectsUnterminatedHeaders)
{
    const std::string rawRequest =
        "GET / HTTP/1.1\r\n"
        "Host: localhost\r\n";

    const auto result = HttpParser::parse(rawRequest);

    EXPECT_FALSE(result.has_value());
}
