project(80CC_CORE CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_COMPILER gcc)

set(80CC_CORE_HEADERS
    include/80CC.hpp
    include/App/SDL2App.hpp
    include/Graphics/Rendering/Rendering.hpp
    include/Graphics/Rendering/Entities/Camera.hpp
    include/Graphics/Rendering/Entities/Mesh.hpp
    include/Graphics/Shading/Shader.hpp
)

set(80CC_CORE_SOURCES
    src/80CC.cpp
    src/App/SDL2App.cpp
)

# PACKAGES...
find_package(spdlog CONFIG REQUIRED)
find_package(SDL2 REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(GLEW REQUIRED)
find_package(imgui CONFIG REQUIRED)

# Include GLEW headers
include_directories(${GLEW_INCLUDE_DIRS})

# find_package(bgfx CONFIG REQUIRED)
# ADD THE CORE LIBRARY 
add_library(80CC_CORE STATIC ${80CC_CORE_SOURCES} ${80CC_CORE_HEADERS})

# TARGET INCLUDES
target_include_directories(80CC_CORE
    PUBLIC ${SDL2_INCLUDE_DIRS}
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(80CC_CORE
    PRIVATE ${SDL2_LIBRARIES}
    PRIVATE spdlog::spdlog
    PRIVATE glm::glm
    # PRIVATE imgui::imgui
    PRIVATE GLEW::GLEW
    # PRIVATE spdlog::spdlog_header_only
    PRIVATE imgui::imgui
)