#ifndef OS_DIFF_HPP
#define OS_DIFF_HPP

/*
 * LINUX SUPPORT HAS CEASED BECAUSE THE ASSIGNMENT IS DUE SOON
 */

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT _WIN32_WINNT_WIN10
#define NOMINMAX
#pragma comment(lib,"Ws2_32.lib")

#include <WinSock2.h>
#include <WS2tcpip.h>

#define CLOSE_SOCKET(socket) closesocket(socket)
#define WINSOCK_CLEANUP WSACleanup()
#define WINSOCK_LINK \
WSAData data{};\
result = WSAStartup(MAKEWORD(2,2),&data);\
if (result != 0)\
{\
    LOG_ERROR("WSAStartup Failure");\
    return EXIT_FAILURE;\
}

#define GET_LAST_ERROR WSAGetLastError()

typedef int sockaddr_length;

#elif __linux__

#include <sys/socket.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h> // memset
#include <errno.h>

#define CLOSE_SOCKET(socket) close(socket)
#define WINSOCK_CLEANUP
#define WINSOCK_LINK
#define GET_LAST_ERROR strerror(errno)

typedef int SOCKET;
typedef socklen_t sockaddr_length;

#endif

#endif
