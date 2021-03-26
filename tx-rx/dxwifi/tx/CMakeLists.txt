file(GLOB tx_sources ./*)

set(TX_DESCRIPTION "OreSat Live DxWiFi transmission program")

add_executable(tx ${tx_sources})

target_link_libraries(tx dxwifi)

install(
    TARGETS tx 
    DESTINATION /usr/bin
    COMPONENT TX
    )

cpack_add_component(
    TX
    DISPLAY_NAME TX 
    DESCRIPTION ${TX_DESCRIPTION}
    GROUP TX 
    INSTALL_TYPES Full
    )
