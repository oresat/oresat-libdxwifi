cmake_minimum_required(VERSION 3.2)

set(CMAKE_C_FLAGS_TESTDEBUG "-Wall -Werror -g -DDXWIFI_TESTS")
set(CMAKE_C_FLAGS_TESTREL   "-Wall -Werror -O3 -DNDEBUG -DDXWIFI_TESTS")

project(dxwifi)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING
        "Build options are: Debug, Release, RelWithDebInfo, MinSizeRel, TestDebug, TestRel"
        FORCE)
endif()

include_directories(${PROJECT_SOURCE_DIR})

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/lib/${CMAKE_BUILD_TYPE})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/lib/${CMAKE_BUILD_TYPE})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin/${CMAKE_BUILD_TYPEn})


add_subdirectory(libdxwifi)
add_subdirectory(dxwifi/tx)
add_subdirectory(dxwifi/rx)
