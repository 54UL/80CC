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
set(ENTRY_LIB_DIRS "external/lib/lib80CC_CORE.a")
file(GLOB_RECURSE GAME_SOURCES "src/Modules/**.cpp") 
file(GLOB_RECURSE GAME_HEADERS "include/Modules/**.hpp") 

message("GAME MODULES DETECTED SOURCES")
message("${GAME_SOURCES}")

find_package(spdlog CONFIG REQUIRED)

add_executable(Entry ${ENTRY_HEADERS} ${ENTRY_SOURCES} ${GAME_SOURCES} ${GAME_HEADERS})

target_include_directories (Entry
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/external/include/
    )


# target_link_libraries(Entry PUBLIC 80CC_CORE PRIVATE)
target_link_libraries(Entry PUBLIC ${ENTRY_LIB_DIRS})