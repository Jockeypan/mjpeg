cmake_minimum_required(VERSION 2.8)

PROJECT( jcodec )

FIND_PACKAGE( OpenCV REQUIRED )



message(STATUS "OpenCV_INCLUDE_DIRS: ${OpenCV_INCLUDE_DIRS}")

include_directories(${OpenCV_INCLUDE_DIRS})

set(the_target "jcodec")

# file(GLOB srcs RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.cpp)

# file(GLOB hdrs RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.hpp)

add_executable(${the_target} Main.cpp mjpegwriter.cpp)

target_link_libraries(${the_target} ${OpenCV_LIBS})