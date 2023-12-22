cmake_minimum_required(VERSION 3.10)
project(MyGameEngine)

# Set C++ standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add subdirectories
add_subdirectory(core)
add_subdirectory(entry)
add_subdirectory(game)