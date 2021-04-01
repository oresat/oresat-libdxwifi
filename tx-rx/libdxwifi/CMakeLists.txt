file(GLOB_RECURSE libdxwifi_sources *)

add_library(dxwifi STATIC ${libdxwifi_sources})

set_target_properties(dxwifi PROPERTIES COMPILE_FLAGS "-Wall -Wextra -Wno-unused-function")

target_link_libraries(dxwifi pcap)
