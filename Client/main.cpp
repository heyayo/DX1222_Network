#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT _WIN32_WINNT_WIN10
#define NOMINMAX
#pragma comment(lib,"Ws2_32.lib")

#include <winsock2.h>
#include <ws2tcpip.h>

#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <atomic>

#include "logging.hpp"

#define DEFAULT_PORT 25565
#define MESSAGE_BUFFER_LENGTH 1024

WSAData wsa_data;
SOCKET main_socket;

bool isRunning = true;

std::vector<std::string> split_string(const std::string& input)
{
    std::vector<std::string> output;
    std::string input_copy;
    size_t position;
    while ((position = input_copy.find(' ')) != std::string::npos)
    {
        output.emplace_back(input_copy.substr(0,position));
        input_copy.erase(0,position + 1);
    }
    return output;
}

void parse_input(const std::string& input)
{
    auto cut = split_string(input);
    
}

std::string username;

int main(int argc, char* argv[])
{
    TAG_MESSAGE("Test",SERVER);
    
    if (argc <= 1)
    {
        LOG_ERROR("Must Provide a Username");
        return EXIT_FAILURE;
    }
    
    LOG_INFO("Starting Up Client");
    int result = WSAStartup(MAKEWORD(2,2),&wsa_data);
    if (result != 0)
    {
        LOG_ERROR("WSAStartup Failure");
        return EXIT_FAILURE;
    }
    
    int port = DEFAULT_PORT;
    const char* server_address = "127.0.0.1";
    if (argc > 2)
    {
        port = std::stoi(argv[2]);
        LOG_INFO("Custom Port | " << port);
        if (port > std::numeric_limits<uint16_t>::max())
        {
            LOG_ERROR("Port exceeds the port range 0-65536");
            WSACleanup();
            return EXIT_FAILURE;
        }
        if (argc > 3)
            server_address = argv[3];
    }

    main_socket = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    if (INVALID_SOCKET == main_socket)
    {
        LOG_ERROR("Failed To Create Socket");
        WSACleanup();
        return EXIT_FAILURE;
    }
    LOG_INFO("Created Socket");

    sockaddr_in server_info{};
    server_info.sin_family = AF_INET;
    server_info.sin_port = htons(static_cast<uint16_t>(port));
    int conversion_result = inet_pton(AF_INET,server_address,&server_info.sin_addr); // Converts String to IP_Address
    if (!conversion_result)
    {
        LOG_ERROR("Bad IP Address Provided");
        closesocket(main_socket);
        WSACleanup();
        return EXIT_FAILURE;
    }

    result = connect(main_socket,reinterpret_cast<sockaddr*>(&server_info),sizeof(server_info));
    if (SOCKET_ERROR == result)
    {
        LOG_ERROR("Failed To Connect To Server");
        WSACleanup();
        return EXIT_FAILURE;
    }
    LOG_INFO("Connected To " << server_address);

    std::atomic<bool> thread_interrupt{};
    thread_interrupt.store(false);
    std::thread input_thread(
        [&]
        {
            while (!thread_interrupt)
            {
                std::string input;
                std::cin >> input;
                
            }
        });

    char message_buffer[MESSAGE_BUFFER_LENGTH];
    while (isRunning)
    {
        memset(message_buffer,'\0',MESSAGE_BUFFER_LENGTH);
        int receive_length = recv(main_socket,message_buffer,MESSAGE_BUFFER_LENGTH,0);
        if (!receive_length)
        {
            LOG_INFO("Server Closed On Their Side");
            break;
        }
        if (receive_length < 0)
        {
            LOG_ERROR("Receive Error");
            goto EXIT_POINT;
        }
        SERVER_MESSAGE(message_buffer);
    }

    // Interupt Thread and Join
    thread_interrupt.store(true);
    while (!input_thread.joinable())
        input_thread.join();

EXIT_POINT:
    LOG_INFO("Client Closing");
    closesocket(main_socket);
    WSACleanup();
    return 0;
}
