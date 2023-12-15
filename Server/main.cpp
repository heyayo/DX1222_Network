#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT _WIN32_WINNT_WIN10
#define NOMINMAX
#pragma comment(lib,"Ws2_32.lib")

#include <winsock2.h>
#include <ws2tcpip.h>

#include <iostream>
#include <map>
#include <string>
#include <vector>
#include <limits>
#include <cstdint>

#include "logging.hpp"

#define TCP_BACKLOG 10
#define DEFAULT_PORT 25565
#define MESSAGE_BUFFER_LENGTH 1024

struct ClientData
{
    sockaddr_in network;
    std::string username;
};

WSAData wsa_data;
SOCKET m_listener_socket;
fd_set m_sockets;
std::map<SOCKET,ClientData> m_client_data;

bool isRunning = true;
char message_buffer[MESSAGE_BUFFER_LENGTH];

void print_clientdata(ClientData data)
{
    char ip_address[INET_ADDRSTRLEN];
    inet_ntop(AF_INET,&data.network.sin_addr,ip_address,INET_ADDRSTRLEN);
    LOG_INFO("IP Address: " << ip_address);
    LOG_INFO("Port: " << ntohs(data.network.sin_port));
    LOG_INFO("Username: " << data.username);
}

int main(int argc, char* argv[])
{
    LOG_INFO("Starting Up Server");
    int result = WSAStartup(MAKEWORD(2,2),&wsa_data);
    if (result != 0)
    {
        LOG_ERROR("WSAStartup Failure");
        return EXIT_FAILURE;
    }

    int port = DEFAULT_PORT;
    if (argc > 1)
    {
        port = std::stoi(argv[1]);
        LOG_INFO("Custom Port | " << port);
        if (port > std::numeric_limits<uint16_t>::max())
        {
            LOG_ERROR("Port exceeds the port range 0-65536");
            WSACleanup();
            return EXIT_FAILURE;
        }
    }
    // LOG_INFO("DEBUG PORT CHECKING HTONS " << htons(static_cast<uint16_t>(port)));
    // LOG_INFO("DEBUG PORT CHECKING CASTING " << static_cast<uint16_t>(port));
    // LOG_INFO("DEBUG PORT CHECKING NO CAST " << (port));
    // LOG_INFO("DEBUG PORT CHECKING NO CAST HTONS " << htons(port));
    // LOG_INFO("DEBUG PORT CHECKING NTONS FROM HTONS NO CAST " << ntohs(htons(port)));
    // LOG_INFO("DEBUG PORT CHECKING NTONS FROM HTONS " << ntohs(htons(static_cast<uint16_t>(port))));

    /*
     * AF_INET | IPv4
     * SOCK_STREAM | Stream Data
     * IPPROTO_TCP | TCP Protocol
     */
    m_listener_socket = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    if (INVALID_SOCKET == m_listener_socket)
    {
        LOG_ERROR("Failed To Create Listener Socket");
        WSACleanup();
        return EXIT_FAILURE;
    }
    LOG_INFO("Created Listener Socket");

    sockaddr_in server_info{};
    server_info.sin_family = AF_INET;
    server_info.sin_port = htons(static_cast<uint16_t>(port));
    server_info.sin_addr.s_addr = htonl(INADDR_ANY);
    result = bind(m_listener_socket,reinterpret_cast<sockaddr*>(&server_info),sizeof(server_info));
    if (SOCKET_ERROR == result)
    {
        LOG_ERROR("Failed To Bind Listener Socket");
        WSACleanup();
        return EXIT_FAILURE;
    }
    LOG_INFO("Binding on port | " << port);

    result = listen(m_listener_socket,TCP_BACKLOG);
    if (SOCKET_ERROR == result)
    {
        LOG_ERROR("Failed To Listen on Socket");
        WSACleanup();
        return EXIT_FAILURE;
    }
    LOG_INFO("Listening On Port | " << port);

    TIMEVAL timeout;
    timeout.tv_sec = 1;

    FD_ZERO(&m_sockets);
    FD_SET(m_listener_socket,&m_sockets);

    int exit_code = EXIT_SUCCESS;
    while (isRunning)
    {
        fd_set temp_set = m_sockets;
        // Check for Incoming Connections on the listener socket
        int check = select(0,&temp_set,nullptr,nullptr,&timeout);
        if (SOCKET_ERROR == check)
        {
            LOG_ERROR("Failure with select() | ERRORCODE: " << WSAGetLastError());
            exit_code = EXIT_FAILURE;
            goto EXIT_POINT;
        }
        if (0 == check)
        {
            continue;
        }
        if (0 > check)
        {
            LOG_ERROR("Non-Critical Failure with select() on Listener Socket");
        }

        for (u_int i = 0; i < m_sockets.fd_count; ++i)
        {
            const SOCKET client = m_sockets.fd_array[i];
            if (client != m_listener_socket)
            {
                // If a client has a message
                memset(message_buffer,'\0',MESSAGE_BUFFER_LENGTH); // Wipe the buffer with (End of File)s
                int msg_len = recv(client,message_buffer,MESSAGE_BUFFER_LENGTH,0);
                if (0 ==msg_len)
                {
                    LOG_INFO("Client Has Disconnected");
                    print_clientdata(m_client_data.at(client));
                    m_client_data.erase(client);
                    closesocket(client);
                    FD_CLR(client,&m_sockets);
                    continue;
                }
                if (msg_len < 0)
                {
                    LOG_WARNING("Failure when receiving from client");
                    print_clientdata(m_client_data.at(client));
                    continue;
                }

                // Print out what the client sent over
                LOG_INFO("Received Client Data");
                LOG_INFO(message_buffer);
                continue;
            }
            
            // If the listener has incoming connections
            sockaddr_in incoming{};
            int incoming_size = sizeof(incoming);
            SOCKET new_client = accept(m_listener_socket,reinterpret_cast<sockaddr*>(&incoming),&incoming_size);
            if (INVALID_SOCKET == new_client)
            {
                LOG_WARNING("Failed To Accept A Client");
                continue;
            }
            char username_buffer[64]{'\0'};
            recv(new_client,username_buffer,64,0);
            FD_SET(new_client,&m_sockets); // Add Client to the list of clients
            ClientData new_client_data{incoming,username_buffer};
            m_client_data.insert(std::make_pair(new_client,new_client_data));
            print_clientdata(new_client_data);
        }
    }

    LOG_INFO("Server Closing");

EXIT_POINT:
    for (u_int i = 0; i < m_sockets.fd_count; ++i)
        closesocket(m_sockets.fd_array[i]);
    closesocket(m_listener_socket);
    WSACleanup();
    return exit_code;
}
