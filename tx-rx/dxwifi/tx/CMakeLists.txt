file(GLOB tx_sources ./*)

add_executable(tx ${tx_sources})

target_link_libraries(tx dxwifi)
