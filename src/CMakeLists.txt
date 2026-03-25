cmake_minimum_required(VERSION 4.0.2)
project(80CC_ENGINE_AND_GAME)
set(CMAKE_CXX_STANDARD 17)

# 80CC CMAKE COMPILING OPTIONS
option(80CC_UNIT_TEST "Compile as unit tests" OFF)

# GTEST: For Windows: Prevent overriding the parent project's compiler/linker settings
 set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

# Set the vcpkg root directory and the tripplet
# IMPORTANT NOTE: Change the VCPKG_ROOT path to your vcpkg installation path, IF YOU DON DO THIS YOU WONT BE ABLE TO COMPILE ON CLION...
set(VCPKG_ROOT "/workspaces/vcpkg" CACHE STRING "")
#set(VCPKG_TARGET_TRIPLET "x64-windows" CACHE STRING "")
set(CMAKE_TOOLCHAIN_FILE "${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")

if(CMAKE_TOOLCHAIN_FILE)
    message("VCPKG READY... TOOLCHAIN FILE PATH [${CMAKE_TOOLCHAIN_FILE}]")
endif()


add_subdirectory(Core)
if(80CC_UNIT_TEST)
    message("Compiling as testers")
    add_subdirectory(Testers)  
else()
    message("Compiling as stand alone editor")
    add_subdirectory(Entry)
endif()
