cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_COMPILER clang++)

project(Generator)

# Find requierd packages (llvm and clang...)
find_package(LLVM REQUIRED CONFIG)
find_package(Clang REQUIRED CONFIG)

# Set compiler flags for LLVM
list(APPEND CMAKE_CXX_FLAGS "${LLVM_DEFINITIONS}")

include_directories(${LLVM_INCLUDE_DIRS})

add_definitions(${LLVM_DEFINITIONS})
add_executable(Generator main.cpp)

target_link_libraries(Generator
    ${LLVM_LIBS}
    clangTooling
    clangFrontend
    clangASTMatchers
    clangBasic
    clangAST
    clangSerialization
    clangLex
    clangParse
    clangSema
    clangEdit
    clangDriver
    clangAnalysis
    clangRewrite
    clangRewriteFrontend
    clangStaticAnalyzerFrontend
    clangStaticAnalyzerCheckers
    clangStaticAnalyzerCore
    )


