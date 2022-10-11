cmake_minimum_required(VERSION 3.10)

set(BUILD_SHARED_LIBS true)

set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR}/system)

add_library(render SHARED render/render.cpp)

set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR}/system/lib)
add_subdirectory(lib/libpng)
add_subdirectory(lib/glad)
add_subdirectory(lib/glm)
add_subdirectory(lib/glfw)

target_link_libraries(render glad)
target_link_libraries(render glm)
target_link_libraries(render glfw)
target_link_libraries(render png)

# target_link_libraries(render basic)