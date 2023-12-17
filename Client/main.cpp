#include "os_diff.hpp"
#include "logging.hpp"
#include "packet_sender.hpp"

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <thread>
#include <atomic>
#include <sstream>

#define DEFAULT_PORT 25565

typedef void(*Command)(const std::vector<std::string>&);
struct CommandEntry
{
    Command command;
    std::string usage;
};

SOCKET main_socket;
std::string username;
std::map<std::string,CommandEntry> m_commands;
std::atomic<bool> is_running{};

std::vector<std::string> split_string(const std::string& input)
{
    std::vector<std::string> output;
    std::string input_copy = input + ' ';
    size_t position;
    while ((position = input_copy.find(' ')) != std::string::npos)
    {
        output.emplace_back(input_copy.substr(0,position));
        input_copy.erase(0,position + 1);
    }
    return output;
}

bool parse_input(const std::string& input)
{
    auto cut = split_string(input);
    if (cut.empty()) return false;
    if (cut[0].empty()) return false;
    if (cut[0][0] != '/') return false;
    if (cut[0].size() <= 1) return false;
    std::string entry = cut[0].substr(1);
    cut.erase(cut.begin());
    if (!m_commands.count(entry))
    {
        CLIENT_MESSAGE("Command Not Found");
        return false;
    }
    m_commands.at(entry).command(cut);
    return true;
}

typedef const std::vector<std::string>& PARAMETERS;

void quit_command(PARAMETERS param)
{
    is_running.store(false);
    CLIENT_MESSAGE("Quitting");
    PACMAN::send_message(main_socket,DISCONNECT,"A");
}

void help_command(PARAMETERS param)
{
    if (param.empty())
    {
        CLIENT_MESSAGE("Commands:");
        for (const auto& command : m_commands)
            CLIENT_MESSAGE('/' << command.first);
    }
    else
    {
        if (!m_commands.count(param[0]))
        {
            CLIENT_MESSAGE("Invalid Parameters for /help. See /help without parameters for more commands.");
            return;
        }
        CLIENT_MESSAGE(m_commands.at(param[0]).usage);
    }
}

void joinroom_command(PARAMETERS param)
{
    if (param.empty())
    {
        CLIENT_MESSAGE("Join Room requires the name of the room. See /help for more commands.");
        return;
    }
    PACMAN::send_message(main_socket,JOIN_ROOM,param[0]);
}

void authenticate_command(PARAMETERS param)
{
    if (param.empty())
    {
        CLIENT_MESSAGE("AUTHENTICATION requires an authentication code. See /help for more commands.");
        return;
    }
    PACMAN::send_message(main_socket,AUTHENTICATE,param[0]);
}

void friend_command(PARAMETERS param)
{
    if (param.empty())
    {
        CLIENT_MESSAGE("You need to provide somebody's name to befriend.");
        return;
    }
    PACMAN::send_message(main_socket,FRIEND_REQUEST,param[0]);
}

void list_command(PARAMETERS param)
{
    PACMAN::send_message(main_socket,ROOM_LIST,"A");
}

void friendslist_command(PARAMETERS param)
{
    PACMAN::send_message(main_socket,FRIENDS_LIST,"A");
}

void whisper_command(PARAMETERS param)
{
    if (param.empty())
    {
        CLIENT_MESSAGE("You need to provide a user and a message to whipser");
        return;
    }
    std::stringstream packet;
    for (const auto& p : param)
        packet << p << ' ';
    PACMAN::send_message(main_socket,WHISPER,packet.str());
}

void shutoff_command(PARAMETERS param)
{
    PACMAN::send_message(main_socket,ADMIN_SHUTOFF,"A");
}

void announce_command(PARAMETERS param)
{
    std::stringstream msg;
    for (const auto& p : param)
        msg << p << ' ';
    PACMAN::send_message(main_socket,ADMIN_ANNOUNCE,msg.str());
}

