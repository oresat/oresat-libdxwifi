file(GLOB_RECURSE libdxwifi_sources *)

# TODO add option to make Static or Shared libary
add_library(dxwifi STATIC ${libdxwifi_sources})

set_target_properties(dxwifi 
    PROPERTIES 
    COMPILE_FLAGS "-Wall -Wextra -Wno-unused-function"
    ARCHIVE_OUTPUT_DIRECTORY ${DXWIFI_ARCHIVE_OUTPUT_DIRECTORY}
    )

target_link_libraries(dxwifi pcap openfec)
