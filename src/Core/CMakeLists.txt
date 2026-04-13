cmake_minimum_required(VERSION 3.20)

project(80CC_CORE CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(80CC_CORE_HEADERS
    include/Build/ProjectGenerator.hpp
    include/80CC.hpp
    include/Paths.hpp
    include/Engine.hpp
    include/App/App.hpp
    include/Dependency.hpp
    include/App/EnginePipeline.hpp
    include/App/ExecutionPipeline.hpp
    include/App/SDL2App.hpp
    include/Scene/Api.hpp
    include/Scene/NodeComponentInfo.hpp
    include/Scene/NodeComponent.hpp
    include/Scene/Transform.hpp
    include/Scene/Scene.hpp
    include/Scene/SceneNode.hpp
    include/Scene/Components/RenderableNode.hpp
    include/Scene/Components/RigidBodyComponent.hpp
    include/Scene/Components/SoftBodyComponent.hpp
    include/Scene/Components/AudioSourceComponent.hpp
    include/Scene/Components/AudioListenerComponent.hpp
    include/Audio/AudioManager.hpp
    include/Physics/PhysicsWorld.hpp
    include/Graphics/Rendering/Entities/SoftBodyRenderable.hpp
    include/Scene/Assets/AssetLoader.hpp
    include/Scene/Assets/AssetBuilder.hpp
    include/Scene/Assets/MaterialDef.hpp
    include/Scene/Assets/ResourceCache.hpp
    include/Graphics/Rendering.hpp
    include/Graphics/Rendering/RenderingContext.hpp
    include/Graphics/Rendering/Renderable.hpp
    include/Graphics/Rendering/FrameBuffer.hpp
    include/Graphics/Rendering/PickerBuffer.hpp
    include/Graphics/Rendering/Entities/Camera.hpp
    include/Graphics/Rendering/Entities/Sprite.hpp
    include/Graphics/Rendering/Entities/Grid.hpp
    include/Graphics/Rendering/SpriteBatch.hpp
    include/Graphics/Shading/Shader.hpp
    include/Graphics/Shading/ShaderPipeline.hpp
    include/Input/PlayerInput.hpp
    include/Input/Controls/GhostCamera.hpp
    include/Input/Controls/EditorCamera.hpp
    include/Game/GameModule.hpp
    include/GlobalKeys.hpp
    include/Dependencies/Globals.hpp
    include/Build/BuildStrings.hpp
    include/Build/BuildConfig.hpp
    include/Build/BuildController.hpp
    include/UI/Build/BuildPanelUI.hpp
    include/UI/ConfigurationsWindow.hpp
    include/UI/DevEditor.hpp
    include/UI/BuildPanel.hpp
    include/UI/Console.hpp
    include/UI/EditorPropertyVisitor.hpp
    include/UI/SpriteEditor.hpp
    include/UI/Widgets/PathFieldWidget.hpp
    include/Graphics/Rendering/Entities/SpriteShape.hpp
    include/Scene/PropertySystem.hpp
    include/Dependencies/Resources.hpp
    include/Networking/NetworkManager.hpp
    include/Networking/NetworkComponent.hpp
    include/Benchmark/Benchmark.hpp
    include/Threading/ThreadPool.hpp
    include/Threading/AsyncQueue.hpp
    include/Threading/WorkerThread.hpp
    include/Threading/ThreadRegistry.hpp
    # IMGUI LIBRARY HEADERS
    external/imgui/imgui.h
    external/imgui/imgui_internal.h
    external/imgui/imstb_rectpack.h
    external/imgui/imstb_truetype.h
    external/imgui/imstb_textedit.h
    external/imgui/imconfig.h
    external/imgui/backends/imgui_impl_sdl2.h
    external/imgui/backends/imgui_impl_opengl3.h
    # Portable file dialogs
    external/portable-file-dialogs/portable-file-dialogs.h)

set(80CC_CORE_SOURCES
    src/80CC.cpp
    src/App/SDL2App.cpp
    src/Scene/Transform.cpp
    src/Scene/Scene.cpp
    src/Scene/SceneNode.cpp
    src/Scene/Assets/AssetLoader.cpp
    src/Scene/Assets/AssetBuilder.cpp
    src/Scene/Assets/ResourceCache.cpp
    src/Scene/Components/RenderableNode.cpp
    src/Scene/Components/RigidBodyComponent.cpp
    src/Scene/Components/SoftBodyComponent.cpp
    src/Scene/Components/GravityAttractorComponent.cpp
    src/Scene/Components/AudioSourceComponent.cpp
    src/Scene/Components/AudioListenerComponent.cpp
    src/Scene/Systems/PhysicsSystem.cpp
    src/Scene/Systems/RenderSystem.cpp
    src/Scene/Systems/AudioSystem.cpp
    src/Scene/Systems/NetworkSystem.cpp
    src/Audio/AudioManager.cpp
    src/Physics/PhysicsWorld.cpp
    src/Graphics/Rendering/Entities/SoftBodyRenderable.cpp
    src/Graphics/Rendering.cpp
    src/Graphics/Rendering/Renderable.cpp
    src/Graphics/Rendering/FrameBuffer.cpp
    src/Graphics/Rendering/PickerBuffer.cpp
    src/Graphics/Rendering/Entities/Camera.cpp
    src/Graphics/Rendering/Entities/Sprite.cpp
    src/Graphics/Rendering/Entities/Grid.cpp
    src/Graphics/Rendering/SpriteBatch.cpp
    src/Graphics/Shading/Shader.cpp
    src/Graphics/Shading/ShaderPipeline.cpp
    src/Input/PlayerInput.cpp
    src/Input/Controls/GhostCamera.cpp
    src/Input/Controls/EditorCamera.cpp
    src/Build/BuildController.cpp
    src/UI/Build/BuildPanelUI.cpp
    src/UI/ConfigurationsWindow.cpp
    src/UI/DevEditor.cpp
    src/UI/SpriteEditor.cpp
    src/UI/BuildPanel.cpp
    src/UI/Widgets/PathFieldWidget.cpp
    src/Engine.cpp
    src/Networking/NetworkManager.cpp
    src/Networking/NetworkComponent.cpp
    # IMGUI LIBRARY SOURCES
    external/imgui/imgui.cpp
    external/imgui/imgui_demo.cpp
    external/imgui/imgui_draw.cpp
    external/imgui/imgui_tables.cpp
    external/imgui/imgui_widgets.cpp
    external/imgui/backends/imgui_impl_sdl2.cpp
    external/imgui/backends/imgui_impl_opengl3.cpp)

# PACKAGES...
find_package(OpenAL MODULE REQUIRED)  # MODULE = bypass vcpkg's stale OpenALConfig.cmake
find_package(spdlog CONFIG REQUIRED)
find_package(SDL2 REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(GLEW REQUIRED)
find_package(Stb REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(cereal CONFIG REQUIRED)
find_package(unofficial-enet CONFIG REQUIRED)
find_package(Bullet CONFIG REQUIRED)

# DIR INCLUDES (avoid this...)
include_directories(${GLEW_INCLUDE_DIRS})
include_directories(src/Core/external/imgui/**)

# ADD THE CORE LIBRARY 
add_library(80CC_CORE STATIC ${80CC_CORE_SOURCES} ${80CC_CORE_HEADERS})

# TARGET INCLUDES
target_include_directories(80CC_CORE
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    PUBLIC ${BULLET_INCLUDE_DIRS}
    PUBLIC ${SDL2_INCLUDE_DIRS}
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui
           ${CMAKE_CURRENT_SOURCE_DIR}/external/portable-file-dialogs
    PUBLIC ${Stb_INCLUDE_DIR}
    PUBLIC ${GLM_INCLUDE_DIRS})

target_link_libraries(80CC_CORE
    PUBLIC ${SDL2_LIBRARIES}
    PUBLIC GLEW::GLEW
    PUBLIC glm::glm
    PUBLIC spdlog::spdlog
    PUBLIC cereal::cereal
    PUBLIC unofficial::enet::enet
    PUBLIC ${BULLET_LIBRARIES}
    PUBLIC OpenAL::OpenAL)