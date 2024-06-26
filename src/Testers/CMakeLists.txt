cmake_minimum_required(VERSION 3.15)

project(Testers CXX)
set(CMAKE_CXX_STANDARD 17)	

option(UNIT_TEST_OR_TEST_BED_CODE "Compile as unit tests or stand alone examples to be run" ON)

# gtests
include(CTest)
include(GoogleTest)
enable_testing()

set(Testers_UNIT_TEST_SOURCES
    UnitTesting/ResourcesTests.cpp
    UnitTesting/SceneNodeTest.cpp
    )

set(Testers_STAND_ALONE_SOURCES 
    # StandAlone/cereal_lib_example.cpp
    StandAlone/enet_rpc_example.cpp
    )   

if (UNIT_TEST_OR_TEST_BED_CODE)
    message("Compiling Unit tests")
    set(Testers_SOURCES ${Testers_UNIT_TEST_SOURCES})
else()
    message("Compiling Stand alone")
    set(Testers_SOURCES ${Testers_STAND_ALONE_SOURCES})
endif()

add_executable(Testers ${Testers_SOURCES})

target_link_libraries(Testers PUBLIC gtest_main 80CC_CORE)
gtest_discover_tests(Testers)