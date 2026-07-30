#include "HttpServer.h"

#include <iostream>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <array>
#include <string>

HttpServer::HttpServer(std::uint16_t port)
     :port_(port), serverSocket_(-1)
     {}

HttpServer::~HttpServer()
{
    if (serverSocket_ >= 0)
    {
        close(serverSocket_);
    }
}

void HttpServer::start()
{
    if (!createSocket())
        return;


    if (!configureSocket())
       return;

    if (!bindSocket())
        return;

    if (!listenForConnections())
        return;

    std::cout <<  "HTTP server listening on port "
              << port_
              << '\n';


    acceptLoop();
}

bool HttpServer::createSocket()
{
    serverSocket_ = socket(AF_INET, SOCK_STREAM, 0);

    if (serverSocket_ == -1)
    {
        perror("socket");
        return false;
    }

    return true;

}

bool HttpServer::configureSocket()
{
    int option = 1;

    if(setsockopt(
           serverSocket_,
           SOL_SOCKET,
           SO_REUSEADDR,
           &option,
           sizeof(option)) == -1)
    {
        perror("setsockopt");
        return false;
    }

    return true;
}

bool HttpServer::bindSocket()
{

    sockaddr_in serverAddress{};

    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    serverAddress.sin_port = htons(port_);

    if(bind(
        serverSocket_,
        reinterpret_cast<sockaddr*>(&serverAddress),
        sizeof(serverAddress)) == -1)
    {
        perror("bind");
        return false;
    }

    return true;
}

bool HttpServer::listenForConnections()
{
    constexpr int backlog = 10;

    if(listen(serverSocket_, backlog) == -1)
    {
        perror("listen");
        return false;
    }

    return true;

}

void HttpServer::acceptLoop()
{
    while (true)
    {
        int clientSocket = accept(
            serverSocket_,
            nullptr,
            nullptr);

        if (clientSocket == -1)
        {
            perror("accept");
            continue;
        }

        std::cout << "New client connected.\n";

        handleClient(clientSocket);
    }
}

void HttpServer::handleClient(int clientSocket)
{
   std::array<char, 4096> buffer{};

   const ssize_t bytesReceived = recv(
    clientSocket,
    buffer.data(),
    buffer.size(),
    0
   );

   if(bytesReceived == -1)
   {
    perror("recv");
    close(clientSocket);
    return;
   }

   if (bytesReceived == 0)
   {
    std::cout <<"Client disconneted.\n";
    close(clientSocket);
    return;
   }

   const std::string rawRequest(
    buffer.data(),
    static_cast<std::size_t>(bytesReceived)
   );

   std::cout << "Received request:\n"
             << rawRequest
             << '\n';

    close(clientSocket);
}