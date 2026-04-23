cmake_minimum_required(VERSION 3.20)
project(SampleModule CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(SampleModule SHARED
    src/SampleModule.cpp
    include/SampleModule.hpp)

target_include_directories(SampleModule
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

# ETTYCC_MODULE_EXPORT drives the dllexport/dllimport switch in GameModule.hpp
target_compile_definitions(SampleModule PRIVATE ETTYCC_MODULE_EXPORT)

# Link against the engine core (headers + static lib)
target_link_libraries(SampleModule PRIVATE 80CC_CORE)

# After build, copy the DLL into the engine's modules/ directory so the
# ModuleLoader picks it up automatically on next startup (or hot-reloads it).
set(MODULES_OUTPUT_DIR "${CMAKE_SOURCE_DIR}/../assets/modules")
add_custom_command(TARGET SampleModule POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${MODULES_OUTPUT_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SampleModule>
        "${MODULES_OUTPUT_DIR}/$<TARGET_FILE_NAME:SampleModule>")
