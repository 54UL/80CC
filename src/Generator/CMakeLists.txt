cmake_minimum_required(VERSION 3.15)

set(CMAKE_CXX_STANDARD 17)

project(Generator)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../Core/include)

add_executable(Generator main.cpp)
