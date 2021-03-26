file(GLOB rx_sources ./*)

set(DXWIFI_RX_VERSION "0.1.0-alpha-${CMAKE_BUILD_TYPE}")

add_compile_definitions(DXWIFI_RX_VERSION="${DXWIFI_RX_VERSION}")

add_executable(rx ${rx_sources})

target_link_libraries(rx dxwifi)

install(TARGETS rx DESTINATION /usr/local/bin)

set(CPACK_GENERATOR "DEB")
set(CPACK_PACKAGE_NAME "oresat-live-tx")
set(CPACK_PACKAGE_VERSION ${DXWIFI_TX_VERSION})
set(CPACK_DEBIAN_PACKAGE_DESCRIPTION "OreSat Live DxWiFi transmission service")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "PSAS <oresat@pdx.edu>")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libpcap-dev (>= 1.8.1-6)")
set(CPACK_DEBIAN_PACKAGE_SECTION "net")
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/oresat/oresat-dxwifi-software")
set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA "${CMAKE_SOURCE_DIR}/config/debian/postinst" "${CMAKE_SOURCE_DIR}/config/debian/postrm" "${CMAKE_SOURCE_DIR}/config/debian/prerm")
execute_process(COMMAND dpkg --print-architecture OUTPUT_VARIABLE CPACK_DEBIAN_PACKAGE_ARCHITECTURE OUTPUT_STRIP_TRAILING_WHITESPACE)
if ("${CPACK_DEBIAN_PACKAGE_ARCHITECTURE}" STREQUAL "")
    set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "i386")
endif()
set(CPACK_PACKAGE_FILE_NAME "${CPACK_DEBIAN_PACKAGE_NAME}_${CPACK_DEBIAN_PACKAGE_VERSION}_${CPACK_DEBIAN_PACKAGE_ARCHITECTURE}")

include(CPack)
