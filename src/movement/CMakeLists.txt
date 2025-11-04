cmake_minimum_required(VERSION 3.10)

add_library(movement SHARED movement.c)

target_link_libraries(movement basic)
