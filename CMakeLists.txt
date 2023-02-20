cmake_minimum_required(VERSION 3.24)
project(server1 C)

set(CMAKE_C_STANDARD 11)

include_directories(.)

add_executable(server1
        server.c
        threadpool.c
        threadpool.h)


