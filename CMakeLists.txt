cmake_minimum_required(VERSION 3.14)
project(http_server)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(http_server
    src/main.cpp
    src/HttpServer.cpp
    src/HttpParser.cpp
    src/HttpRequest.cpp
    src/HttpResponse.cpp
)

target_include_directories(http_server PRIVATE include)

target_compile_options(http_server PRIVATE -Wall -Wextra)