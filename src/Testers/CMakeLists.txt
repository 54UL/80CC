project(Testers CXX)
set(CMAKE_CXX_STANDARD 17)	

# gtests
include(CTest)
include(GoogleTest)
enable_testing()

set(Testers_SOURCES 
    UnitTesting/SceneNodeTest.cpp
    UnitTesting/PathsManagerTest.cpp
)

add_executable(Testers ${Testers_SOURCES})

target_link_libraries(Testers PUBLIC gtest_main 80CC_CORE)
gtest_discover_tests(Testers)