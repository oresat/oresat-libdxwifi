/**
 *  dxwifi.h
 * 
 *  DESCRIPTION: DxWiFi project includes, definitions, and types
 * 
 *  https://github.com/oresat/oresat-dxwifi-software
 * 
 */

#ifndef LIBDXWIFI_H
#define LIBDXWIFI_H

/************************
 *  Versioning
 ***********************/

#ifndef DXWIFI_VERSION
// Version is defined in root CMake file
#define DXWIFI_VERSION "0.1.0-dev"
#endif // LIBDXWIFI_VERSION


/************************
 *  Constants
 ***********************/

// https://www.tcpdump.org/manpages/pcap.3pcap.html
#define DXWIFI_SNAPLEN_MAX 65535

#define DXWIFI_DFLT_PACKET_BUFFER_TIMEOUT 20

#define DXWIFI_FRAME_CONTROL_SIZE 256

/************************
 *  Types
 ***********************/

/**
 * Control frames are used to signal file boundaries to the receiver during 
 * multi-file transmission
 */
typedef enum {
    DXWIFI_CONTROL_FRAME_NONE       = 0x00,
    DXWIFI_CONTROL_FRAME_UNKNOWN    = 0x01,
    DXWIFI_CONTROL_FRAME_PREAMBLE   = 0xff,
    DXWIFI_CONTROL_FRAME_EOT        = 0xaa
} dxwifi_control_frame_t;


#endif // LIBDXWIFI_H