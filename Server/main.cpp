#include "os_diff.hpp"
#include "logging.hpp"
#include "NETWORK_CODES.hpp"
#include "packet_sender.hpp"

#include <iostream>
#include <map>
#include <string>
#include <vector>
#include <queue>
#include <limits>
#include <algorithm>
#include <logging.hpp>
#include <sstream>
#include <memory>

#define TCP_BACKLOG 10
#define DEFAULT_PORT 25565
#define STARTING_ROOM_NAME "HOMEROOM"
#define DEFAULT_AUTHENTICATION_CODE "secure_code"

struct ClientData
{
    std::string username;
    std::string room;
    SOCKET socket;
    sockaddr_in network;
    std::map<SOCKET,std::shared_ptr<ClientData>> friends;
    std::map<SOCKET,bool> pending;

    ClientData(const std::string& name, const std::string& rm, SOCKET sock, sockaddr_in net) : username(name), room(rm), socket(sock), network(net) {}
};


typedef std::shared_ptr<ClientData> ClientDataPtr;
struct ChatRoom
{
    std::string name;
    std::map<SOCKET,ClientDataPtr> clients;
};

struct Database
{
    std::map<SOCKET,ClientDataPtr> by_socket;
    std::map<std::string,ClientDataPtr> by_name;
    std::map<std::string,ChatRoom> rooms;
    std::map<SOCKET,ClientDataPtr> administrators;

    // No Error checking for self sending
    bool befriend(SOCKET sender, const std::string& receipient)
    {
        const auto& receiver = by_name.at(receipient);
        const auto& who = by_socket.at(sender);
        if (who->pending.count(receiver->socket))
        {
            receiver->friends.insert(std::make_pair(sender,who)); // Add to their list
            who->pending.erase(receiver->socket); // Remove the pending friend request
            who->friends.insert(std::make_pair(receiver->socket,receiver)); // Add to sender's list
            return true;
        }
        receiver->pending.insert(std::make_pair(sender,true)); // Send a pending friend request
        return false;
    }

    bool unfriend(SOCKET sender, const std::string& receipient)
    {
        const auto& receiver = by_name.at(receipient);
        const auto& person = by_socket.at(sender);
        if (person->friends.count(receiver->socket))
        {
            receiver->friends.erase(sender);
            person->friends.erase(receiver->socket);
            return true;
        }
        return false;
    }

    void wipe_slate(const ClientDataPtr& who)
    {
        const auto& flist = who->friends;
        for (const auto& f : flist)
            f.second->friends.erase(who->socket); // Remove this person from their friend's friends list
    }

    void join(const ClientDataPtr& user, const std::string& room)
    {
        rooms[room].clients.insert(std::make_pair(user->socket,user));
        user->room = room;
    }
    void leave(const ClientDataPtr& user)
    {
        rooms.at(user->room).clients.erase(user->socket);
    }
    void move(SOCKET user, const std::string& room)
    {
        const auto& userdata = by_socket.at(user);
        leave(userdata); // Remove from old room
        join(userdata,room); // Inserts into existing or creates a new room
    }

    bool add(const ClientDataPtr& user)
    {
        if (by_name.count(user->username)) return false; // Check for existing user
        by_socket.emplace(std::make_pair(user->socket,user));
        by_name.emplace(std::make_pair(user->username,user));
        join(user,STARTING_ROOM_NAME);
        return true;
    }

    void rem(const ClientDataPtr& user)
    {
        if (administrators.count(user->socket))
            administrators.erase(user->socket);
        wipe_slate(user);
        by_socket.erase(user->socket);
        by_name.erase(user->username);
        leave(user);
    }
    void rem(SOCKET socket)
    {
        if (administrators.count(socket))
            administrators.erase(socket);
        wipe_slate(by_socket.at(socket));
        leave(by_socket.at(socket));
        by_name.erase(by_socket.at(socket)->username);
        by_socket.erase(socket);
    }
    void rem(const std::string& name)
    {
        if (administrators.count(by_name.at(name)->socket))
            administrators.erase(by_name.at(name)->socket);
        wipe_slate(by_name.at(name));
        leave(by_name.at(name));
        by_socket.erase(by_name.at(name)->socket);
        by_name.erase(name);
    }

