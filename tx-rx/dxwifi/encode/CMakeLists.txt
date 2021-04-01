file(GLOB encode_sources ./*)

add_executable(encode ${encode_sources})

set_target_properties(encode
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${DXWIFI_RUNTIME_OUTPUT_DIRECTORY}
    )

target_link_libraries(encode dxwifi)
