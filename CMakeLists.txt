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

include(FetchContent)

FetchContent_Declare(
    googletest
     URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)

FetchContent_MakeAvailable(googletest)

enable_testing()

add_executable(unit_tests
   tests/test_parser.cpp
   src/HttpParser.cpp
)

target_include_directories(unit_tests PRIVATE include)

target_link_libraries(unit_tests GTest::gtest_main)

include(GoogleTest)

gtest_discover_tests(unit_tests)