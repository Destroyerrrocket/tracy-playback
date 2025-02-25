cmake_minimum_required(VERSION 3.31)

project(tracy-playback-recorder)
enable_testing()

set(CMAKE_INSTALL_PREFIX ${PROJECT_SOURCE_DIR})
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(src)
add_subdirectory(test)


