cmake_minimum_required(VERSION 3.10)

add_library(render SHARED sdl_render.cpp)

# add_subdirectory(../../vendored/SDL EXCLUDE_FROM_ALL)
# add_subdirectory(../../vendored/SDL_image EXCLUDE_FROM_ALL)

# Link SDL3 and SDL3_image libraries
target_link_libraries(render 
    SDL3::SDL3
    SDL3_image::SDL3_image
    basic
    engine
)

# Include directories for headers
target_include_directories(render PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../
)