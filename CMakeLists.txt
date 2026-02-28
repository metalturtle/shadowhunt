cmake_minimum_required(VERSION 3.16)
# project(ShadowHunt C CXX)
project(ShadowHunt)

# Export compile commands for IDEs and linters
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Default to Debug so breakpoints work when building from IDE
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()
# Ensure Debug builds have no optimization so breakpoints line up
set(CMAKE_C_FLAGS_DEBUG "-g -O0" CACHE STRING "C flags for Debug" FORCE)
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0" CACHE STRING "C++ flags for Debug" FORCE)

# set(CMAKE_CXX_STANDARD 14)
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")

set(BUILD_SHARED_LIBS true)

message("CMAKE_CURRENT_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}")

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/bin)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")
# set(BOX2D_SAMPLES OFF CACHE BOOL "" FORCE)
# set(BOX2D_UNIT_TESTS OFF CACHE BOOL "" FORCE)
# set(BOX2D_BENCHMARKS OFF CACHE BOOL "" FORCE)
# set(BOX2D_DOCS OFF CACHE BOOL "" FORCE)

set(BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(INSTALL_DEMOS OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED ON CACHE BOOL "" FORCE)
set(BUILD_STATIC OFF CACHE BOOL "" FORCE)

set(LIBXML2_WITH_MODULES OFF CACHE BOOL "" FORCE)
set(LIBXML2_WITH_PROGRAMS OFF CACHE BOOL "" FORCE)
set(LIBXML2_WITH_TESTS OFF CACHE BOOL "" FORCE)
set(LIBXML2_WITH_PYTHON OFF CACHE BOOL "" FORCE)
set(LIBXML2_WITH_HTTP OFF CACHE BOOL "" FORCE)
set(LIBXML2_WITH_CATALOG OFF CACHE BOOL "" FORCE)
set(LIBXML2_WITH_DEBUG OFF CACHE BOOL "" FORCE)

add_subdirectory(vendored/SDL EXCLUDE_FROM_ALL)
add_subdirectory(vendored/SDL_image EXCLUDE_FROM_ALL)
# add_subdirectory(vendored/box2d EXCLUDE_FROM_ALL)
add_subdirectory(vendored/Chipmunk2D EXCLUDE_FROM_ALL)
add_subdirectory(vendored/libxml2 EXCLUDE_FROM_ALL)
add_subdirectory(vendored/tmx EXCLUDE_FROM_ALL)


# add_subdirectory(src/lib/cJSON)

# add_library(chipmunk SHARED IMPORTED)
# set_target_properties(chipmunk PROPERTIES IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/lib/libchipmunk.so)


# add_library(glad SHARED IMPORTED)
# set_target_properties(glad PROPERTIES IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/lib/libglad.so)

# add_library(glm SHARED IMPORTED )
# set_target_properties(glm PROPERTIES IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/lib/libglm_shared.so)


# add_library(glfw SHARED IMPORTED )
# set_target_properties(glfw PROPERTIES IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/lib/libglfw.so)

# add_library(png SHARED IMPORTED)
# set_target_properties(png PROPERTIES IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/lib/libpng16.so)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/lib)
# include_directories(${CMAKE_CURRENT_SOURCE_DIR}/vendored/Chipmunk2D/include)
# include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/lib/glfw/include)

# add_subdirectory(src/basic)
# add_subdirectory(src/unix)
# add_subdirectory(src/engine)
# add_subdirectory(src/movement)
# add_subdirectory(src/render)

# Unix files are now in unix library
# We still need a main.cpp for the executable entry point
# For now, keep unix_main in the executable but this creates duplicate symbols
# TODO: Split unix_main.cpp to separate main() from other functions
add_executable(unix_main src/basic/basic.c src/basic/cvar.c src/basic/bitstream.c src/basic/files.c src/basic/cJSON.c src/unix/unix_main.c src/unix/unix_net.c src/render/sdl_render.c src/movement/movement.c src/engine/engine.c src/engine/net_conlayer.c src/engine/sv_main.c src/engine/cl_main.c src/engine/streammanage.c src/engine/entity.c src/engine/netEntity.c src/movement/ray.c src/engine/inputCommand.c src/engine/entitySystem.c)

target_include_directories(unix_main PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/vendored/Chipmunk2D/include)
# target_link_libraries(unix_main png)
# target_link_libraries(unix_main chipmunk)
# target_link_libraries(unix_main glm)
# target_link_libraries(unix_main glfw)
# target_link_libraries(unix_main cjson)

target_link_libraries(unix_main SDL3::SDL3)
target_link_libraries(unix_main SDL3_image::SDL3_image)
# target_link_libraries(unix_main box2d::box2d)
target_link_libraries(unix_main chipmunk)
target_link_libraries(unix_main tmx)

# target_link_libraries(render SDL3::SDL3)
# target_link_libraries(render SDL3_image::SDL3_image)

# target_link_libraries(unix_main basic)
# target_link_libraries(unix_main engine)
# target_link_libraries(unix_main movement)
# target_link_libraries(unix_main render)

# target_link_libraries(unix_main PRIVATE SDL3::SDL3 SDL3_image::SDL3_image box2d::box2d tmx)


set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_custom_target(copy-shader-files ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/shaders
    ${CMAKE_BINARY_DIR}/shaders
    DEPENDS ${MY_TARGET})

# Commented out - directories don't exist in source root
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

# ============================================================================
# TEST EXECUTABLE: test_netEntity
# Tests ESVar (Entity State Variable) functionality from netEntity.c
# Run with: ./test_netEntity
# ============================================================================
# add_executable(test_netEntity 
#     src/tests/test_netEntity.c
#     src/basic/basic.c 
#     src/basic/cvar.c 
#     src/basic/bitstream.c 
#     src/basic/files.c 
#     src/basic/cJSON.c
#     src/engine/netEntity.c
#     src/engine/streammanage.c
# )

# target_include_directories(test_netEntity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/vendored/Chipmunk2D/include)
# target_link_libraries(test_netEntity SDL3::SDL3)
# target_link_libraries(test_netEntity m)  # math library