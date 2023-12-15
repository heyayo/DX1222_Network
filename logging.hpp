#ifndef LOGGING_HPP
#define LOGGING_HPP

#define LOG_ERROR(msg) std::cerr << "[ERROR] " << msg << std::endl
#define LOG_WARNING(msg) std::cout << "[WARN] " << msg << std::endl
#define LOG_INFO(msg) std::cout << "[INFO] " << msg << std::endl

#define TAG_MESSAGE(msg, tag) std::cout << "\["#tag"\] " << msg << std::endl
#define SERVER_MESSAGE(msg) TAG_MESSAGE(msg,SERVER)
#define CLIENT_MESSAGE(msg) TAG_MESSAGE(msg,CLIENT)

#endif // LOGGING_HPP