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
    std::string rawRequest =
    "POST /login HTTP/1.1\r\n"
    "Content-Type: application/json\r\n"
    "Content-Length: 38\r\n"
    "\r\n"
    R"({"username":"idriss","password":"1234"})";

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
        R"({"username":"idriss","password":"1234"})"
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
