#ifndef NETWORK_PACKET_SENDER_HPP
#define NETWORK_PACKET_SENDER_HPP

#include "NETWORK_CODES.hpp"

#include <string>
#include <vector>

// To get the word SOCKET
typedef unsigned long long SOCKET;

namespace PACMAN
{
    const static int packet_size = 1024; // 1022 + HEADER_CODE + TAIL_CODE

    enum class RECV_RETURN_CODE
    {
        RECV_ERROR,
        RECV_ZERO_LEN,
        RECV_GOOD
    };

    bool send_message(SOCKET receipient, NETWORK_CODE header, const std::string &message);
    RECV_RETURN_CODE receive_message(SOCKET sender, std::string& output);
}

#endif //NETWORK_PACKET_SENDER_HPP
