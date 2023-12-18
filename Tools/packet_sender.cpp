#include "packet_sender.hpp"
#include "os_diff.hpp"
#include "logging.hpp"

#include <sstream>
#include <iostream>
#include <algorithm>

namespace PACMAN
{
    const int packet_message_size = packet_size - 2;

    void clean_string(std::string& input)
    {
        // Erase all codes
        input.erase(std::remove(input.begin(),input.end(),TAIL_CODE_END), input.end());
        input.erase(std::remove(input.begin(),input.end(),TAIL_CODE_CONTINUE), input.end());
        input.erase(std::remove(input.begin(),input.end(),DISCONNECT), input.end());
        input.erase(std::remove(input.begin(),input.end(),MESSAGE), input.end());
        input.erase(std::remove(input.begin(),input.end(),REFUSE_CONNECTION), input.end());
    }

    bool send_message(SOCKET receipient, NETWORK_CODE header, const std::string &message)
    {
        const size_t packets =
                (message.size() / packet_message_size) +
                ((message.size() % packet_message_size) != 0);
        for (int i = 0; i < packets; ++i)
        {
            std::stringstream final;
            final << header; // Insert Header Code
            final << message.substr( i*packet_message_size,
                                     (i+1) == packets-1 ? message.npos : (i+1) * packet_message_size );
            final << ((i == packets - 1) ? TAIL_CODE_END : TAIL_CODE_CONTINUE);
            std::string final_message = final.str();
            int result = send(receipient, final_message.c_str(), final_message.size(),0);
            if (result < 0)
            {
                LOG_ERROR("send_message() failed on packet " << i+1 << " Out Of " << packets);
                return false;
            }
        }

        return true;
    }

    RECV_RETURN_CODE receive_message(SOCKET sender, std::string& output)
    {
        std::stringstream msg_stream;
        std::stringstream final;

        bool all_messages_received = false;
        do
        {
            char buffer[packet_size+1]{'\0'};
            int result = recv(sender,buffer,packet_size,0);
            if (result < 0)
            {
                LOG_ERROR("Failure in recv()");
                return RECV_RETURN_CODE::RECV_ERROR;
            }
            if (result == 0) return RECV_RETURN_CODE::RECV_ZERO_LEN;

            if (buffer[result-1] == TAIL_CODE_END)
            {
                all_messages_received = true;
                buffer[result-1] = '\0';
            }
            msg_stream << buffer;
        }
        while (!all_messages_received);

        std::string unclean = msg_stream.str();
        final << unclean[0]; // Store header
        clean_string(unclean);
        final << unclean;

        output = final.str();

        return RECV_RETURN_CODE::RECV_GOOD;
    }
}
