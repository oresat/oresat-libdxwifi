cmake_minimum_required(VERSION 3.2)

project(dxwifi)

# Define Test Variants
set(CMAKE_C_FLAGS_TESTDEBUG "-Wall -Werror -g -DDXWIFI_TESTS")
set(CMAKE_C_FLAGS_TESTREL   "-Wall -Werror -O3 -DNDEBUG -DDXWIFI_TESTS")

# Determine Build configuration
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING
        "Build options are: Debug, Release, RelWithDebInfo, MinSizeRel, TestDebug, TestRel"
        FORCE)
endif()

# Versioning
set(DXWIFI_VERSION_MAJOR "0")
set(DXWIFI_VERSION_MINOR "1")
set(DXWIFI_VERSION_PATCH "0")
set(DXWIFI_VERSION_EXTRA "alpha")
string(CONCAT DXWIFI_VERSION 
    "${DXWIFI_VERSION_MAJOR}."
    "${DXWIFI_VERSION_MINOR}."
    "${DXWIFI_VERSION_PATCH}-"
    "${DXWIFI_VERSION_EXTRA}-"
    "${CMAKE_BUILD_TYPE}"
    )
add_compile_definitions(DXWIFI_VERSION="${DXWIFI_VERSION}")

# User Options
option(LIBDXWIFI_DISABLE_ASSERTS    "Disable Assert Functions"     OFF)
option(LIBDXWIFI_DISABLE_LOGGING    "Disable Logging"              OFF)
option(INSTALL_SYSLOG_CONFIG        "Include configuration files for rsyslog and logrotate" OFF)

if(LIBDXWIFI_DISABLE_ASSERTS)
    add_compile_definitions(LIBDXWIFI_DISABLE_ASSERTS)
endif()

if(LIBDXWIFI_DISABLE_LOGGING)
    add_compile_definitions(LIBDXWIFI_DISABLE_LOGGING)
endif()

# Include and output directories
include_directories(${PROJECT_SOURCE_DIR})

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/lib/${CMAKE_BUILD_TYPE})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/lib/${CMAKE_BUILD_TYPE})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin/${CMAKE_BUILD_TYPE})

# CPack configuration
set(CPACK_GENERATOR "DEB")
set(CPACK_PACKAGE_NAME "oresat-dxwifi")
set(CPACK_PACKAGE_VERSION ${DXWIFI_VERSION})
set(CPACK_PACKAGE_CONTACT "PSAS <oresat@pdx.edu>")
set(CPACK_DEBIAN_PACKAGE_DESCRIPTION "Oresat Live DxWiFi")
set(CPACK_DEBIAN_PACKAGE_SECTION "net")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libpcap-dev (>= 1.8.1-6)")
set(CPACK_DEB_COMPONENT_INSTALL ON)
set(CPACK_DEBIAN_ENABLE_COMPONENT_DEPENDS ON)
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/oresat/oresat-dxwifi-software")
set(CPACK_DEBIAN_RX_PACKAGE_CONTROL_EXTRA 
    "${CMAKE_SOURCE_DIR}/config/debian/rx/postinst" 
    "${CMAKE_SOURCE_DIR}/config/debian/rx/postrm" 
    "${CMAKE_SOURCE_DIR}/config/debian/rx/prerm"
    )
set(CPACK_DEBIAN_TX_PACKAGE_CONTROL_EXTRA 
    "${CMAKE_SOURCE_DIR}/config/debian/tx/postinst" 
    "${CMAKE_SOURCE_DIR}/config/debian/tx/postrm" 
    "${CMAKE_SOURCE_DIR}/config/debian/tx/prerm"
    )
include(CPack)

# Sub targets
add_subdirectory(libdxwifi)
add_subdirectory(dxwifi/tx)
add_subdirectory(dxwifi/rx)
