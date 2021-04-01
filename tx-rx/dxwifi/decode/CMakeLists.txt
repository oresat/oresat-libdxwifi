file(GLOB decode_sources ./*)

add_executable(decode ${decode_sources})

set_target_properties(decode
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${DXWIFI_RUNTIME_OUTPUT_DIRECTORY}
    )

target_link_libraries(decode dxwifi)

install(
    TARGETS decode
    DESTINATION /usr/bin
    COMPONENT DECODE
    )

if(INSTALL_SYSLOG_CONFIG)
    install(
        FILES ${CMAKE_SOURCE_DIR}/config/logrotate/dxwifi.conf
        DESTINATION /etc/logrotate.d
        COMPONENT DECODE
        )
    install(
        FILES ${CMAKE_SOURCE_DIR}/config/rsyslog/00-dxwifi.conf
        DESTINATION /etc/rsyslog.d
        COMPONENT DECODE
        )
endif()

cpack_add_component(
    DECODE
    DISPLAY_NAME DECODE
    DESCRIPTION ${RX_DESCRIPTION}
    GROUP DECODE
    INSTALL_TYPES Full
    )
