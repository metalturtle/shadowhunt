cmake_minimum_required(VERSION 3.10)
project(ShadowHunt_basic C)


add_library(basic SHARED basic.c cvar.c bitstream.c)

set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR}/basic)
