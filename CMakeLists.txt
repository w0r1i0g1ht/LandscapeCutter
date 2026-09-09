cmake_minimum_required(VERSION 4.4)

set(LC_WINDOWS_SDK_FLOOR "10.0.19041.0")
set(LC_EFFECTIVE_WINDOWS_SDK_FLOOR "${LC_WINDOWS_SDK_FLOOR}")
if(DEFINED LC_MIN_WINDOWS_SDK_VERSION)
    if(LC_MIN_WINDOWS_SDK_VERSION VERSION_LESS LC_WINDOWS_SDK_FLOOR)
        message(FATAL_ERROR
            "The LandscapeCutter Windows SDK floor cannot be lowered below "
            "${LC_WINDOWS_SDK_FLOOR}; requested ${LC_MIN_WINDOWS_SDK_VERSION}.")
    endif()
    set(LC_EFFECTIVE_WINDOWS_SDK_FLOOR "${LC_MIN_WINDOWS_SDK_VERSION}")
endif()

project(LandscapeCutter VERSION 0.1.0 LANGUAGES CXX RC)

if(NOT WIN32)
    message(FATAL_ERROR "LandscapeCutter currently supports Windows only.")
endif()

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "LandscapeCutter currently supports x64 builds only.")
endif()

if(MSVC)
    if(NOT DEFINED CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION
       OR CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION STREQUAL "")
        message(FATAL_ERROR
            "Could not determine the selected Windows SDK version. Install Windows SDK "
            "${LC_EFFECTIVE_WINDOWS_SDK_FLOOR} or newer.")
    endif()

    if(CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION VERSION_LESS LC_EFFECTIVE_WINDOWS_SDK_FLOOR)
        message(FATAL_ERROR
            "LandscapeCutter requires Windows SDK ${LC_EFFECTIVE_WINDOWS_SDK_FLOOR} or newer; "
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
find_package(libjpeg-turbo CONFIG REQUIRED)
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
