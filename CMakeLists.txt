cmake_minimum_required(VERSION 3.28)

project(LandscapeCutter VERSION 0.1.0 LANGUAGES CXX RC)

if(NOT WIN32)
    message(FATAL_ERROR "LandscapeCutter currently supports Windows only.")
endif()

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "LandscapeCutter currently supports x64 builds only.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Widgets)
if(Qt6_VERSION VERSION_GREATER_EQUAL "6.9.0")
    message(FATAL_ERROR "Milestone 0 is pinned to the Qt 6.8 series.")
endif()
message(STATUS "Using Qt ${Qt6_VERSION}")

find_package(spdlog CONFIG REQUIRED)
find_package(wil CONFIG REQUIRED)

include(cmake/Warnings.cmake)
add_subdirectory(src)

include(CTest)
if(BUILD_TESTING)
    find_package(Catch2 3 CONFIG REQUIRED)
    add_subdirectory(tests)
endif()
