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
 *  Constants
 ***********************/

// https://www.tcpdump.org/manpages/pcap.3pcap.html
#define DXWIFI_SNAPLEN_MAX 65535

#define DXWIFI_DFLT_PACKET_BUFFER_TIMEOUT 20

#define DXWIFI_FRAME_CONTROL_DATA_SIZE 256
#define DXWIFI_FRAME_CONTROL_CHECK_THRESHOLD 0.75


/************************
 *  Types
 ***********************/

/**
 * Control frames are used to signal file boundaries to the receiver during 
 * multi-file transmission
 */
typedef enum {
    CONTROL_FRAME_NONE                  = 0x00,
    CONTROL_FRAME_PREAMBLE              = 0xff,
    CONTROL_FRAME_END_OF_TRANSMISSION   = 0xaa
} dxwifi_control_frame_t;


#endif // LIBDXWIFI_H