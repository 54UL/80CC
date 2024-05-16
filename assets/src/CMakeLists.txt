project(Game CXX)

set(GAME_HEADERS 
    include/Modules/HelloWorldModule.hpp
)

set(GAME_SOURCES 
    src/Modules/HelloWorldModule.cpp
)

add_library(Game STATIC ${GAME_HEADERS} ${GAME_SOURCES})

target_include_directories(Game
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(Game PRIVATE 80CC_CORE)