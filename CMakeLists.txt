cmake_minimum_required(VERSION 3.10)
project(ShadowHunt C CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")

set(BUILD_SHARED_LIBS true)

message("CMAKE_CURRENT_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}")

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/bin)

# add_subdirectory(src/lib/cJSON)
add_subdirectory(src/lib/Chipmunk2D)

add_subdirectory(src/basic)
add_subdirectory(src/engine)
add_subdirectory(src/system)

add_executable(unix_main src/system/unix/unix_main.cpp src/system/unix/unix_net.c)

target_link_libraries(unix_main basic)
target_link_libraries(unix_main engine)
target_link_libraries(unix_main render)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_custom_target(copy-shader-files ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/shaders
    ${CMAKE_BINARY_DIR}/shaders
    DEPENDS ${MY_TARGET})

add_custom_target(copy-level-files ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/levels
    ${CMAKE_BINARY_DIR}/levels
    DEPENDS ${MY_TARGET})

add_custom_target(copy-resource-files ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/res
    ${CMAKE_BINARY_DIR}/res
    DEPENDS ${MY_TARGET})