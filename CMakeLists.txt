cmake_minimum_required(VERSION 3.19)
project(sylar)

include(cmake/utils.cmake)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_VERBOSE_MAKEFILE OFF)

set(LIB_SRC
    sylar/log.cpp
    )

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)

add_subdirectory(sylar)
add_subdirectory(test)
