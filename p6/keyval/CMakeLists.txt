cmake_minimum_required(VERSION 3.6)
project(keyval)

set(CMAKE_CXX_STANDARD 11)

set(SOURCE_FILES main.cpp)

add_executable(keyval ${SOURCE_FILES})

target_link_libraries(keyval pthread)
target_link_libraries(keyval rt)