cmake_minimum_required(VERSION 3.10)

add_library(unix SHARED unix_main.cpp unix_net.c)

target_link_libraries(unix 
    basic
    SDL3::SDL3
)

target_include_directories(unix PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../
)
