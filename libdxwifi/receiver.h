/**
 *  receiver.h
 *  
 *  DESCRIPTION: Receiver handles capturing data frames and unpacking the payload
 *  data
 * 
 *  https://github.com/oresat/oresat-dxwifi-software
 * 
 *  NOTES: struct fields prefixed with a '__' denote private scope
 * 
 */

#ifndef LIBDXWIFI_RECEIVER_H
#define LIBDXWIFI_RECEIVER_H

/************************
 *  Includes
 ***********************/

#include <pcap.h>

#include <libdxwifi/fec.h>
#include <libdxwifi/details/radiotap.h>
#include <libdxwifi/details/ieee80211.h>


/************************
 *  Constants
 ***********************/

#define DXWIFI_RX_PACKET_BUFFER_SIZE_MIN IEEE80211_MTU_MAX_LEN
#define DXWIFI_RX_PACKET_BUFFER_SIZE_MAX (1024 * 1024 * 5)  // 5mb


/************************
 *  Data structures
 ***********************/

typedef enum {
    DXWIFI_RX_NORMAL,
    DXWIFI_RX_TIMED_OUT,
    DXWIFI_RX_DEACTIVATED,
    DXWIFI_RX_ERROR
} dxwifi_rx_state_t;


/**
 * The received radiotap header is populated by the MAC driver and attached to
 * each captured packet. Different drivers will add different radiotap data, and
 * the following fields may not be present in every packet. The following 
 * radiotap fields are what has been tested on the AR9271 ath9k_htc drivers as 
 * being present. 
 * 
 * See https://www.radiotap.org/ for more information
 */
typedef struct {

    uint32_t tsft[2];       /* Time Sync Function Timer in microseconds */
    struct {
        uint16_t frequency;
        uint16_t flags;
    } channel;              /* Tx/Rx Frequency and modulation flags     */

    uint16_t rx_flags;      /* Received frame extended flags            */
    struct {
        uint8_t known;
        uint8_t flags;
        uint8_t mcs;
    } mcs;                  /* MCS rate index as in IEEE802.11n-2009    */

    uint8_t flags;          /* Received frame properties                */

    uint8_t antenna;        /* Antenna used during capture              */

    int8_t  ant_signal;     /* Antenna signal in dBm                    */
} dxwifi_rx_radiotap_hdr;

/**
 *  The DxWifi RX frame structure comes in like this:
 * 
 * 
 *    [ radiotap header + radiotap fields ] <--
 *    [         ieee80211 header          ]   |- All allocated on the same block
 *    [             payload               ]   |
 *    [       frame check sequence        ] <--
 *   
 *  When Pcap captures a packet the entire packet is just globbed together on an
 *  allocated block that Pcap owns. We copy the data onto the __frame array and 
 *  point the fields to the correct spots in that array.
 * 
 */
typedef struct {
    const ieee80211_radiotap_hdr  *rtap_hdr;  /* packed radiotap header       */
    const ieee80211_hdr           *mac_hdr;   /* link-layer header            */
    const uint8_t                 *payload;   /* packet data                  */
    const uint8_t                 *fcs;       /* frame check sequence         */

    const uint8_t                 *__frame;   /* storage for the data         */
} dxwifi_rx_frame;


/**
 *  The stats object is used to track information about each data frame captured
 *  as well as overall capture statistics.
 */
typedef struct {
    uint32_t                total_payload_size;     /* Accumulated size of each payload */
    uint32_t                total_writelen;         /* Total number of bytes written out*/
    uint32_t                total_caplen;           /* Total number of bytes captured   */
    uint32_t                total_blocks_lost;      /* Number of data blocks lost       */
    uint32_t                total_noise_added;      /* Number of bytes of noise added   */
    uint32_t                num_packets_processed;  /* Number of packets processed      */
    uint32_t                packets_dropped;        /* Packets dropped because by rx    */
    uint32_t                bad_crcs;               /* Number of packets with a bad CRC */
    dxwifi_rx_state_t       capture_state;          /* State of last capture            */
    struct pcap_pkthdr      pkt_stats;              /* Stats for the current capture    */
    struct pcap_stat        pcap_stats;             /* Pcap statistics                  */
    dxwifi_rx_radiotap_hdr  rtap;                   /* Radiotap metadata                */
} dxwifi_rx_stats;


