cmake_minimum_required(VERSION 3.31)
project(tracy_playback_bin C CXX)

add_executable(${PROJECT_NAME} main.cpp)
target_link_libraries(${PROJECT_NAME} tracy_playback)

