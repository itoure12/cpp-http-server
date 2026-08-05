#include "HttpParser.h"
#include "HttpServer.h"

#include <iostream>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <array>
#include <string>
#include "HttpResponse.h"
#include <cerrno>
#include <string_view>
#include <optional>
#include <utility>

HttpServer::HttpServer(std::uint16_t port, Router router)
     :port_(port), serverSocket_(-1), router_(std::move(router))
     {
        router_.freeze();
     }

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
   std::string rawRequest;
   std::optional<std::size_t> expectedRequestSize;

 while (true)
    {

       const ssize_t bytesReceived = recv(
            clientSocket,
            buffer.data(),
            buffer.size(),
            0
        );

        if (bytesReceived == -1)
        {
            if(errno == EINTR)
            {
                continue;
            }

            perror("recv");
            close(clientSocket);
            return;
        }

        if (bytesReceived == 0)
        {
            std::cout << "Client disconnected.\n";
            close(clientSocket);
            return;
        }

        rawRequest.append(
            buffer.data(),
            static_cast<std::size_t>(bytesReceived)
        );
        
        if (!expectedRequestSize.has_value()) 
        { 

            const std::size_t requestHeadEnd =
                rawRequest.find("\r\n\r\n");

            if (requestHeadEnd == std::string::npos)
            {
                if (rawRequest.size() > MAX_REQUEST_HEAD_SIZE)
                {
                    std::cout << "HTTP headers are too large.\n";
                    close(clientSocket);
                    return;
                }

                continue;
            }

            const std::size_t requestHeadSize = requestHeadEnd + 4;

            if (requestHeadSize > MAX_REQUEST_HEAD_SIZE)
            {
                std::cout << "HTTP headers are too large.\n";
                close(clientSocket);
                return;
            }

            const auto determinedSize  =
            HttpParser::determineRequestSize(
                std::string_view(
                    rawRequest.data(),
                    requestHeadSize
                )
            );

            if (!determinedSize.has_value())
            {
                std::cout << "Invalid HTTP request head.\n";
                close(clientSocket);
                return;
            }

            const std::size_t expectedBodySize =
                  determinedSize.value() - requestHeadSize;

            if (expectedBodySize > MAX_BODY_SIZE)
            {
                std::cout << "HTTP request body is too large.\n";
                close(clientSocket);
                return;
            }

            expectedRequestSize = determinedSize.value();
        }

      

        if(rawRequest.size() < expectedRequestSize.value())
        {
            continue;
        }

         if(rawRequest.size() > expectedRequestSize.value())
        {
            std::cout <<"Unexpected data after HTTP request.\n";
            close(clientSocket);
            return;
        }
        break;
    }


  const auto request = HttpParser::parse(rawRequest);

        if (!request.has_value())
        {
            std::cout << "Invalid HTTP request.\n";
            close(clientSocket);
            return;
        }

        std::cout << "Received "
                  << request->method
                  << " "
                  << request->path
                  << '\n';


        HttpResponse response = router_.route(*request);

        
        response.setHeader("Connection", "close");

        const std::string rawResponse = response.serialize();

        if(!sendAll(clientSocket, rawResponse))
        {
            close(clientSocket);
            return;
        }
        close(clientSocket);


}

bool HttpServer::sendAll(
    int clientSocket,
    std::string_view data)
    
{
    std::size_t totalBytesSent = 0;

    while (totalBytesSent < data.size())
    {
        const ssize_t bytesSent = send(
            clientSocket,
            data.data() + totalBytesSent,
            data.size() - totalBytesSent,
            MSG_NOSIGNAL
        );

        if (bytesSent == -1)
        {
            if (errno == EINTR)
            {
                continue;
            }

            perror("send");
            return false;
        }

        totalBytesSent += static_cast<std::size_t>(bytesSent);
    }

   return true;
}     
