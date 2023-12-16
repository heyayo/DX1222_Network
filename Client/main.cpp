#include "os_diff.hpp"

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <thread>
#include <atomic>

#include "logging.hpp"

#define DEFAULT_PORT 25565
#define MESSAGE_BUFFER_LENGTH 1024

typedef void(*Command)(const std::vector<std::string>&);

SOCKET main_socket;

std::string username;
std::map<std::string,Command> m_commands;

std::atomic<bool> is_running{};

enum INPUT_PARSE_CODE
{
    VALID_INPUT = 0,
    NO_INPUT = -1,
    BAD_INPUT = -2,
    NOT_A_COMMAND = 1
};

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

INPUT_PARSE_CODE parse_input(const std::string& input)
{
    auto cut = split_string(input);
    if (cut.empty()) return NO_INPUT;
    if (cut[0].empty()) return NO_INPUT;
    if (cut[0][0] != '/') return NOT_A_COMMAND;
    if (cut[0].size() <= 1) return BAD_INPUT;
    std::string command = cut[0].substr(1);
    cut.erase(cut.begin());
    if (!m_commands.count(command)) return NOT_A_COMMAND;
    m_commands.at(command)(cut);
    
    return VALID_INPUT;
}

void quit_command(const std::vector<std::string>& param)
{
    is_running.store(false);
    CLIENT_MESSAGE("Quitting");
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
    m_commands.insert(std::make_pair(
        "help",
        [](const std::vector<std::string>& param)
        {
            CLIENT_MESSAGE("Commands:");
            for (const auto& command : m_commands)
                CLIENT_MESSAGE('/' << command.first);
        } ));
    m_commands.insert(std::make_pair("quit", quit_command));

    int result = 0;
    LOG_INFO("Starting Up Client");
    WINSOCK_LINK;
    
    int port = DEFAULT_PORT;
    const char* server_address = "127.0.0.1";
    if (argc > 2)
    {
        port = std::stoi(argv[2]);
        LOG_INFO("Custom Port | " << port);
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
    LOG_INFO("Connected To " << server_address);

    std::thread input_thread(
        [&]
        {
            while (is_running.load())
            {
                std::string input;
                std::cin >> input;
                auto code = parse_input(input);
                switch (code)
                {
                case BAD_INPUT:
                    CLIENT_MESSAGE("No Command Detected");
                    break;
                case NO_INPUT:
                    CLIENT_MESSAGE("TODO Do message sending");
                    break;
                case NOT_A_COMMAND:
                    CLIENT_MESSAGE("Not A Command");
                    break;
                default:
                    break;
                }
            }
        });

    bool exit_code = EXIT_SUCCESS;
    while (is_running.load())
    {
        timeval timeout;
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
        char message_buffer[MESSAGE_BUFFER_LENGTH]{'\0'};
        const ssize_t receive_length = recv(main_socket,message_buffer,MESSAGE_BUFFER_LENGTH,0);
        if (!receive_length)
        {
            LOG_INFO("Server Closed On Their Side");
            break;
        }
        if (receive_length < 0)
        {
            LOG_ERROR("recv() Error | ERROR: " << GET_LAST_ERROR);
            goto EXIT_POINT;
        }
        SERVER_MESSAGE(message_buffer); // Replace with TAG_MESSAGE and username
    }

    // Interupt Thread and Join
    LOG_INFO("Waiting To Join Input Thread");
    input_thread.join();

EXIT_POINT:
    LOG_INFO("Client Closing");
    CLOSE_SOCKET(main_socket);
    WINSOCK_CLEANUP;
    return exit_code;
}
