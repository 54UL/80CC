cmake_minimum_required(VERSION 3.20)
project(GravityModule CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(GravityModule SHARED
    src/GravityModule.cpp
    src/GravityDynamicsSystem.cpp
    include/GravityModule.hpp
    include/GravityDynamicsSystem.hpp
    include/GravityConfig.hpp
    include/FusionDataComponent.hpp)

target_include_directories(GravityModule
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

# ETTYCC_MODULE_EXPORT drives the dllexport/dllimport switch in GameModule.hpp
target_compile_definitions(GravityModule PRIVATE ETTYCC_MODULE_EXPORT)

# Link against the engine core (headers + static lib)
target_link_libraries(GravityModule PRIVATE 80CC_CORE)

# After build, copy the DLL into the engine's modules/ directory so the
# ModuleLoader picks it up automatically on next startup (or hot-reloads it).
set(MODULES_OUTPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../modules")
add_custom_command(TARGET GravityModule POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${MODULES_OUTPUT_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:GravityModule>
        "${MODULES_OUTPUT_DIR}/$<TARGET_FILE_NAME:GravityModule>")
