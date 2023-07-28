cmake_minimum_required(VERSION 3.19)
project(sylar)

include(cmake/utils.cmake)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_VERBOSE_MAKEFILE OFF)
set(CMAKE_CXX_FLAGS "$ENV{CXXFLAGS} -rdynamic -O3 -fPIC -ggdb -Wall -Wno-builtin-macro-redefined -Wno-unused-function")
set(CMAKE_C_FLAGS "$ENV{CXXFLAGS} -rdynamic -O3 -fPIC -ggdb -Wall -Wno-builtin-macro-redefined -Wno-unused-function")

find_package(yaml-cpp REQUIRED)
find_package(Boost REQUIRED COMPONENTS lexical_cast)

include_directories(${CMAKE_SOURCE_DIR}/sylar)

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(sylar)
add_subdirectory(test)