    const ChatRoom& room(SOCKET socket)
    {
        return rooms.at(by_socket.at(socket)->room);
    }
};

SOCKET m_listener_socket;
fd_set m_sockets;
Database m_users{};
bool isRunning = true;
std::string authcode = DEFAULT_AUTHENTICATION_CODE;

void print_clientdata(const ClientDataPtr& data)
{
    char ip_address[INET_ADDRSTRLEN];
    inet_ntop(AF_INET,&data->network.sin_addr,ip_address,INET_ADDRSTRLEN);
    SERVER_MESSAGE("IP Address: " << ip_address);
    SERVER_MESSAGE("Port: " << ntohs(data->network.sin_port));
    SERVER_MESSAGE("Username: " << data->username);
    SERVER_MESSAGE("Socket: " << data->socket);
}

std::stringstream get_server_stream()
{
    std::stringstream stream;
    stream << "[SERVER] | ";
    return stream;
}

// Automatically adds the server tag
void announce_all(const std::string& message)
{
    for (const auto& user : m_users.by_socket)
        PACMAN::send_message(user.first,MESSAGE,message);
}

void announce_all_but(SOCKET socket, const std::string& message)
{
    for (const auto& user : m_users.by_socket)
    {
        if (user.first == socket) continue;
        PACMAN::send_message(user.first,MESSAGE,message);
    }
}

void announce_room(const std::string& room, const std::string& message)
{
    const ChatRoom& theRoom = m_users.rooms[room];
    for (const auto& user : theRoom.clients)
        PACMAN::send_message(user.first,MESSAGE,message);
}

void announce_room_but(SOCKET socket, const std::string& room, const std::string& message)
{
    const ChatRoom& theRoom = m_users.rooms[room];
    for (const auto& user : theRoom.clients)
    {
        if (user.first == socket) continue;
        PACMAN::send_message(user.first,MESSAGE,message);
    }
}

void announce_all_friends(SOCKET socket, const std::string& message)
{
    const auto& fList = m_users.by_socket.at(socket)->friends;
    for (const auto& f : fList)
        PACMAN::send_message(f.first,MESSAGE,message);
}

void disconnect_user(SOCKET client)
{
    std::stringstream announcement = get_server_stream();
    announcement << m_users.by_socket.at(client)->username << " has disconnected from the server.";
    announce_all_but(client,announcement.str());
    announcement = get_server_stream();
    announcement << "Your friend " << m_users.by_socket.at(client)->username << " has disconnected from the server.";
    announce_all_friends(client,announcement.str());
//    const auto& room = m_users.room(client);
//    for (const auto& user : room.clients)
//    {
//        if (user.first == client) continue;
//        PACMAN::send_message(user.first,MESSAGE,announcement.str());
//    }
    SERVER_MESSAGE("Client Has Disconnected");
    print_clientdata(m_users.by_socket.at(client));
    m_users.rem(client);
    FD_CLR(client,&m_sockets);
}

