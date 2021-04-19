file(GLOB encode_sources ./*)

add_executable(encode ${encode_sources})

set_target_properties(encode
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${DXWIFI_RUNTIME_OUTPUT_DIRECTORY}
    )

target_link_libraries(encode dxwifi)

install(
    TARGETS encode
    DESTINATION /usr/bin
    COMPONENT ENCODE
    )

if(INSTALL_SYSLOG_CONFIG)
    install(
        FILES ${CMAKE_SOURCE_DIR}/config/logrotate/dxwifi.conf
        DESTINATION /etc/logrotate.d
        COMPONENT ENCODE
        )
    install(
        FILES ${CMAKE_SOURCE_DIR}/config/rsyslog/00-dxwifi.conf
        DESTINATION /etc/rsyslog.d
        COMPONENT ENCODE
        )
endif()

cpack_add_component(
    DECODE
    DISPLAY_NAME ENCODE
    DESCRIPTION "DxWiFI FEC Encoding program"
    GROUP ENCODE
    INSTALL_TYPES Full
    )
