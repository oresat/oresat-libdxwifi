file(GLOB rx_sources ./*)

set(RX_DESCRIPTION "OreSat Live DxWiFi receiver program")

add_executable(rx ${rx_sources})

target_link_libraries(rx dxwifi)

install(
    TARGETS rx 
    DESTINATION /usr/bin
    COMPONENT RX
    )

cpack_add_component(
    RX
    DISPLAY_NAME RX 
    DESCRIPTION ${RX_DESCRIPTION}
    GROUP RX 
    INSTALL_TYPES Full
    )
