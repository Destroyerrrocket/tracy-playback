cmake_minimum_required(VERSION 3.31)
project(tracy_playback VERSION 1.0.0 LANGUAGES C CXX)
include(GNUInstallDirs)

set(SOURCE_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/eventStream.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/playback.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/playbackThread.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/threadGroupAllocator.cpp
)

set(HEADER_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/include/eventStream.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/playback.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/playbackThread.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/processInfo.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/threadGroupAllocator.h
)

add_library(${PROJECT_NAME} STATIC ${SOURCE_FILES} ${HEADER_FILES})
set_target_properties(${PROJECT_NAME} PROPERTIES VERSION ${PROJECT_VERSION})
set_target_properties(${PROJECT_NAME} PROPERTIES PUBLIC_HEADER ${HEADER_FILES})
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
install(TARGETS ${PROJECT_NAME} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

find_package(Tracy)
target_link_libraries(${PROJECT_NAME} PRIVATE Tracy::TracyClient tracy_recorder)