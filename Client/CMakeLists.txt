cmake_minimum_required(VERSION 3.5.0)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/output)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_SOURCE_DIR}/output)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_SOURCE_DIR}/output)

add_executable(NETCLIENT main.cpp)

target_include_directories(NETCLIENT PUBLIC ${CMAKE_SOURCE_DIR})
target_include_directories(NETCLIENT PUBLIC ${CMAKE_SOURCE_DIR}/tools)
target_link_libraries(NETCLIENT PUBLIC NETTOOLS)
if (WIN32)
    target_link_libraries(NETCLIENT PUBLIC wsock32 ws2_32)
endif()
