#include "HttpParser.h"


#include <sstream>
#include <string>
#include <cctype>
#include <algorithm>
#include <charconv>
#include <limits>

std::optional<HttpRequest> HttpParser::parse(const std::string& rawRequest) {

    std::istringstream requestStream(rawRequest);
    std::string line;

    HttpRequest request;

    if(!std::getline(requestStream, line))
    {
        return std::nullopt;
    }

    if (!line.empty() && line.back() == '\r')
    {
        line.pop_back();
    }

    std::istringstream requestLineStream(line);

    if(!(requestLineStream
         >> request.method
         >> request.path
         >> request.version))

    {
        return std::nullopt;
    }

    std::string extra;

    if(requestLineStream >> extra)
    {
      return std::nullopt;
    }

    if (request.path.empty() || request.path.front() != '/')
    {
        return std::nullopt;
    }

    if (request.version != "HTTP/1.1")
    {
        return std::nullopt;
    }

    bool endOfHeadersFound = false;

    while (std::getline(requestStream, line))
    {
        if(!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }

        if(line.empty())
        {
            endOfHeadersFound = true;
            break;
        }

        std::size_t colonPosition = line.find(':');

        if(colonPosition == std::string::npos)
        {
            return std::nullopt;
        }

        std::string key = line.substr(0, colonPosition);
        std::string value = line.substr(colonPosition + 1);

         if(key.empty())
        {
            return std::nullopt;
        }

        if (key.find_first_of(" \t") != std::string::npos)
        {
           return std::nullopt;
        }

        std::transform(
            key.begin(),
            key.end(),
            key.begin(),
            [](unsigned char c)
            {
                return std::tolower(c);
            }
        );

        std::size_t firstValueCharacter =  value.find_first_not_of(" \t");

        if (firstValueCharacter == std::string::npos)
        {
            value.clear();
        }
        else
        {
            const std::size_t lastValueCharacter =
               value.find_last_not_of(" \t");

            value = value.substr(
                firstValueCharacter,
                lastValueCharacter - firstValueCharacter + 1
            );
        }



        const bool inserted =
          request.headers.emplace(key, value).second;

        if (!inserted)
        {
            return std::nullopt;
        }
    }

    if (!endOfHeadersFound)
    {
        return std::nullopt;
    }

    std::ostringstream bodyStream;
    bodyStream << requestStream.rdbuf();

    request.body = bodyStream.str();

    const auto contentLengthHeader =
        request.headers.find("content-length");

    if (contentLengthHeader != request.headers.end())
    {
        const auto contentLength =
            parseContentLength(contentLengthHeader->second);

        if (!contentLength.has_value())
        {
            return std::nullopt;
        }

        if (request.body.size() != contentLength.value())
        {
            return std::nullopt;
        }
    }

    return request;



}

std::optional<std::size_t> HttpParser::parseContentLength(
    std::string_view value
)
{
    if (value.empty())
    {
        return std::nullopt;
    }

    std::size_t contentLength = 0;

    const char* const begin = value.data();
    const char* const end = begin + value.size();

    const auto [ptr, error] = std::from_chars(
        begin,
        end,
        contentLength
    );

    if (error != std::errc{} || ptr != end)
    {
        return std::nullopt;

    }

    return contentLength;
}

std::optional<std::size_t> HttpParser::determineRequestSize(
    std::string_view requestHead
)
{
    if (
        requestHead.size() < 4 ||
        requestHead.substr(requestHead.size() - 4) != "\r\n\r\n"
    )
    {
        return std::nullopt;
    }

    std::istringstream requestHeadStream{
        std::string(requestHead)
    };

    std::string line;

    if(!std::getline(requestHeadStream, line))
    {
        return std::nullopt;
    }

    std::optional<std::size_t> contentLength;

    while (std::getline(requestHeadStream, line))
    {
        if(!line.empty() && line.back() == '\r')
        {
            line.pop_back();

        }

        if (line.empty())
        {
            break;
        }

        const std::size_t colonPosition = line.find(':');


        if (colonPosition == std::string::npos)
        {
            return std::nullopt;
        }

        std::string key = line.substr(0, colonPosition);
        std::string value = line.substr(colonPosition + 1);

        if (key.empty())
        {
            return std::nullopt;
        }

        if (key.find_first_of(" \t") != std::string::npos)
        {
            return std::nullopt;
        }

        std::transform(
            key.begin(),
            key.end(),
            key.begin(),
            [](unsigned char character)
            {
                return std::tolower(character);
            }
        );

        if (key != "content-length")
        {
            continue;
        }

        if (contentLength.has_value())
        {
            return std::nullopt;
        }

        const std::size_t firstValueCharacter =
        value.find_first_not_of(" \t");

        if (firstValueCharacter == std::string::npos)
        {
            return std::nullopt;
        }

        const std::size_t lastValueCharacter =
        value.find_last_not_of(" \t");

        value = value.substr(
            firstValueCharacter,
            lastValueCharacter - firstValueCharacter + 1
        );

        contentLength = parseContentLength(value);

        if (!contentLength.has_value())
        {
            return std::nullopt;
        }
    }

    if(!contentLength.has_value())
    {
        return requestHead.size();
    }

    if(
        contentLength.value() >
        std::numeric_limits<std::size_t>::max() - requestHead.size()
    )
    {
        return std::nullopt;
    }

    return requestHead.size() + contentLength.value();
}
