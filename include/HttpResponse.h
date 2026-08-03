#pragma once 

#include <string>
#include <unordered_map>

class HttpResponse
{
    public:
       explicit HttpResponse(int statusCode, std::string body = "");

       void setHeader(std::string name, std::string  value);

       std::string serialize() const;

    private:
       struct Header
       {
        std::string displayName;
        std::string value;
       };
       std::string version_ = "HTTP/1.1";
       int statusCode_;
       std::string reasonPhrase_;
       
       std::unordered_map<std::string, Header> headers_;
       std::string body_;

       static std::string reasonPhraseFor(int statusCode);
       static std::string normalizeHeaderName(std::string name);
       static std::string canonicalizeHeaderName(
        const std::string& normalizedName);
};  