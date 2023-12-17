cmake_minimum_required(VERSION 3.5.0)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/output)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_SOURCE_DIR}/output)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_SOURCE_DIR}/output)

add_library(NETTOOLS packet_sender.cpp)

target_include_directories(NETTOOLS PUBLIC ${CMAKE_SOURCE_DIR})
if (WIN32)
    target_link_libraries(NETTOOLS PUBLIC wsock32 ws2_32)
endif()