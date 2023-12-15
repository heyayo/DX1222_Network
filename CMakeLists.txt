cmake_minimum_required(VERSION 3.0.0)
project(NETWORK VERSION 1.0.0)

add_subdirectory(Client)
add_subdirectory(Server)

add_custom_target(ALLBUILD)
add_dependencies(ALLBUILD NETCLIENT NETSERVER)
