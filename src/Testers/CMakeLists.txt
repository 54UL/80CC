project(Testers CXX)
set(CMAKE_CXX_STANDARD 17)	

# gtests
include(CTest)
include(GoogleTest)
enable_testing()

# headers and source
# set(Testers_HEADERS 
#     include/Modules/HelloWorldModule.hpp
# )

set(Testers_SOURCES 
    UnitTesting/SceneNodeTest.cpp
)

# libraries...
# find_package(SDL2 REQUIRED)
# find_package(GLUT REQUIRED)
# find_package(Bullet CONFIG REQUIRED)

add_executable(Testers ${Testers_SOURCES})

# target_include_directories(Testers
#     PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
# )

target_link_libraries(Testers PUBLIC gtest_main 80CC_CORE)
gtest_discover_tests(Testers)