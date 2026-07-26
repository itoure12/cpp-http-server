#include "HttpParser.h"


#include <sstream>
#include <string>
#include <cctype>
#include <algorithm>

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

    while (std::getline(requestStream, line))
    {
        if(!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }

        if(line.empty())
        {
            break;
        }

        std::size_t colonPosition = line.find(':');

        if(colonPosition == std::string::npos)
        {
            return std::nullopt;
        }

        std::string key = line.substr(0, colonPosition);
        std::string value = line.substr(colonPosition + 1);

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
            value = value.substr(firstValueCharacter);
        }

        if(key.empty())
        {
            return std::nullopt;
        }

        request.headers[key] = value;
    }

    std::ostringstream bodyStream;
    bodyStream << requestStream.rdbuf();

    request.body = bodyStream.str();

    return request;



}