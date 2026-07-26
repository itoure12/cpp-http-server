#pragma once 

#include <optional>
#include "HttpRequest.h"
#include <string>

class HttpParser
{
    public:
       static std::optional<HttpRequest> parse(
        const std::string& rawRequest
    );
};