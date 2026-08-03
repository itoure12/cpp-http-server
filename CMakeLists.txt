cmake_minimum_required(VERSION 3.14)
project(http_server LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)

add_library(http_server_lib STATIC
    src/Config.cpp
    src/HttpParser.cpp
    src/HttpRequest.cpp
    src/HttpResponse.cpp
    src/HttpServer.cpp
    src/Logger.cpp
    src/Router.cpp
    src/SignalHandler.cpp
    src/ThreadPool.cpp
)

target_include_directories(http_server_lib
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_options(http_server_lib
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
)

target_link_libraries(http_server_lib
    PUBLIC
        Threads::Threads
)

add_executable(http_server
    src/main.cpp
)

target_link_libraries(http_server
    PRIVATE
        http_server_lib
)

target_compile_options(http_server
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
)
include(FetchContent)

FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)


FetchContent_MakeAvailable(googletest)

enable_testing()

add_executable(unit_tests
   tests/test_parser.cpp
   tests/HttpResponseTest.cpp
)

target_link_libraries(unit_tests
    PRIVATE
        http_server_lib
        GTest::gtest_main
)

target_compile_options(unit_tests
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
)

include(GoogleTest)

gtest_discover_tests(unit_tests)
