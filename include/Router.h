#pragma once

#include "HttpRequest.h"
#include "HttpResponse.h"

#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <unordered_map>

class Router 
{
    public:
       using Handler = 
       std::function<HttpResponse(const HttpRequest&)>;

       void addRoute(
        std::string method,
        std::string path,
        Handler handler
       );

       [[nodiscard]]
       HttpResponse route(const HttpRequest& request) const;

       void freeze() noexcept;

    private:
         using MethodTable = std::map<std::string, Handler>;
         std::unordered_map<std::string, MethodTable> routes_;
         bool frozen_ = false;
         
         static bool isKnownMethod(
            std::string_view method
         ) noexcept;
};