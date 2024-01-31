project(80CC_CORE CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_COMPILER gcc)

set(80CC_CORE_HEADERS
    include/80CC.hpp
    include/App/App.hpp
    include/App/EnginePipeline.hpp
    include/App/ExecutionPipeline.hpp
    include/App/SDL2App.hpp
    include/Scene/Transform.hpp
    include/Graphics/Rendering.hpp
    include/Graphics/Rendering/RenderingContext.hpp
    include/Graphics/Rendering/Renderable.hpp
    include/Graphics/Rendering/Entities/Camera.hpp
    include/Graphics/Rendering/Entities/Sprite.hpp
    include/Graphics/Shading/Shader.hpp
    include/Graphics/Shading/ShaderPipeline.hpp
    include/Input/PlayerInput.hpp
    include/Input/Controls/GhostCamera.hpp
    include/UI/DevEditor.hpp
    include/Engine.hpp
    # IMGUI LIBRARY HEADERS
    external/imgui/imgui.h
    external/imgui/imgui_internal.h
    external/imgui/imstb_rectpack.h
    external/imgui/imstb_truetype.h
    external/imgui/imstb_textedit.h
    external/imgui/imconfig.h
    external/imgui/backends/imgui_impl_sdl2.h
    external/imgui/backends/imgui_impl_opengl3.h
)

set(80CC_CORE_SOURCES
    src/80CC.cpp
    src/App/SDL2App.cpp
    src/Scene/Transform.cpp
    src/Graphics/Rendering.cpp
    # src/Graphics/Renderable.cpp
    src/Graphics/Rendering/Entities/Camera.cpp
    src/Graphics/Rendering/Entities/Sprite.cpp
    src/Graphics/Shading/Shader.cpp
    src/Graphics/Shading/ShaderPipeline.cpp
    src/Input/PlayerInput.cpp
    src/Input/Controls/GhostCamera.cpp
    src/UI/DevEditor.cpp
    src/Engine.cpp
    # IMGUI LIBRARY SOURCES
    external/imgui/imgui.cpp
    external/imgui/imgui_demo.cpp
    external/imgui/imgui_draw.cpp
    external/imgui/imgui_tables.cpp
    external/imgui/imgui_widgets.cpp
    external/imgui/backends/imgui_impl_sdl2.cpp
    external/imgui/backends/imgui_impl_opengl3.cpp
)

# PACKAGES...
find_package(spdlog CONFIG REQUIRED)
find_package(SDL2 REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(GLEW REQUIRED)
find_package(Stb REQUIRED)

# Include GLEW headers
include_directories(${GLEW_INCLUDE_DIRS})
# Include Imgui headers
include_directories(src/Core/external/imgui/**)

# find_package(bgfx CONFIG REQUIRED)
# ADD THE CORE LIBRARY 
add_library(80CC_CORE STATIC ${80CC_CORE_SOURCES} ${80CC_CORE_HEADERS})

# TARGET INCLUDES
target_include_directories(80CC_CORE
    PUBLIC ${SDL2_INCLUDE_DIRS}
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui
    PRIVATE ${Stb_INCLUDE_DIR}
)

target_link_libraries(80CC_CORE
    PRIVATE ${SDL2_LIBRARIES}
    PRIVATE spdlog::spdlog
    PRIVATE glm::glm
    PRIVATE GLEW::GLEW
)