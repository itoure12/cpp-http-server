#pragma once

#include <cstdint>

class HttpServer
{
    public:
         explicit HttpServer(std::uint16_t port);
         ~HttpServer();

         HttpServer(const HttpServer&) = delete;
         HttpServer& operator=(const HttpServer&) = delete;

         void start();

    private:
        std::uint16_t port_;
        int serverSocket_;

        bool createSocket();
        bool configureSocket();
        bool bindSocket();
        bool listenForConnections();
        void acceptLoop();
        void handleClient(int clientSocket);


};
