#include "HttpResponse.h"

#include <gtest/gtest.h>
#include <stdexcept>

TEST(HttpResponseTest, SerializesBasicOkResponse)
{
    HttpResponse response(200, "Hello");

    EXPECT_EQ(
        response.serialize(),
        "HTTP/1.1 200 OK\r\n"
        "Content-Length: 5\r\n"
        "\r\n"
        "Hello"
    );
}


TEST(HttpResponseTest, RejectsStatusCodeOutsideHttpRange)
{
    EXPECT_THROW(
        HttpResponse(99, ""),
        std::invalid_argument
    );

    EXPECT_THROW(
        HttpResponse(600, ""),
        std::invalid_argument
    );
}

TEST(HttpResponseTest, UsesUnknownReasonPhraseForUnrecognizedStatusCode)
{
    HttpResponse response(299, "");

    EXPECT_EQ(
        response.serialize(),
        "HTTP/1.1 299 Unknown\r\n"
        "Content-Length: 0\r\n"
        "\r\n"
    );
}

TEST(HttpResponseTest, RejectsManuallySetContentLength)
{
    HttpResponse response(200, "Hello");

    EXPECT_THROW(
        response.setHeader("Content-Length", "999"),
        std::invalid_argument
    );

    EXPECT_THROW(
        response.setHeader("content-length", "999"),
        std::invalid_argument
    );

    EXPECT_THROW(
        response.setHeader("CONTENT-LENGTH", "999"),
        std::invalid_argument
    );
}

TEST(HttpResponseTest, ReplacesHeaderCaseInsensitively)
{
    HttpResponse response(200, "Hello");

    response.setHeader("Content-Type", "text/plain");
    response.setHeader("content-type", "application/json");

    EXPECT_EQ(
        response.serialize(),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: 5\r\n"
        "\r\n"
        "Hello"
    );
}