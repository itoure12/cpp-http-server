#include "HttpServer.h"
#include "HttpResponse.h"
#include "Router.h"
#include "HttpRequest.h"
#include <utility>
int main() {

    Router router;

    router.addRoute(
        "GET",
        "/",
        [](const HttpRequest&)
        {
            HttpResponse response(
                200,
                "Hello\n"
            );

            response.setHeader(
                "Content-Type",
                "text/plain"
            );

            return response;
        }
    );

    HttpServer server(
        8080,
        std::move(router)
    );


    server.start();

    return 0;


}