int main(const int argc, char* argv[])
{
    // Parse Console Arguments
    int port = DEFAULT_PORT;
    int result{};
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

    // Initialize Server Variables
    m_users.rooms.insert(std::make_pair(std::string{STARTING_ROOM_NAME}, ChatRoom{})); // Default Room

    // Initialize winsock2
    SERVER_MESSAGE("Starting Up Server");
    WINSOCK_LINK

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
    LOG_INFO("Created Listener Socket | " << m_listener_socket);

    sockaddr_in server_info{};
    server_info.sin_family = AF_INET;
    server_info.sin_port = htons(static_cast<uint16_t>(port));
    server_info.sin_addr.s_addr = htonl(INADDR_ANY);
    result = bind(m_listener_socket,reinterpret_cast<sockaddr*>(&server_info),sizeof(server_info));
    if (0 > result)
    {
        LOG_ERROR("Failed To Bind Listener Socket | " << GET_LAST_ERROR);
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
        WINSOCK_CLEANUP;
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
            LOG_ERROR("Failure with select() | ERROR: " << GET_LAST_ERROR << " LINE: " << __LINE__);
            exit_code = EXIT_FAILURE;
            goto EXIT_POINT;
        }
        if (0 == check) // select() timeout
            continue;

        for (int i = 0; i < temp_set.fd_count; ++i)
        {
            const SOCKET client = temp_set.fd_array[i];
            if (client != m_listener_socket)
            {
                std::string from_client;
                const PACMAN::RECV_RETURN_CODE recv_result = PACMAN::receive_message(client,from_client);
                switch (recv_result)
                {
                    case PACMAN::RECV_RETURN_CODE::RECV_ERROR:
                    {
                        LOG_WARNING("Failure when receiving from client");
                        print_clientdata(m_users.by_socket.at(client));
                        continue;
                    }
                    case PACMAN::RECV_RETURN_CODE::RECV_ZERO_LEN:
                    {
                        disconnect_user(client);
                        continue;
                    }
                    case PACMAN::RECV_RETURN_CODE::RECV_GOOD:
                        break;
                }

                // Parse Incoming Messages
                switch (from_client[0])
                {
                    case DISCONNECT:
                    {
                        disconnect_user(client);
                        continue;
                    }
                    case MESSAGE:
                    {
                        // Print out what the client sent over
                        std::stringstream formatted_message;
                        formatted_message << '[' << m_users.by_socket.at(client)->username << "] | " << from_client.c_str()+1;
                        const auto& room = m_users.room(client);
                        for (const auto& user : room.clients)
                        {
                            if (user.second->socket == client) continue;
                            PACMAN::send_message(user.first, MESSAGE,formatted_message.str());
                        }
                        SERVER_MESSAGE(formatted_message.str());
                        continue;
                    }
                    case JOIN_ROOM:
                    {
                        const std::string& username = m_users.by_socket.at(client)->username;
                        if (from_client.size() <= 1)
                        {
                            LOG_WARNING(username << " asked to join with no room name.");
                            std::stringstream stream = get_server_stream();
                            stream << "You need to give a room name";
                            PACMAN::send_message(client,MESSAGE,stream.str());
                            continue;
                        }
                        std::string roomname = from_client.c_str()+2;

                        const ChatRoom& afterRoom = m_users.rooms[roomname];
                        const ChatRoom& beforeRoom = m_users.room(client);
                        m_users.move(client,roomname);

                        std::stringstream joinMessage = get_server_stream();
                        joinMessage << username << " has joined " << afterRoom.name;
                        for (const auto& user : afterRoom.clients)
                        {
                            if (user.first == client) continue;
                            PACMAN::send_message(user.first,MESSAGE,joinMessage.str());
                        }
                        std::stringstream leaveMessage = get_server_stream();
                        leaveMessage << username << " has left " << beforeRoom.name;
                        for (const auto& user : beforeRoom.clients)
                            PACMAN::send_message(user.first,MESSAGE,leaveMessage.str());

                        SERVER_MESSAGE(m_users.by_socket.at(client)->username << " Has Moved To " << roomname);

                        continue;
                    }
                    case AUTHENTICATE:
                    {
                        std::string provided_code = from_client.c_str()+2;
                        const std::string& author = m_users.by_socket.at(client)->username;

                        if (provided_code == authcode)
                        {
                            m_users.administrators.insert(std::make_pair(client,m_users.by_socket.at(client)));
                            SERVER_MESSAGE(author << " has authenticated as Administrator");
                            PACMAN::send_message(client,MESSAGE, "You are now an administrator.");
                            continue;
                        }

                        SERVER_MESSAGE(author << " attempted to authenticate as Administrator with code " << provided_code);
                        PACMAN::send_message(client,MESSAGE, "You have entered an invalid code.");
                        continue;
                    }
                    case FRIEND_REQUEST:
                    {
                        std::string userToFriend = from_client.c_str()+2;
                        const auto& senderData = m_users.by_socket.at(client);
                        const std::string& sender = senderData->username;
                        if (userToFriend == sender) // Self send
                        {
                            std::stringstream warn = get_server_stream();
                            warn << "You cannot send a friend request to yourself.";
                            SERVER_MESSAGE(sender << " tried to befriend himself");
                            PACMAN::send_message(client, MESSAGE, warn.str());
                            continue;
                        }
                        if (!m_users.by_name.count(userToFriend)) // Possible Send
                        {
                            std::stringstream errormsg = get_server_stream();
                            errormsg << userToFriend << " does not exist.";
                            SERVER_MESSAGE(sender << " tried to send a friend request to unknown user " << userToFriend);
                            PACMAN::send_message(client,MESSAGE,errormsg.str());
                            continue;
                        }
                        const auto& userToFriendData = m_users.by_name.at(userToFriend);
                        if (userToFriendData->pending.count(senderData->socket))
                        {
                            std::stringstream errormsg = get_server_stream();
                            errormsg << "You have already sent a friend request to this person";
                            SERVER_MESSAGE(sender << " sent a duplicate friend request to " << userToFriend);
                            PACMAN::send_message(client,MESSAGE,errormsg.str());
                            continue;
                        }
                        if (senderData->friends.count(userToFriendData->socket))
                        {
                            std::stringstream errormsg = get_server_stream();
                            errormsg << "You are already friends with " << userToFriend;
                            SERVER_MESSAGE(sender << " tried to befriend " << userToFriend << " again");
                            PACMAN::send_message(client,MESSAGE,errormsg.str());
                            continue;
                        }
                        bool friendStatus = m_users.befriend(client,userToFriend);
                        if (friendStatus)
                        {
                            std::stringstream updateThem = get_server_stream();
                            updateThem << sender << " has accepted your friend request.";
                            std::stringstream updateClient = get_server_stream();
                            updateClient << "You are now friends with " << userToFriend << '.';
                            SERVER_MESSAGE(sender << " is now friends with " << userToFriend);
                            PACMAN::send_message(m_users.by_name.at(userToFriend)->socket,MESSAGE,updateThem.str());
                            PACMAN::send_message(client,MESSAGE,updateClient.str());
                            continue;
                        }

                        std::stringstream noticeThem = get_server_stream();
                        noticeThem << sender << " has sent you a friend request.";
                        std::stringstream noticeMe = get_server_stream();
                        noticeMe << "You have sent a friend request to " << userToFriend << '.';

                        PACMAN::send_message(m_users.by_name.at(userToFriend)->socket,MESSAGE,noticeThem.str());
                        PACMAN::send_message(client,MESSAGE,noticeMe.str());
                        continue;
                    }
                    case FRIENDS_LIST:
                    {
                        std::stringstream flist = get_server_stream();
                        const auto& who = m_users.by_socket.at(client);
                        flist << "Friends:" << std::endl;
                        for (const auto& f : who->friends)
                        {
                            flist << '\t' <<  f.second->username;
                            flist << std::endl;
                        }
                        flist << "Pending:";
                        for (const auto& p : who->pending)
                        {
                            flist << std::endl;
                            flist << '\t' << m_users.by_socket.at(p.first)->username;
                        }
                        flist << std::endl;
                        PACMAN::send_message(client,MESSAGE,flist.str());
                        continue;
                    }
                    case ROOM_LIST:
                    {
                        std::stringstream list = get_server_stream();
                        list << "Room List:" << std::endl;

                        const auto& room = m_users.room(client);
                        for (const auto& user : room.clients)
                        {
                            list << '\t' <<  user.second->username;
                            list << std::endl;
                        }

                        PACMAN::send_message(client,MESSAGE,list.str());
                        continue;
                    }
                    case WHISPER:
                    {
                        std::string rest_of_the_message = from_client.c_str()+2;
                        auto iter = rest_of_the_message.find(' ');
                        std::string target = rest_of_the_message.substr(0,iter);
                        std::string message = rest_of_the_message.substr(iter);
                        if (target == m_users.by_socket.at(client)->username)
                        {
                            std::stringstream stream = get_server_stream();
                            stream << "You cannot whisper to yourself.";
                            SERVER_MESSAGE(m_users.by_socket.at(client)->username << " attempted to whisper to himself");
                            PACMAN::send_message(client,MESSAGE,stream.str());
                            continue;
                        }

                        if (!m_users.by_name.count(target))
                        {
                            std::stringstream stream = get_server_stream();
                            stream << target << " does not exist.";
                            SERVER_MESSAGE(m_users.by_socket.at(client)->username << " attempted to whisper to someone who doesn't exist");
                            PACMAN::send_message(client,MESSAGE,stream.str());
                            continue;
                        }
                        const auto& targetData = m_users.by_name.at(target);
                        std::stringstream whisper;
                        whisper << "[WHISPER FROM " << m_users.by_socket.at(client)->username << "] | " << message;
                        PACMAN::send_message(targetData->socket,MESSAGE,whisper.str());

                        continue;
                    }
                    case ADMIN_SHUTOFF:
                    {
                        if (!m_users.administrators.count(client))
                        {
                            SERVER_MESSAGE(m_users.by_socket.at(client)->username << " issued a shutdown request as a normal user");
                            PACMAN::send_message(client, MESSAGE, "You have to be an administrator to do this action.");
                            continue;
                        }
                        std::stringstream msg = get_server_stream();
                        msg << "Server shutdown has been issued";
                        announce_all(msg.str());
                        isRunning = false;
                        continue;
                    }
                    case ADMIN_ANNOUNCE:
                    {
                        if (!m_users.administrators.count(client))
                        {
                            SERVER_MESSAGE(m_users.by_socket.at(client)->username << " issued an announcement request as a normal user");
                            PACMAN::send_message(client, MESSAGE, "You have to be an administrator to do this action.");
                            continue;
                        }
                        std::string msg = from_client.c_str() + 2;
                        std::stringstream announcement = get_server_stream();
                        announcement << msg;
                        announce_all(announcement.str());
                        continue;
                    }
                    default:
                        LOG_WARNING("Received Unrecognised Code | " << from_client[0]);
                        break;
                }
                continue; // Skip self socket
            }

            // If the listener has incoming connections
            sockaddr_in incoming{};
            sockaddr_length incoming_size = sizeof(incoming);
            const SOCKET new_client = accept(m_listener_socket,reinterpret_cast<sockaddr*>(&incoming),&incoming_size);
            char username_buffer[64]{'\0'};
            recv(new_client,username_buffer,63,0);

            // Add Client to Database
            SERVER_MESSAGE("Client Has Connected");
            ClientDataPtr new_client_data = std::make_shared<ClientData>(std::string(username_buffer),STARTING_ROOM_NAME,new_client,incoming);
            bool exist = m_users.add(new_client_data);
            if (!exist)
            {
                SERVER_MESSAGE("Attempted join from User with conflicting names | NAME: " << new_client_data->username);
                PACMAN::send_message(new_client,REFUSE_CONNECTION,"This username is taken");
                CLOSE_SOCKET(new_client);
                continue;
            }
            print_clientdata(new_client_data);

            // Hard Coding Tags ([TAG]), no time for rewrite
            std::stringstream welcome_msg = get_server_stream();
            welcome_msg << "Welcome " << new_client_data->username << ". You are in room HOMEROOM.";
            PACMAN::send_message(new_client,MESSAGE,welcome_msg.str());

            std::stringstream announcement_msg = get_server_stream();
            announcement_msg << new_client_data->username << " has joined the server.";
            announce_all_but(new_client, announcement_msg.str());

            FD_SET(new_client,&m_sockets);
        }
    }

    LOG_INFO("Server Closing");

EXIT_POINT:
    for (int i = 0; i < m_sockets.fd_count; ++i)
    {
        if (m_sockets.fd_array[i] == m_listener_socket)
        {
            CLOSE_SOCKET(m_sockets.fd_array[i]);
            continue;
        }
        disconnect_user(m_sockets.fd_array[i]);
    }
    WINSOCK_CLEANUP;
    return exit_code;
}
