#pragma once 

#include <string>
#include <unordered_map>

struct HttpResponse
{
    std::string version;

    int statusCode;
    std::string statusMessage;

    std::unordered_map<std::string, std::string> headers;

    std::string body;
};