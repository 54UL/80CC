
add_library(Core STATIC
    scene/SceneManager.hpp
)

target_include_directories(Core
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(Core
    PUBLIC Box2D
    PUBLIC enet
    PUBLIC SDL2
    PUBLIC ImGui
)

