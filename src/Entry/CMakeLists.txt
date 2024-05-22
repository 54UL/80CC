cmake_minimum_required(VERSION 3.15)

project(Entry)
set(CMAKE_CXX_STANDARD 17)	


set(ENTRY_HEADERS 
    include/EntryPoint.hpp
)

set(ENTRY_SOURCES 
    src/EntryPoint.cpp
)

# TODO: disable all game related stuff if not compiling game modules
file(GLOB_RECURSE GAME_SOURCES "temp/**.cpp") 
file(GLOB_RECURSE GAME_HEADERS "temp/**.hpp") 

message("GAME MODULES DETECTED SOURCES")
message("${GAME_SOURCES}")

find_package(spdlog CONFIG REQUIRED)

add_executable(Entry ${ENTRY_HEADERS} ${ENTRY_SOURCES} ${GAME_SOURCES} ${GAME_HEADERS})

target_include_directories(Entry
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(Entry PUBLIC 80CC_CORE PRIVATE spdlog::spdlog)