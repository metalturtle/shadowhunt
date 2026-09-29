cmake_minimum_required(VERSION 3.16)
# project(ShadowHunt C CXX)
project(ShadowHunt)

include(CTest)

# Reuse compiler output across clean builds when ccache is installed.
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set(CMAKE_C_COMPILER_LAUNCHER ${CCACHE_PROGRAM})
    set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE_PROGRAM})
endif()

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

if(EMSCRIPTEN)
    # The browser build links everything statically into one .wasm, and
    # SDL_image only needs PNG/JPEG, which it decodes with its bundled stb.
    set(BUILD_SHARED_LIBS OFF)
    set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
    foreach(format AVIF JXL TIF WEBP SVG)
        set(SDLIMAGE_${format} OFF CACHE BOOL "" FORCE)
    endforeach()
    set(SDLIMAGE_BACKEND_STB ON CACHE BOOL "" FORCE)
else()
    set(BUILD_SHARED_LIBS true)
endif()

message("CMAKE_CURRENT_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}")

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/bin)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")
# set(BOX2D_SAMPLES OFF CACHE BOOL "" FORCE)
# set(BOX2D_UNIT_TESTS OFF CACHE BOOL "" FORCE)
# set(BOX2D_BENCHMARKS OFF CACHE BOOL "" FORCE)
# set(BOX2D_DOCS OFF CACHE BOOL "" FORCE)



add_subdirectory(vendored/SDL EXCLUDE_FROM_ALL)
add_subdirectory(vendored/SDL_image EXCLUDE_FROM_ALL)
# add_subdirectory(vendored/box2d EXCLUDE_FROM_ALL)


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
if(EMSCRIPTEN)
    set(SHADOWHUNT_NET_SOURCE src/web/web_net.c)
else()
    set(SHADOWHUNT_NET_SOURCE src/unix/unix_net.c)
endif()

add_executable(unix_main src/basic/basic.c src/basic/cvar.c src/basic/bitstream.c src/basic/files.c src/basic/cJSON.c src/core/raycast.c src/core/stealth_rules.c src/engine/stealth.c src/engine/bot.c src/unix/unix_main.c ${SHADOWHUNT_NET_SOURCE} src/render/sdl_render.c src/movement/movement.c src/engine/engine.c src/engine/net_conlayer.c src/engine/sv_main.c src/engine/cl_main.c src/engine/streammanage.c src/engine/entity.c src/engine/netEntity.c src/movement/ray.c src/engine/inputCommand.c src/engine/entitySystem.c)

# Chipmunk2D is used header-only (inline cpVect math); nothing links it.
target_include_directories(unix_main PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/vendored/Chipmunk2D/include)
# target_link_libraries(unix_main png)
# # target_link_libraries(unix_main glm)
# target_link_libraries(unix_main glfw)
# target_link_libraries(unix_main cjson)

target_link_libraries(unix_main SDL3::SDL3)
target_link_libraries(unix_main SDL3_image::SDL3_image)
# target_link_libraries(unix_main box2d::box2d)

# target_link_libraries(render SDL3::SDL3)
# target_link_libraries(render SDL3_image::SDL3_image)

# target_link_libraries(unix_main basic)
# target_link_libraries(unix_main engine)
# target_link_libraries(unix_main movement)
# target_link_libraries(unix_main render)

# target_link_libraries(unix_main PRIVATE SDL3::SDL3 SDL3_image::SDL3_image box2d::box2d tmx)


set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