int main(const int argc, char* argv[])
{
    is_running.store(true); // Store Atomic Boolean to sync input thread and main thread while loops
    if (argc <= 1)
    {
        LOG_ERROR("Must Provide a Username");
        return EXIT_FAILURE;
    }
    username = argv[1];

    // Load Commands
    m_commands.insert(std::make_pair("help",
                                     CommandEntry{
                                             help_command,
                                             "/help [COMMAND]\n"
                                             "Prints out all possible commands or how to use one\n"
                                             "Example:\n"
                                             "/help quit"
                                     }));
    m_commands.insert(std::make_pair("quit",
                                     CommandEntry{
                                             quit_command,
                                             "/quit\n"
                                             "Disconnects from the server and closes the client\n"
                                             "Example:\n"
                                             "/quit"
                                     }));
    m_commands.insert(std::make_pair("join",
                                     CommandEntry{
                                             joinroom_command,
                                             "/join [ROOMNAME]\n"
                                             "Join a different Chatroom\n"
                                             "Example:\n"
                                             "/join HOMEROOM"
                                     }));
    m_commands.insert(std::make_pair("auth",
                                     CommandEntry{
                                             authenticate_command,
                                             "/auth [PASSWORD]\n"
                                             "Authenticates yourself with the server for Administrator Privileges\n"
                                             "Example:\n"
                                             "/auth victorwee"
                                     }));
    m_commands.insert(std::make_pair("friend",
                                     CommandEntry{
                                             friend_command,
                                             "/friend [USERNAME]\n"
                                             "Requests to be somebody's friend or accept somebody's request\n"
                                             "Example:\n"
                                             "/friend chiansong"
                                     }));
    m_commands.insert(std::make_pair("flist",
                                     CommandEntry{
                                             friendslist_command,
                                             "/flist\n"
                                             "Shows your list of friends and incoming requests\n"
                                             "Example:\n"
                                             "/flist"
                                     }));
    m_commands.insert(std::make_pair("list",
                                     CommandEntry{
                                             list_command,
                                             "/list\n"
                                             "Shows everybody in the same room\n"
                                             "Example:\n"
                                             "/list"
                                     }));
    m_commands.insert(std::make_pair("whisper",
                                     CommandEntry{
                                             whisper_command,
                                             "/whisper [USERNAME]\n"
                                             "Sends a private message across the server\n"
                                             "Example:\n"
                                             "/whisper michael_jordon hello there michael jordon"
                                     }));
    m_commands.insert(std::make_pair("shutdown",
                                     CommandEntry{
                                             shutoff_command,
                                             "/shutdown\n"
                                             "Shuts down the server. You must be an administator to do this action\n"
                                             "Example:\n"
                                             "/shutdown"
                                     }));
    m_commands.insert(std::make_pair("announce",
                                     CommandEntry{
                                             announce_command,
                                             "/announce [MESSAGE]\n"
                                             "Announces a message to everyone in the server. You must be an administator to do this action\n"
                                             "Example:\n"
                                             "/announce hi everybody"
                                     }));

    int result = 0;
    LOG_INFO("Starting Up Client");
    WINSOCK_LINK;
    
    int port = DEFAULT_PORT;
    const char* server_address = "127.0.0.1";
    if (argc > 2)
    {
        try
        {
            port = std::stoi(argv[2]);
            LOG_INFO("Custom Port | " << port);
        } catch (const std::exception& e)
        {
            LOG_WARNING(argv[2] << " is an invalid port value");
            LOG_WARNING("Defaulting to port 25565");
            port = DEFAULT_PORT;
        }
        if (port > std::numeric_limits<uint16_t>::max())
        {
            LOG_ERROR("Port exceeds the port range 0-65536");
            WINSOCK_CLEANUP;
            return EXIT_FAILURE;
        }
        if (argc > 3)
            server_address = argv[3];
    }

    main_socket = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    if (0 > main_socket) // INVALID_SOCKET is ~0 which is apparently same as -1
    {
        LOG_ERROR("Failed To Create Socket");
        WINSOCK_CLEANUP;
        return EXIT_FAILURE;
    }
    LOG_INFO("Created Socket");

    sockaddr_in server_info{};
    server_info.sin_family = AF_INET;
    server_info.sin_port = htons(static_cast<uint16_t>(port));
    const int conversion_result = inet_pton(AF_INET,server_address,&server_info.sin_addr); // Converts String to IP_Address
    if (!conversion_result)
    {
        LOG_ERROR("Bad IP Address Provided");
        CLOSE_SOCKET(main_socket);
        WINSOCK_CLEANUP;
        return EXIT_FAILURE;
    }

    result = connect(main_socket,reinterpret_cast<sockaddr*>(&server_info),sizeof(server_info));
    if (0 > result) // SOCKET_ERROR is -1 | Linux also returns -1
    {
        LOG_ERROR("Failed To Connect To Server");
        WINSOCK_CLEANUP;
        return EXIT_FAILURE;
    }
    CLIENT_MESSAGE("Connected To " << server_address);
    LOG_INFO("Uploading Username");
    int username_send_result = send(main_socket,username.c_str(),username.size(),0);
    if (username_send_result < 0)
    {
        LOG_ERROR("Failure when sending Username to Server | " << GET_LAST_ERROR);
        CLOSE_SOCKET(main_socket);
        WINSOCK_CLEANUP;
        return EXIT_FAILURE;
    }

    std::thread input_thread(
        [&]
        {
            while (is_running.load())
            {
                std::string input;
                std::getline(std::cin,input);
                if (input.empty()) continue;
                if (parse_input(input)) continue;
                PACMAN::send_message(main_socket,MESSAGE,input);
            }
        });

    bool exit_code = EXIT_SUCCESS;
    while (is_running.load())
    {
        // Initialize Network Variables
        timeval timeout{};
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        fd_set server_connection;
        FD_ZERO(&server_connection);
        FD_SET(main_socket,&server_connection);

        const int select_result = select(0,&server_connection,nullptr,nullptr,&timeout);
        if (0 > select_result)
        {
            LOG_ERROR("Failure with select() | ERRORCODE: " << GET_LAST_ERROR);
            exit_code = EXIT_FAILURE;
            goto EXIT_POINT;
        }
        if (0 == select_result) continue; // Timeout

//        char message_buffer[MESSAGE_BUFFER_LENGTH]{'\0'};
//        const int receive_length = recv(main_socket,message_buffer,MESSAGE_BUFFER_LENGTH,0);
//        if (!receive_length)
//        {
//            LOG_INFO("Server Closed On Their Side");
//            is_running.store(false);
//            break;
//        }
//        if (receive_length < 0)
//        {
//            LOG_ERROR("recv() Error | ERROR: " << GET_LAST_ERROR);
//            goto EXIT_POINT;
//        }

        std::string from_server;
        const PACMAN::RECV_RETURN_CODE recv_result = PACMAN::receive_message(main_socket,from_server);

        switch (recv_result)
        {
            case PACMAN::RECV_RETURN_CODE::RECV_ERROR:
            {
                LOG_WARNING("Server Connection Suddenly Terminated | ERROR: " << GET_LAST_ERROR);
                goto EXIT_POINT;
            }
            case PACMAN::RECV_RETURN_CODE::RECV_ZERO_LEN:
            {
                SERVER_MESSAGE("Server has terminated the connection on their side");
                is_running.store(false);
                continue;
            }
            case PACMAN::RECV_RETURN_CODE::RECV_GOOD:
                break;
        }

        std::cout << from_server.c_str()+1 << std::endl;
    }

EXIT_POINT:
    // Interrupt Thread and Join
    LOG_INFO("Waiting To Join Input Thread");
    is_running.store(false);
    std::cin.putback('\r');
    input_thread.join();

    LOG_INFO("Client Closing");
    CLOSE_SOCKET(main_socket);
    WINSOCK_CLEANUP;
    return exit_code;
}
