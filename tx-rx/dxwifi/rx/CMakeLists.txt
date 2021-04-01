file(GLOB rx_sources ./*)

add_executable(rx ${rx_sources})

target_link_libraries(rx dxwifi)
