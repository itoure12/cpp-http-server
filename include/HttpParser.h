#pragma once

#include <optional>
#include "HttpRequest.h"
#include <string>
#include <string_view>
#include <cstddef>

class HttpParser
{
    public:
       static std::optional<HttpRequest> parse(
        const std::string& rawRequest
    );

    private:
         static std::optional<std::size_t> parseContentLength(
            std::string_view value
         );
};