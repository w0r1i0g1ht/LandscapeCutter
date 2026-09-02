cmake_minimum_required(VERSION 4.4)

project(LandscapeCutter VERSION 0.1.0 LANGUAGES CXX RC)

if(NOT WIN32)
    message(FATAL_ERROR "LandscapeCutter currently supports Windows only.")
endif()

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "LandscapeCutter currently supports x64 builds only.")
endif()

set(LC_MIN_WINDOWS_SDK_VERSION "10.0.19041.0" CACHE STRING
    "Minimum supported Windows SDK version")
if(MSVC)
    if(NOT DEFINED CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION
       OR CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION STREQUAL "")
        message(FATAL_ERROR
            "Could not determine the selected Windows SDK version. Install Windows SDK "
            "${LC_MIN_WINDOWS_SDK_VERSION} or newer.")
    endif()

    if(CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION VERSION_LESS LC_MIN_WINDOWS_SDK_VERSION)
        message(FATAL_ERROR
            "LandscapeCutter requires Windows SDK ${LC_MIN_WINDOWS_SDK_VERSION} or newer; "
            "selected ${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}.")
    endif()

    message(STATUS "Using Windows SDK ${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 6.8.2 EXACT REQUIRED COMPONENTS Core Gui Widgets)
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
