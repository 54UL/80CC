project(Testers CXX)

# set(Testers_HEADERS 
#     include/Modules/HelloWorldModule.hpp
# )

set(Testers_SOURCES 
    StandAlone/bullet_softbodies.cpp
)
find_package(SDL2 REQUIRED)
find_package(GLUT REQUIRED)
find_package(Bullet CONFIG REQUIRED)

add_executable(Testers ${Testers_SOURCES})

# target_include_directories(Testers
#     PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
# )

target_link_libraries(Testers PRIVATE 80CC_CORE ${BULLET_LIBRARIES} GLUT::GLUT SDL2::SDL2)