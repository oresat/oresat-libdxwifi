file(GLOB rx_sources ./*)

set(RX_DESCRIPTION "OreSat Live DxWiFi receiver program")

add_executable(rx ${rx_sources})

target_link_libraries(rx dxwifi)

set_target_properties( rx 
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${DXWIFI_RUNTIME_OUTPUT_DIRECTORY}
    )

install(
    TARGETS rx 
    DESTINATION /usr/bin
    COMPONENT RX
    )

if(INSTALL_SYSLOG_CONFIG)
    install(
        FILES ${CMAKE_SOURCE_DIR}/config/logrotate/dxwifi.conf
        DESTINATION /etc/logrotate.d
        COMPONENT RX
        )
    install(
        FILES ${CMAKE_SOURCE_DIR}/config/rsyslog/00-dxwifi.conf
        DESTINATION /etc/rsyslog.d
        COMPONENT RX
        )
endif()

cpack_add_component(
    RX
    DISPLAY_NAME RX 
    DESCRIPTION ${RX_DESCRIPTION}
    GROUP RX 
    INSTALL_TYPES Full
    )