/**
 *  Receiver is responsible for handling packet capture. The receiver must be
 *  initialized before use and torn down after. It is the user's responsibility 
 *  to fill in the fields with the correct capture settings they want. 
 * 
 *  NOTES: add_noise is only used if the ordered flag is set. When receiving an
 *  "ordered" transmission it's important that the frame number is stuffed into 
 *  the last four bytes of the MAC header's addr1 field. If the frame number 
 *  is not present then the receiver will not be able to sort the packet data.
 * 
 */
typedef struct {
    unsigned    dispatch_count;     /* Number of packets to process at a time */
    unsigned    capture_timeout;    /* Number of seconds to wait for a packet */
    size_t      packet_buffer_size; /* Size of the intermediate packet buffer */
    bool        ordered;            /* Packets have packed sequence data      */
    bool        add_noise;          /* Add noise for missing packets          */
    uint8_t     noise_value;        /* Value to use for noise                 */
    uint8_t     sender_addr[IEEE80211_MAC_ADDR_LEN];
                                    /* Transmitters MAC address               */
    uint32_t    max_hamming_dist;   /* Max number of bit errors in address    */

    // https://www.tcpdump.org/manpages/pcap.3pcap.html
    const char *filter;             /* BPF Program string                     */
    bool        optimize;           /* Optimize compiled filter?              */
    int         snaplen;            /* Snapshot length in bytes               */
    int         pb_timeout;         /* PCAP Packet buffer timeout             */

    volatile bool   __activated;    /* Currently capturing packets?           */
    pcap_t*         __handle;       /* Pcap session handle                    */

#if defined(DXWIFI_TESTS)
    const char*     savefile;       /* Name of file to read packets from      */
#endif

} dxwifi_receiver;


#define DXWIFI_RECEIVER_DFLT_INITIALIZER {\
    .dispatch_count     = 1,\
    .capture_timeout    = -1,\
    .packet_buffer_size = DXWIFI_RX_PACKET_BUFFER_SIZE_MAX,\
    .ordered            = false,\
    .add_noise          = false,\
    .noise_value        = 0xff,\
    .sender_addr        = DXWIFI_DFLT_SENDER_ADDR,\
    .max_hamming_dist   = 5,\
    .filter             = NULL,\
    .optimize           = true,\
    .snaplen            = DXWIFI_SNAPLEN_MAX,\
    .pb_timeout         = DXWIFI_DFLT_PACKET_BUFFER_TIMEOUT\
}\


/************************
 *  Functions
 ***********************/


/**
 *  DESCRIPTION:    Initializes the receiver object
 * 
 *  ARGUMENTS:
 * 
 *     receiver:    pointer to an allocated receiver object
 * 
 *     device_name: Name of the WiFi device to capture packets on. The 
 *                  specified device must be enabled in monitor mode.
 *  
 */
void init_receiver(dxwifi_receiver* receiver, const char* device_name);


/**
 *  DESCRIPTION:    Tearsdown any resources associated with the receiver
 * 
 *  ARGUMENTS: 
 * 
 *      receiver:   pointer to an allocated receiver object
 * 
 */
void close_receiver(dxwifi_receiver* receiver);


/**
 *  DESCRIPTION:    Captures any packets matching the specified filter and 
 *                  writes out the payload data to @fd. Will continue capturing
 *                  packets until the capture is stopped via receiver_stop_capture,
 *                  timeout occurs, or End-Of-Transmission is signalled from the
 *                  transmitter.
 * 
 *  ARGUMENTS:
 * 
 *      receiver:   pointer to an allocated receiver object
 * 
 *      fd:         File descriptor to write out payload data to
 * 
 *      out:        pointer to an allocated stats object or NULL if stats aren't
 *                  needed
 * 
 * 
 *  NOTES: Packet data is buffered before it is written out. The receiver also
 *  supports options for ordering the packets and filling in missing data with
 *  noise before it is written out. 
 */
void receiver_activate_capture(dxwifi_receiver* receiver, int fd, dxwifi_rx_stats* out);


/**
 *  DESCRIPTION:    Signals to the receiver to stop capturing packets
 * 
 *  ARGUMENTS:
 * 
 *      receiver:   pointer to an allocated receiver object
 * 
 *  NOTES: There are no guartantees that no more packets will be processed. At
 *  most at least one more packet may be processed.
 * 
 */
void receiver_stop_capture(dxwifi_receiver* receiver);

#endif // LIBDXWIFI_RECEIVER_H
