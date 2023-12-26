cmake_minimum_required(VERSION 3.15)
project(80CC_ENGINE_AND_GAME)

# Set the vcpkg root directory and the tripplet
set(VCPKG_ROOT "/workspaces/vcpkg" CACHE STRING "")
set(VCPKG_TARGET_TRIPLET "x64-linux" CACHE STRING "")
set(CMAKE_TOOLCHAIN_FILE "${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")

# Include the vcpkg toolchain file
if(CMAKE_TOOLCHAIN_FILE)
    message("VCPKG READY... TOOLCHAIN FILE PATH [${CMAKE_TOOLCHAIN_FILE}]")
endif()

# Add subdirectories
add_subdirectory(Core)
add_subdirectory(Entry)
add_subdirectory(Game)