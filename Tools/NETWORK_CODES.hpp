#ifndef NETWORK_NETWORK_CODES_HPP
#define NETWORK_NETWORK_CODES_HPP

enum NETWORK_CODE : unsigned char
{
    DISCONNECT,
    MESSAGE, // This cannot be zero becuase string manip will treat as a null terminator
    JOIN_ROOM,
    AUTHENTICATE,
    ROOM_LIST,
    FRIEND_REQUEST,
    FRIENDS_LIST,
    WHISPER,
    ADMIN_SHUTOFF,
    ADMIN_ANNOUNCE,
    TAIL_CODE_END,
    TAIL_CODE_CONTINUE,
    REFUSE_CONNECTION
};

#endif //NETWORK_NETWORK_CODES_HPP
