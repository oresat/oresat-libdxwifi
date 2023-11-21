find_package(PythonLibs REQUIRED)
find_package(pybind11 REQUIRED)

file(GLOB tx_cpp_sources ./*)
file(GLOB tx_sources ./*)

include_directories(${PYTHON_INCLUDE_DIRS})
include_directories(${CMAKE_SOURCE_DIR}libdxwifi/details)

set(TX_DESCRIPTION "OreSat Live DxWiFi transmission program")


add_library(tx_cpp_lib STATIC ${tx_cpp_sources})
target_include_directories(tx_cpp_lib PUBLIC ${PYTHON_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR}libdxwifi/details)
target_link_libraries(tx_cpp_lib ${PYTHON_LIBRARIES} dxwifi)

add_executable(tx ${tx_sources})
target_link_libraries(tx dxwifi tx_cpp_lib)


pybind11_add_module(tx_module tx_wrapper.cpp)
target_link_libraries(tx_module PRIVATE pybind11::module ${PYTHON_LIBRARIES} dxwifi tx_cpp_lib)

set_target_properties(tx_module PROPERTIES PREFIX ""
                                           SUFFIX ".so"
                                           OUTPUT_NAME "tx_module"
                                           LIBRARY_OUTPUT_DIRECTORY ${DXWIFI_RUNTIME_OUTPUT_DIRECTORY}
)

set_target_properties( tx 
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${DXWIFI_RUNTIME_OUTPUT_DIRECTORY}
    )

install(
    TARGETS tx 
    DESTINATION /usr/bin
    COMPONENT TX
    )

install(
    FILES ${CMAKE_SOURCE_DIR}/config/network/50-ath9k-htc.link
    DESTINATION /etc/systemd/network
    COMPONENT TX
    )

install(
    FILES ${CMAKE_SOURCE_DIR}/startmonitor.sh
    DESTINATION /usr/bin/
    PERMISSIONS OWNER_WRITE OWNER_READ OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
    RENAME "startmonitor"
    COMPONENT TX
    )

install(
    FILES ${CMAKE_SOURCE_DIR}/config/systemd/oresat-dxwifi-txd.service
    DESTINATION /lib/systemd/system
    COMPONENT TX
    )

if(INSTALL_SYSLOG_CONFIG)
    install(
        FILES ${CMAKE_SOURCE_DIR}/config/logrotate/dxwifi.conf
        DESTINATION /etc/logrotate.d
        COMPONENT TX
        )
    install(
        FILES ${CMAKE_SOURCE_DIR}/config/rsyslog/00-dxwifi.conf
        DESTINATION /etc/rsyslog.d
        COMPONENT TX
        )
endif()


cpack_add_component(
    TX
    DISPLAY_NAME TX 
    DESCRIPTION ${TX_DESCRIPTION}
    GROUP TX 
    INSTALL_TYPES Full
    )
