project(Entry)


set(ENTRY_HEADERS 
    include/Bootstrapper/EntryPoint.hpp
)

set(ENTRY_SOURCES 
    src/Bootstrapper/EntryPoint.cpp
)
find_package(spdlog CONFIG REQUIRED)

add_executable(Entry ${ENTRY_HEADERS} ${ENTRY_SOURCES})

target_include_directories(Entry
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(Entry PUBLIC 80CC_CORE PRIVATE spdlog::spdlog)