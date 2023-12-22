add_library(Game STATIC
    src/modules/game_module.cpp
)

target_include_directories(Game
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(Game PRIVATE Core)