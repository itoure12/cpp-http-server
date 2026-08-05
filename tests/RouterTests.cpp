#include "HttpRequest.h"
#include "HttpResponse.h"
#include "Router.h"
#include <gtest/gtest.h>
#include <stdexcept>
#include <string>

namespace
{
    Router::Handler makeHandler()
    {
        return [](const HttpRequest&)
        {
            return HttpResponse(200, "OK\n");
        };
    }
}

TEST(RouterTest, AcceptsValidRoute)
{
    Router router;

    EXPECT_NO_THROW(
        router.addRoute(
            "GET",
            "/health",
            makeHandler()
        )
    );
}

TEST(RouterTest, RejectsUnknownMethod)
{
    Router router;

    EXPECT_THROW(
        router.addRoute(
            "PATCH",
            "/health",
            makeHandler()
        ),
        std::invalid_argument
    );
}

TEST(RouterTest, RejectsEmptyPath)
{
    Router router;

    EXPECT_THROW(
        router.addRoute(
            "GET",
            "",
            makeHandler()
        ),
        std::invalid_argument
    );
}

TEST(RouterTest, RejectsPathWithoutLeadingSlash)
{
    Router router;

    EXPECT_THROW(
        router.addRoute(
            "GET",
            "health",
            makeHandler()
        ),
        std::invalid_argument
    );
}

TEST(RouterTest, RejectsEmptyHandler)
{
    Router router;

    Router::Handler emptyHandler;

    EXPECT_THROW(
        router.addRoute(
            "GET",
            "/health",
            emptyHandler
        ),
        std::invalid_argument
    );
}

TEST(RouterTest, RejectsDuplicateRoute)
{
    Router router;

    router.addRoute(
        "GET",
        "/health",
        makeHandler()
    );

    EXPECT_THROW(
        router.addRoute(
            "GET",
            "/health",
            makeHandler()
        ),
        std::logic_error
    );
}

TEST(RouterTest, AcceptsDifferentMethodsForSamePath)
{
    Router router;

    EXPECT_NO_THROW(
        router.addRoute(
            "GET",
            "/users",
            makeHandler()
        )
    );

    EXPECT_NO_THROW(
        router.addRoute(
            "POST",
            "/users",
            makeHandler()
        )
    );
}

TEST(RouterTest, AcceptsSameMethodForDifferentPaths)
{
    Router router;

    EXPECT_NO_THROW(
        router.addRoute(
            "GET",
            "/users",
            makeHandler()
        )
    );

    EXPECT_NO_THROW(
        router.addRoute(
            "GET",
            "/health",
            makeHandler()
        )
    );
}

TEST(RouterTest, RejectsRouteAddedAfterFreeze)
{
    Router router;

    router.freeze();

    EXPECT_THROW(
        router.addRoute(
            "GET",
            "/health",
            makeHandler()
        ),
        std::logic_error
    );
}

TEST(RouterTest, Returns501ForUnknownMethod)
{
    Router router;

    router.addRoute(
        "GET",
        "/health",
        makeHandler()
    );

    HttpRequest request;
    request.method = "PATCH";
    request.path = "/health";

    HttpResponse response = router.route(request);

    std::string serialized = response.serialize();

    EXPECT_NE(
        serialized.find(
            "HTTP/1.1 501 Not Implemented\r\n"
        ),
        std::string::npos
    );
}

TEST(RouterTest, Returns404ForUnknownPath)
{
    Router router;

    HttpRequest request;
    request.method = "GET";
    request.path = "/unknown";

    HttpResponse response = router.route(request);

    std::string serialized = response.serialize();

    EXPECT_NE(
        serialized.find(
            "HTTP/1.1 404 Not Found\r\n"
        ),
        std::string::npos
    );
}

TEST(RouterTest, Returns405WithAllowHeaderForUnsupportedMethod)
{
    Router router;

    router.addRoute(
        "GET",
        "/users",
        makeHandler()
    );

    router.addRoute(
        "POST",
        "/users",
        makeHandler()
    );

    HttpRequest request;
    request.method = "DELETE";
    request.path = "/users";

    HttpResponse response = router.route(request);

    std::string serialized = response.serialize();

    EXPECT_NE(
        serialized.find(
            "HTTP/1.1 405 Method Not Allowed\r\n"
        ),
        std::string::npos
    );

    EXPECT_NE(
        serialized.find(
            "Allow: GET, POST\r\n"
        ),
        std::string::npos
    );
}

TEST(RouterTest, ExecutesMatchingHandler)
{
    Router router;

    bool handlerWasCalled = false;

    router.addRoute(
        "GET",
        "/health",
        [&handlerWasCalled](const HttpRequest&)
        {
            handlerWasCalled = true;

            return HttpResponse(
                200,
                "handler result\n"
            );
        }
    );

    HttpRequest request;
    request.method = "GET";
    request.path = "/health";

    HttpResponse response = router.route(request);

    std::string serialized = response.serialize();

    EXPECT_TRUE(handlerWasCalled);

    EXPECT_NE(
        serialized.find(
            "HTTP/1.1 200 OK\r\n"
        ),
        std::string::npos
    );

    EXPECT_NE(
        serialized.find("handler result\n"),
        std::string::npos
    );
}

TEST(RouterTest, Returns501ForUnknownMethodEvenWhenPathIsUnknown)
{
    Router router;

    HttpRequest request;
    request.method = "PATCH";
    request.path = "/unknown";

    const HttpResponse response =
        router.route(request);

    const std::string serialized =
        response.serialize();

    EXPECT_NE(
        serialized.find(
            "HTTP/1.1 501 Not Implemented\r\n"
        ),
        std::string::npos
    );
}