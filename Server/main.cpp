#include "os_diff.hpp"

#include <iostream>
#include <map>
#include <string>
#include <vector>
#include <queue>
#include <limits>
#include <algorithm>
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

struct ChatRoom
{
    std::string name;
    std::map<SOCKET,ClientData> clients;
};

typedef bool ToAddClient;

SOCKET m_listener_socket;
fd_set m_sockets;
std::vector<SOCKET> m_clients;
std::map<SOCKET,ClientData> m_client_data;
std::queue<std::pair<SOCKET,ToAddClient>> m_events;

std::map<std::string,ChatRoom> m_rooms;

bool isRunning = true;
char message_buffer[MESSAGE_BUFFER_LENGTH];

void print_clientdata(const ClientData& data)
{
    char ip_address[INET_ADDRSTRLEN];
    inet_ntop(AF_INET,&data.network.sin_addr,ip_address,INET_ADDRSTRLEN);
    SERVER_MESSAGE("IP Address: " << ip_address);
    SERVER_MESSAGE("Port: " << ntohs(data.network.sin_port));
    SERVER_MESSAGE("Username: " << data.username);
}

void add_client(SOCKET client, const ClientData& data)
{
    m_client_data.insert({client,data});
    FD_SET(client,&m_sockets);
    m_events.emplace(client,true);
}

void remove_client(SOCKET client)
{
    m_client_data.erase(client);
    FD_CLR(client,&m_sockets);
    CLOSE_SOCKET(client);
    m_events.emplace(client,false);
}

int main(const int argc, char* argv[])
{
    m_rooms.insert(std::make_pair(std::string{"home"},ChatRoom{})); // Default Room
    
    int result{};
    LOG_INFO("Starting Up Server");
    WINSOCK_LINK;

    int port = DEFAULT_PORT;
    if (argc > 1)
    {
        port = std::stoi(argv[1]);
        LOG_INFO("Custom Port | " << port);
        if (port > std::numeric_limits<uint16_t>::max())
        {
            LOG_ERROR("Port exceeds the port range 0-65536");
            WINSOCK_CLEANUP;
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
    if (0 > m_listener_socket)
    {
        LOG_ERROR("Failed To Create Listener Socket");
        WINSOCK_CLEANUP;
        return EXIT_FAILURE;
    }
    LOG_INFO("Created Listener Socket");

    sockaddr_in server_info{};
    server_info.sin_family = AF_INET;
    server_info.sin_port = htons(static_cast<uint16_t>(port));
    server_info.sin_addr.s_addr = htonl(INADDR_ANY);
    result = bind(m_listener_socket,reinterpret_cast<sockaddr*>(&server_info),sizeof(server_info));
    if (0 > result)
    {
        LOG_ERROR("Failed To Bind Listener Socket");
        CLOSE_SOCKET(m_listener_socket);
        WINSOCK_CLEANUP;
        return EXIT_FAILURE;
    }
    LOG_INFO("Binding on port | " << port);

    result = listen(m_listener_socket,TCP_BACKLOG);
    if (0 > result)
    {
        LOG_ERROR("Failed To Listen on Socket");
        CLOSE_SOCKET(m_listener_socket);
        WINSOCK_CLEANUP
        return EXIT_FAILURE;
    }
    LOG_INFO("Listening On Port | " << port);


    FD_ZERO(&m_sockets);
    FD_SET(m_listener_socket,&m_sockets);

    int exit_code = EXIT_SUCCESS;
    while (isRunning)
    {
        timeval timeout{};
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        fd_set temp_set = m_sockets;
        // Check for Incoming Connections on the listener socket
        const int check = select(0,&temp_set,nullptr,nullptr,&timeout);
        if (0 > check)
        {
            LOG_ERROR("Failure with select() | ERRORCODE: " << GET_LAST_ERROR);
            exit_code = EXIT_FAILURE;
            goto EXIT_POINT;
        }
        if (0 == check) // select() timeout
            continue;

        for (const auto client : m_clients)
        {
            if (client != m_listener_socket)
            {
                // If a client has a message
                memset(message_buffer,'\0',MESSAGE_BUFFER_LENGTH); // Wipe the buffer with (End of File)s
                const ssize_t msg_len = recv(client,message_buffer,MESSAGE_BUFFER_LENGTH-1,0);
                if (0 == msg_len)
                {
                    SERVER_MESSAGE("Client Has Disconnected");
                    print_clientdata(m_client_data.at(client));
                    remove_client(client);
                    continue;
                }
                if (0 > msg_len)
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
            sockaddr_length incoming_size = sizeof(incoming);
            const SOCKET new_client = accept(m_listener_socket,reinterpret_cast<sockaddr*>(&incoming),&incoming_size);
            if (0 > new_client)
            {
                LOG_WARNING("Failed To Accept A Client");
                continue;
            }
            char username_buffer[64]{'\0'};
            recv(new_client,username_buffer,63,0);
            ClientData new_client_data{incoming,username_buffer};
            SERVER_MESSAGE("Client Has Connected");
            print_clientdata(new_client_data);
            add_client(new_client,new_client_data);
        }

        while (!m_events.empty())
        {
            const auto& event = m_events.front();
            if (event.second)
                m_clients.emplace_back(event.first);
            else
            {
                const auto iter = std::find(m_clients.begin(),m_clients.end(),event.first);
                m_clients.erase(iter);
            }
            m_events.pop();
        }
    }

    LOG_INFO("Server Closing");

EXIT_POINT:
    for (const auto sock : m_clients)
        CLOSE_SOCKET(sock);
    CLOSE_SOCKET(m_listener_socket);
    WINSOCK_CLEANUP;
    return exit_code;
}