if(EMSCRIPTEN)
    # Stage only the assets the game loads, then pack them into the page's
    # virtual filesystem (the repository's res/ also holds tools and sources).
    set(WEB_ASSETS ${CMAKE_BINARY_DIR}/web-assets)
    set(WEB_OUT ${CMAKE_BINARY_DIR}/web)
    file(REMOVE_RECURSE ${WEB_ASSETS})
    file(COPY ${CMAKE_SOURCE_DIR}/levels DESTINATION ${WEB_ASSETS})
    file(GLOB WEB_TEXTURES ${CMAKE_SOURCE_DIR}/res/*.png)
    file(COPY ${WEB_TEXTURES} DESTINATION ${WEB_ASSETS}/res)
    foreach(folder sprites playeranims)
        file(COPY ${CMAKE_SOURCE_DIR}/res/${folder} DESTINATION ${WEB_ASSETS}/res)
    endforeach()
    file(COPY ${CMAKE_SOURCE_DIR}/res/GUI/fonttest.png DESTINATION ${WEB_ASSETS}/res/GUI)

    set_target_properties(unix_main PROPERTIES
        OUTPUT_NAME shadowhunt
        SUFFIX ".html"
        RUNTIME_OUTPUT_DIRECTORY ${WEB_OUT})
    target_link_options(unix_main PRIVATE
        "SHELL:--preload-file ${WEB_ASSETS}@/"
        "SHELL:--shell-file ${CMAKE_SOURCE_DIR}/web/shell.html"
        -sALLOW_MEMORY_GROWTH=1
        -sINITIAL_MEMORY=64MB
        -lwebsocket.js)
endif()

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

# res/ (the art) is not in the repository; builds and headless tests work
# without it, but windowed clients need it to draw anything.
if(EXISTS ${CMAKE_SOURCE_DIR}/res)
    add_custom_target(copy-resource-files ALL
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/res
        ${CMAKE_BINARY_DIR}/res
        DEPENDS ${MY_TARGET})
else()
    message(WARNING "res/ not found: the game builds and tests run, but clients will draw no art")
endif()

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

if(BUILD_TESTING)
    add_executable(raycast_test tests/raycast_test.c src/core/raycast.c)
    target_include_directories(raycast_test PRIVATE ${CMAKE_SOURCE_DIR}/src/core)
    add_test(NAME raycast_geometry COMMAND raycast_test)

    add_executable(stealth_rules_test tests/stealth_rules_test.c src/core/stealth_rules.c)
    target_include_directories(stealth_rules_test PRIVATE ${CMAKE_SOURCE_DIR}/src/core)
    add_test(NAME stealth_rules COMMAND stealth_rules_test)

    find_package(Python3 COMPONENTS Interpreter)
    if(Python3_Interpreter_FOUND)
        add_test(
            NAME multiplayer_smoke_4_clients
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_SOURCE_DIR}/tests/multiplayer_smoke.py
                    --executable $<TARGET_FILE:unix_main>
                    --tuning ${CMAKE_SOURCE_DIR}/tests/fixtures/fast_tuning.json
                    --clients 4
                    --timeout 15
                    --verify-authority-disconnect
                    --verify-malformed
        )
        set_tests_properties(multiplayer_smoke_4_clients PROPERTIES TIMEOUT 60)

        add_test(
            NAME multiplayer_round_rematch
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_SOURCE_DIR}/tests/multiplayer_smoke.py
                    --executable $<TARGET_FILE:unix_main>
                    --tuning ${CMAKE_SOURCE_DIR}/tests/fixtures/fast_tuning.json
                    --clients 4
                    --timeout 15
                    --level ${CMAKE_SOURCE_DIR}/tests/fixtures/stealth_range.json
                    --verify-round
        )
        set_tests_properties(multiplayer_round_rematch PROPERTIES TIMEOUT 60)

        add_test(
            NAME stealth_visibility_4_clients
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_SOURCE_DIR}/tests/multiplayer_smoke.py
                    --executable $<TARGET_FILE:unix_main>
                    --tuning ${CMAKE_SOURCE_DIR}/tests/fixtures/fast_tuning.json
                    --clients 4
                    --timeout 15
                    --verify-stealth
        )
        set_tests_properties(stealth_visibility_4_clients PROPERTIES TIMEOUT 60)

        add_test(
            NAME stealth_full_server_8_clients
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_SOURCE_DIR}/tests/multiplayer_smoke.py
                    --executable $<TARGET_FILE:unix_main>
                    --tuning ${CMAKE_SOURCE_DIR}/tests/fixtures/fast_tuning.json
                    --clients 8
                    --timeout 20
                    --verify-stealth
                    --hold-seconds 5
        )
        # Nine processes at once starve parallel neighbours; keep this one serial.
        set_tests_properties(stealth_full_server_8_clients PROPERTIES TIMEOUT 60 RUN_SERIAL TRUE)

        add_test(
            NAME stealth_lit_rematch_visibility
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_SOURCE_DIR}/tests/multiplayer_smoke.py
                    --executable $<TARGET_FILE:unix_main>
                    --tuning ${CMAKE_SOURCE_DIR}/tests/fixtures/fast_tuning.json
                    --clients 2
                    --timeout 15
                    --level ${CMAKE_SOURCE_DIR}/tests/fixtures/stealth_lit.json
                    --round-seconds 2
                    --verify-lit-rounds
        )
        set_tests_properties(stealth_lit_rematch_visibility PROPERTIES TIMEOUT 60)

        add_test(
            NAME bots_play_a_round
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_SOURCE_DIR}/tests/multiplayer_smoke.py
                    --executable $<TARGET_FILE:unix_main>
                    --tuning ${CMAKE_SOURCE_DIR}/tests/fixtures/fast_tuning.json
                    --clients 2
                    --timeout 15
                    --level ${CMAKE_SOURCE_DIR}/tests/fixtures/stealth_lit.json
                    --verify-bots
        )
        set_tests_properties(bots_play_a_round PROPERTIES TIMEOUT 60)

        add_test(
            NAME tuning_and_level_hot_reload
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_SOURCE_DIR}/tests/multiplayer_smoke.py
                    --executable $<TARGET_FILE:unix_main>
                    --tuning ${CMAKE_SOURCE_DIR}/tests/fixtures/fast_tuning.json
                    --clients 2
                    --timeout 15
                    --verify-hot-reload
        )
        set_tests_properties(tuning_and_level_hot_reload PROPERTIES TIMEOUT 60)

        add_test(
            NAME stealth_pellet_tag_duel
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_SOURCE_DIR}/tests/multiplayer_smoke.py
                    --executable $<TARGET_FILE:unix_main>
                    --tuning ${CMAKE_SOURCE_DIR}/tests/fixtures/fast_tuning.json
                    --clients 2
                    --timeout 15
                    --level ${CMAKE_SOURCE_DIR}/tests/fixtures/stealth_duel.json
                    --round-seconds 6
                    --verify-tag
        )
        set_tests_properties(stealth_pellet_tag_duel PROPERTIES TIMEOUT 60)
    endif()
endif()
