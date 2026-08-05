#pragma once

#include "Router.h"

#include <cstdint>
#include <cstddef>
#include <string_view>


class HttpServer
{
    public:
          HttpServer(
            std::uint16_t port,
            Router router
          );
          
         ~HttpServer();

         HttpServer(const HttpServer&) = delete;
         HttpServer& operator=(const HttpServer&) = delete;

         void start();

    private:

        static constexpr std::size_t MAX_REQUEST_HEAD_SIZE = 16 * 1024;
        static constexpr std::size_t MAX_BODY_SIZE = 1024 * 1024;


        std::uint16_t port_;
        int serverSocket_;
         Router router_;


        bool createSocket();
        bool configureSocket();
        bool bindSocket();
        bool listenForConnections();
        bool sendAll(int clientSocket, std::string_view data);
        void acceptLoop();
        void handleClient(int clientSocket);



};
