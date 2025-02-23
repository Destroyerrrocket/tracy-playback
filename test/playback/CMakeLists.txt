cmake_minimum_required(VERSION 3.31)
project(playback_tests)
include(GoogleTest)

set(SOURCE_FILES main.cpp playback_tests.cpp)

add_executable(${PROJECT_NAME} ${SOURCE_FILES})

install(TARGETS ${PROJECT_NAME} DESTINATION bin)

find_package(GTest)
target_link_libraries(${PROJECT_NAME} tracy_playback gtest::gtest)

gtest_discover_tests(${PROJECT_NAME})