file(GLOB tx_sources ./*)

set(TX_DESCRIPTION "OreSat Live DxWiFi transmission program")

add_executable(tx ${tx_sources})

target_link_libraries(tx dxwifi)

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
