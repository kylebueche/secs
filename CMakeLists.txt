cmake_minimum_required(VERSION 4.0)
project(secs)

set(CMAKE_CXX_STANDARD 20)

file(GLOB_RECURSE SRC_FILES src/*.h src/*.cpp)

add_executable(secs ${SRC_FILES})

target_include_directories(secs PUBLIC src)
