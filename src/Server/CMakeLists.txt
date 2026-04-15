cmake_minimum_required(VERSION 4.0.2)

project(80CC_Server)
set(CMAKE_CXX_STANDARD 17)

set(SERVER_SOURCES
    src/main.cpp
)

find_package(spdlog CONFIG REQUIRED)

add_executable(80CC_Server ${SERVER_SOURCES})

target_include_directories(80CC_Server
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(80CC_Server PUBLIC 80CC_CORE)

# Mark this as a dedicated server build (no window, console app)
target_compile_definitions(80CC_Server PRIVATE
    COMPILE_80CC_DEDICATED_SERVER=1
    COMPILE_80CC_STAND_ALONE_EXECUTABLE=1
)
