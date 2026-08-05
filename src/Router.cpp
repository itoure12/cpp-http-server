#include "Router.h"

#include <stdexcept>
#include <utility>

bool Router::isKnownMethod(std::string_view method) noexcept
{
    return method == "GET" 
        || method == "POST"
        || method == "PUT"
        || method == "DELETE";
}

void Router::freeze() noexcept
{
    frozen_ = true;
}

void Router::addRoute(
     std::string method,
     std::string path,
     Handler handler
)
{
    if (frozen_)
    {
        throw std::logic_error(
            "cannot add a route after the router is frozen"
        );
    }

    if (!isKnownMethod(method))
    {
        throw std::invalid_argument(
            "unsupported HTTP method"
        );
    }

    if (path.empty() || path.front() != '/')
    {
        throw std::invalid_argument(
            "route path must start with '/'"
        );
    }

    if (!handler)
    {
        throw std::invalid_argument(
            "route handler cannot be empty"
        );
    }

    auto pathResult =
        routes_.try_emplace(std::move(path));

    auto pathIt = pathResult.first;
    
    auto routeResult = 
        pathIt->second.emplace(
            std::move(method),
            std::move(handler)
        );

    if (!routeResult.second)
    {
        throw std::logic_error(
            "route is already registered"
        );
    }    
}

HttpResponse Router::route(const HttpRequest& request) const 
{
    if(!isKnownMethod(request.method))
    {
        return HttpResponse(
            501,
            "Not Implemented\n"
        );
    }

    auto pathIt = routes_.find(request.path);

    if(pathIt == routes_.end())
    {
        return HttpResponse(
            404,
            "Not Found\n"
        );
    }

    auto methodIt =
         pathIt->second.find(request.method);

    if (methodIt == pathIt->second.end())
    {
       std::string allowedMethods;

       for (const auto& route : pathIt->second)
       {
        if (!allowedMethods.empty())
        {
           allowedMethods += ", "; 
        }

        allowedMethods += route.first;
        
       }

       HttpResponse response(
        405,
        "Method Not Allowed\n"
       );

       response.setHeader(
        "Allow",
        allowedMethods
       );

       return response;
       
       
    }   
    
    return methodIt->second(request);
}