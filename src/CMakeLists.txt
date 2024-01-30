cmake_minimum_required(VERSION 3.15)
project(80CC_ENGINE_AND_GAME)
set(CMAKE_CXX_STANDARD 17)

# set(CMAKE_CXX_COMPILER, "g++")
set(DEFAULT_ENTRY ON)
# Set the vcpkg root directory and the tripplet
set(VCPKG_ROOT "/workspaces/vcpkg" CACHE STRING "")
set(VCPKG_TARGET_TRIPLET "x64-linux" CACHE STRING "")
set(CMAKE_TOOLCHAIN_FILE "${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")

# Include the vcpkg toolchain file
if(CMAKE_TOOLCHAIN_FILE)
    message("VCPKG READY... TOOLCHAIN FILE PATH [${CMAKE_TOOLCHAIN_FILE}]")
endif()

add_subdirectory(Core)

# Add subdirectories
if(DEFAULT_ENTRY)
    add_subdirectory(Entry)
    add_subdirectory(Game)
else()
    add_subdirectory(Testers)  
endif()
