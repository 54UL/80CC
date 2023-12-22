add_executable(Bootstrapper
    src/Bootstrapper/EntryPoint.cpp
)

target_include_directories(Bootstrapper
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(Bootstrapper PRIVATE Core)