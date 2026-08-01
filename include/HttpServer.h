#pragma once

#include <cstdint>
#include <cstddef>

class HttpServer
{
    public:
         explicit HttpServer(std::uint16_t port);
         ~HttpServer();

         HttpServer(const HttpServer&) = delete;
         HttpServer& operator=(const HttpServer&) = delete;

         void start();

    private:

        static constexpr std::size_t MAX_REQUEST_HEAD_SIZE = 16 * 1024;
        static constexpr std::size_t MAX_BODY_SIZE = 1024 * 1024;


        std::uint16_t port_;
        int serverSocket_;

        bool createSocket();
        bool configureSocket();
        bool bindSocket();
        bool listenForConnections();
        void acceptLoop();
        void handleClient(int clientSocket);


};
