#include "HttpResponse.h"

#include <stdexcept>
#include <utility>
#include <cctype>
#include <sstream>

HttpResponse::HttpResponse(int statusCode, std::string body)
    : statusCode_(statusCode),
      body_(std::move( body))
{
    if (statusCode_ < 100 || statusCode_ > 599)
    {
        throw std::invalid_argument(
            "HTTP status code must be between 100 and 599");
    }

    reasonPhrase_ = reasonPhraseFor(statusCode_);

}    

std::string HttpResponse::reasonPhraseFor(int statusCode)
{
    switch (statusCode)
    {
        case 200:
            return "OK";
        
          case 400:
            return "Bad Request";

        case 404:
            return "Not Found";

        case 405:
            return "Method Not Allowed";

        case 413:
            return "Content Too Large";

        case 500:
            return "Internal Server Error";

        case 501:
            return "Not Implemented";

        default:
            return "Unknown";
    }
}

std::string HttpResponse::normalizeHeaderName(std::string name)
{
    for(char& character : name)
    {
        character = static_cast<char>(
            std::tolower(static_cast<unsigned char>(character)));
    }
    
    return name;
}

std::string HttpResponse::canonicalizeHeaderName(
    const std::string& normalizedName)
{
    std::string displayName = normalizedName;
    bool capitalizeNext = true;

    for (char& character : displayName)
    {
        if (character == '-')
        {
            capitalizeNext = true;
        }

        else if (capitalizeNext)
        {
            character = static_cast<char>(
                std::toupper(static_cast<unsigned char>(character)));

            capitalizeNext = false;    
        }
    }

    return displayName;
}
void HttpResponse::setHeader(std::string name, std::string value)
{
    std::string normalizedName = normalizeHeaderName(std::move(name));

    if (normalizedName == "content-length")
    {
        throw std::invalid_argument(
            "Content-Length is managed automatically");
    }

    headers_.insert_or_assign(
        normalizedName,
        Header{
            canonicalizeHeaderName(normalizedName),
            std::move(value)
        });
}

std::string HttpResponse::serialize() const 
{
    std::ostringstream output;

    output << version_ 
           << ' '
           << statusCode_
           << ' '
           << reasonPhrase_
           << "\r\n";

    for (const auto& entry : headers_)
    {
       const Header& header = entry.second;
       
       output << header.displayName
              << ": "  
              << header.value
              << "\r\n";
    }   
    
    output << "Content-Length: "
           << body_.size()
           << "\r\n";

    output << "\r\n";
    output << body_;
    
    return output.str();

}
