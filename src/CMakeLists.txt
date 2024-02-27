cmake_minimum_required(VERSION 3.15)
project(80CC_ENGINE_AND_GAME)
set(CMAKE_CXX_STANDARD 17)

# gtests
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/609281088cfefc76f9d0ce82e1ff6c30cc3591e5.zip
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# set(CMAKE_CXX_COMPILER, "g++")
set(TESTS ON)
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
if(TESTS)
    add_subdirectory(Testers)  
else()
    add_subdirectory(Entry)
    add_subdirectory(Game)
endif()
