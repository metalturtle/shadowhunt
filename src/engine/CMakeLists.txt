cmake_minimum_required(VERSION 3.10)

set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR}/engine)

add_library(engine SHARED engine.c net_conlayer.c sv_main.c cl_main.c streammanage.c entity.c)

target_link_libraries(engine basic movement unix SDL3::SDL3)

target_include_directories(engine PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../
)