cmake_minimum_required(VERSION 3.21)
project(CASLStudio VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(CASL_BUILD_UI "Build the Qt Widgets desktop application" ON)
option(CASL_BUILD_TESTS "Build core unit tests" ON)

add_subdirectory(core)

if(CASL_BUILD_UI)
    add_subdirectory(app)
endif()

if(CASL_BUILD_TESTS)
    enable_testing()
    add_subdirectory(core/tests)
    add_subdirectory(core/tools)
endif()
