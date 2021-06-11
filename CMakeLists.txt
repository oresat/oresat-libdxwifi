cmake_minimum_required(VERSION 3.10)

# Define Test Variants
set(CMAKE_C_FLAGS_TESTDEBUG "-Wall -Werror -g -DDXWIFI_TESTS")
set(CMAKE_C_FLAGS_TESTREL   "-Wall -Werror -O3 -DNDEBUG -DDXWIFI_TESTS")

# Cross-Compilation support, only supports armhf at the moment
if(PLATFORM STREQUAL "armhf")
    if(NOT LINUX_ARM_ENV_ROOT)
        set(LINUX_ARM_ENV_ROOT /mnt/data/armhf)
    endif()
    set(CMAKE_LIBRARY_ARCHITECTURE arm-linux-gnueabihf)
    set(CMAKE_TOOLCHAIN_FILE platform/arm-linux-gnueabihf-gcc.cmake)
endif()

# Project directive will detect toolchain if we did not specify a platform
project(dxwifi)

# Determine Build configuration
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING
        "Build options are: Debug, Release, RelWithDebInfo, MinSizeRel, TestDebug, TestRel"
        FORCE)
endif()

# Versioning
set(DXWIFI_VERSION_MAJOR "1")
set(DXWIFI_VERSION_MINOR "0")
set(DXWIFI_VERSION_PATCH "0")
set(DXWIFI_VERSION_REVISION "0")

string(CONCAT CPACK_PACKAGE_VERSION 
    "${DXWIFI_VERSION_MAJOR}."
    "${DXWIFI_VERSION_MINOR}."
    "${DXWIFI_VERSION_PATCH}-"
    "${DXWIFI_VERSION_REVISION}"
    )

string(CONCAT DXWIFI_VERSION 
    "${CPACK_PACKAGE_VERSION}-"
    "${CMAKE_BUILD_TYPE}"
    )

add_compile_definitions(DXWIFI_VERSION="${DXWIFI_VERSION}")

# User Options
option(OPENFEC_DEBUG_MODE            "Build OpenFEC in Debug mode"  OFF)
option(LIBDXWIFI_DISABLE_ASSERTS     "Disable Assert Functions"     OFF)
option(LIBDXWIFI_DISABLE_LOGGING     "Disable Logging"              OFF)
option(INSTALL_SYSLOG_CONFIG         "Include configuration files for rsyslog and logrotate" OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(LIBDXWIFI_DISABLE_ASSERTS)
    add_compile_definitions(LIBDXWIFI_DISABLE_ASSERTS)
endif()

if(LIBDXWIFI_DISABLE_LOGGING)
    add_compile_definitions(LIBDXWIFI_DISABLE_LOGGING)
endif()

# Include and output directories
include_directories(
    ${PROJECT_SOURCE_DIR} 
    ${PROJECT_SOURCE_DIR}/openfec/src/lib_common
    ${PROJECT_SOURCE_DIR}/openfec/src/lib_stable
    )

set(DXWIFI_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin/${CMAKE_BUILD_TYPE})
set(DXWIFI_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/lib/${CMAKE_BUILD_TYPE})
set(DXWIFI_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/lib/${CMAKE_BUILD_TYPE})


# Deal with dependencies
find_library(LIB_PCAP pcap)
if(NOT LIB_PCAP)
    message(FATAL_ERROR "pcap not found. Please install libpcap-dev")
endif()

find_library(LIB_GPIOD gpiod)
if(NOT LIB_GPIOD)
    message(FATAL_ERROR "gpiod not found. Please install libgpiod-dev")
endif()

# Global CPack configuration
set(CPACK_GENERATOR "DEB")
set(CPACK_PACKAGE_NAME "oresat-dxwifi")
set(CPACK_PACKAGE_CONTACT "PSAS <oresat@pdx.edu>")
set(CPACK_DEBIAN_PACKAGE_DESCRIPTION "Oresat Live DxWiFi")
set(CPACK_DEBIAN_PACKAGE_SECTION "net")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libpcap-dev (>= 1.8.1-6), iw")
set(CPACK_DEB_COMPONENT_INSTALL ON)
set(CPACK_DEBIAN_ENABLE_COMPONENT_DEPENDS ON)
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/oresat/oresat-dxwifi-software")

# Determine architecture
if(PLATFORM STREQUAL "armhf") 
    set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "armhf")
else() # Else use host architecture
    execute_process(COMMAND dpkg --print-architecture OUTPUT_VARIABLE CPACK_DEBIAN_PACKAGE_ARCHITECTURE OUTPUT_STRIP_TRAILING_WHITESPACE)
    if ("${CPACK_DEBIAN_PACKAGE_ARCHITECTURE}" STREQUAL "")
        set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "i386")
    endif()
endif()

# RX component CPack config
set(CPACK_DEBIAN_RX_PACKAGE_CONTROL_EXTRA 
    "${CMAKE_SOURCE_DIR}/config/debian/rx/postinst" 
    "${CMAKE_SOURCE_DIR}/config/debian/rx/postrm" 
    "${CMAKE_SOURCE_DIR}/config/debian/rx/prerm"
    )
set(CPACK_DEBIAN_RX_FILE_NAME "${CPACK_PACKAGE_NAME}-rx_${CPACK_PACKAGE_VERSION}_${CPACK_DEBIAN_PACKAGE_ARCHITECTURE}.deb")

# TX component CPack config
set(CPACK_DEBIAN_TX_PACKAGE_CONTROL_EXTRA 
    "${CMAKE_SOURCE_DIR}/config/debian/tx/postinst" 
    "${CMAKE_SOURCE_DIR}/config/debian/tx/postrm" 
    "${CMAKE_SOURCE_DIR}/config/debian/tx/prerm"
    )
set(CPACK_DEBIAN_TX_FILE_NAME "${CPACK_PACKAGE_NAME}-tx_${CPACK_PACKAGE_VERSION}_${CPACK_DEBIAN_PACKAGE_ARCHITECTURE}.deb")

# Encode component CPack config
set(CPACK_DEBIAN_ENCODE_PACKAGE_CONTROL_EXTRA 
    "${CMAKE_SOURCE_DIR}/config/debian/encode/postinst" 
    "${CMAKE_SOURCE_DIR}/config/debian/encode/postrm" 
    "${CMAKE_SOURCE_DIR}/config/debian/encode/prerm"
    )
set(CPACK_DEBIAN_ENCODE_FILE_NAME "${CPACK_PACKAGE_NAME}-encode_${CPACK_PACKAGE_VERSION}_${CPACK_DEBIAN_PACKAGE_ARCHITECTURE}.deb")

# Encode component CPack config
set(CPACK_DEBIAN_DECODE_PACKAGE_CONTROL_EXTRA 
    "${CMAKE_SOURCE_DIR}/config/debian/decode/postinst" 
    "${CMAKE_SOURCE_DIR}/config/debian/decode/postrm" 
    "${CMAKE_SOURCE_DIR}/config/debian/decode/prerm"
    )
set(CPACK_DEBIAN_DECODE_FILE_NAME "${CPACK_PACKAGE_NAME}-decode_${CPACK_PACKAGE_VERSION}_${CPACK_DEBIAN_PACKAGE_ARCHITECTURE}.deb")

include(CPack)

# Sub targets
add_subdirectory(rscode)
add_subdirectory(openfec)
add_subdirectory(libdxwifi)
add_subdirectory(dxwifi/tx)
add_subdirectory(dxwifi/rx)
add_subdirectory(dxwifi/encode)
add_subdirectory(dxwifi/decode)
