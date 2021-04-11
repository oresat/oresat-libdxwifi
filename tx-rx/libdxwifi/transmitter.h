/**
 *  transmitter.h
 *  
 *  DESCRIPTION: Transmitter reads blocks of data from an input source, attaches 
 *  required headers, and then injects the packet with Pcap
 * 
 *  https://github.com/oresat/oresat-dxwifi-software
 * 
 *  NOTES: struct fields prefixed with a '__' denote private scope
 * 
 */

#ifndef LIBDXWIFI_TRANSMITTER_H
#define LIBDXWIFI_TRANSMITTER_H

/************************
 *  Includes
 ***********************/

#include <pcap.h>

#include <libdxwifi/dxwifi.h>
#include <libdxwifi/details/assert.h>
#include <libdxwifi/details/ieee80211.h>

/************************
 *  Constants
 ***********************/

#define DXWIFI_TX_HEADER_SIZE \
    (sizeof(dxwifi_tx_radiotap_hdr) + sizeof(ieee80211_hdr))

#define DXWIFI_TX_FRAME_SIZE_MAX IEEE80211_MTU_MAX_LEN

#define DXWIFI_TX_PAYLOAD_SIZE_MAX \
    (DXWIFI_TX_FRAME_SIZE_MAX - DXWIFI_TX_HEADER_SIZE - IEEE80211_FCS_SIZE)

#define DXWIFI_TX_RADIOTAP_PRESENCE_BIT_FIELD \
    ( 0x1 << IEEE80211_RADIOTAP_FLAGS    \
    | 0x1 << IEEE80211_RADIOTAP_RATE     \
    | 0x1 << IEEE80211_RADIOTAP_TX_FLAGS)\

#define DXWIFI_TX_FRAME_HANDLER_MAX 8

#define DXWIFI_TX_RADIOTAP_HDR_SIZE 12

/************************
 *  Data structures
 ***********************/

/**
 *  The radiotap header is used to communicate to the driver information about 
 *  our packet. The header data itself is discarded before transmission.
 * 
 *  There are a lot of defined radiotap fields but most only make sense for 
 *  Rx packets. These are included becuase they help control injection 
 * 
 *  NOTE: Before modifying this struct be sure to read https://www.radiotap.org/ 
 *  for details. Fields are strictly ordered and aligned to natural boundries
 * 
 */
typedef struct  __attribute__((packed)) {
    ieee80211_radiotap_hdr  hdr;      /* packed radiotap header   */
    uint8_t                 flags;    /* frame flags              */
    uint8_t                 rate;     /* data rate (500Kbps)      */
    uint16_t                tx_flags; /* transmission flags       */
} dxwifi_tx_radiotap_hdr;

compiler_assert(sizeof(dxwifi_tx_radiotap_hdr) == DXWIFI_TX_RADIOTAP_HDR_SIZE, 
    "Mismatch in actual radiotap header size and calculated size");


/**
 *  The DxWifi frame structure looks like this:
 * 
 * 
 *    [ radiotap header + radiotap fields ] <--
 *    [         ieee80211 header          ]   |- All allocated on the same block
 *    [             payload               ]   |
 *    [       frame check sequence        ] <--
 *   
 *  
 *  Note, the FCS lives in the same memory block as the payload. The FCS should 
 *  always be the last four bytes of the frame and should always start at the 
 *  address `frame.payload[frame.payload_size]`.
 * 
 */
typedef struct __attribute__((packed)) { 
    // Actual Data Frame
    dxwifi_tx_radiotap_hdr  radiotap_hdr;  /* frame metadata               */
    ieee80211_hdr           mac_hdr;       /* link-layer header            */
    uint8_t                 payload[DXWIFI_TX_PAYLOAD_SIZE_MAX + IEEE80211_FCS_SIZE];       
                                            /* packet data and FCS          */

    // Control data about the payload, not transmitted with the data frame
    uint32_t                payload_size;   /* Size of the actual payload   */
} dxwifi_tx_frame;

compiler_assert(sizeof(dxwifi_tx_frame) == (DXWIFI_TX_FRAME_SIZE_MAX + sizeof(uint32_t)), 
    "Mismatch in actual tx frame size and calculated size");


typedef enum {
    DXWIFI_TX_NORMAL,
    DXWIFI_TX_TIMED_OUT,
    DXWIFI_TX_DEACTIVATED,
    DXWIFI_TX_ERROR
} dxwifi_tx_state_t;


/**
 *  The stats object is used to track information about each block of data that
 *  is transmitted as well as overall stats about the transmission
 */
typedef struct {
    uint32_t                data_frame_count;   /* number of data frames sent   */
    uint32_t                ctrl_frame_count;   /* number of ctrl frames sent   */
    uint32_t                total_bytes_read;   /* total bytes read from source */
    uint32_t                total_bytes_sent;   /* total of bytes sent via pcap */
    uint32_t                prev_bytes_read;    /* Size of last read            */
    uint32_t                prev_bytes_sent;    /* Size of last transmission    */
    dxwifi_tx_state_t       tx_state;           /* State of last transmission   */
    dxwifi_control_frame_t  frame_type;         /* Type of the last frame       */
} dxwifi_tx_stats;


/**
 *  Tx frame callbacks can modify how many payload bytes will be transmistted by 
 *  modifying the frame->payload_size field. Thus, if additional data is added 
 *  to the payload the NEW payload size must be attached to the frame->payload_size 
 *  field. Conversely, to truncate the  data you can attach a number less than 
 *  the payload size. If frame->payload_size is zero then the data frame WILL 
 *  NOT be transmitted. Lastly, modifying the radiotap/mac headers may result in 
 *  transmission failure and should only be modified at your own risk.
 * 
 *  Note: It is the user's responsiblity to handle scope and lifetime of user
 *  parameters
 */
typedef void (*dxwifi_tx_frame_cb)( 
        dxwifi_tx_frame* frame, /* Mutable reference to current data frame  */
        dxwifi_tx_stats stats,  /* Stats about the current transmission     */
        void* user              /* User supplied parameters                 */
        );


/**
 *  Frame handlers are called in two different scenarios: preinjection and 
 *  postinjection. Preinject handlers are called right before the data frame is 
 *  injected via pcap. The provided callback function is given a mutable 
 *  reference to the current data frame allowing the user to make last-minute 
 *  modifications before it is transmitted. Postinject will have updated stats 
 *  about the current state of transmission as well as give the user the ability
 *  to copy/read data on the frame before it is reused. Lastly, both preinject
 *  and postinject handlers can be used as event signals for the user space to
 *  perform arbitrary tasks like logging, sleep for transmission delay, etc. 
 */
typedef struct {
    dxwifi_tx_frame_cb  callback;
    void*               user_args;
} dxwifi_tx_frame_handler;


/**
 *  Transmitter is responsible for handling file transmission. The transmitter
 *  must be intialized before use and torn down after. It is the user's 
 *  responsibility to fill in the fields with the correct data they want for 
 *  their transmission.
 */
typedef struct {
    size_t      blocksize;          /* Size in bytes to read at a time      */
    int         transmit_timeout;   /* Number of seconds to wait for a read */
    int         redundant_ctrl_frames;
                                    /* Number of extra ctrl frames to send  */
    uint8_t     address[IEEE80211_MAC_ADDR_LEN];
                                    /* Transmitters MAC address             */
    uint8_t     rtap_flags;         /* Radiotap flags                       */
    uint8_t     rtap_rate_mbps;     /* Radiotap data rate                   */
    uint16_t    rtap_tx_flags;      /* Radiotap Tx flags                    */
    ieee80211_frame_control fctl;   /* Frame control settings               */


    dxwifi_tx_frame_handler __preinjection[DXWIFI_TX_FRAME_HANDLER_MAX];
                                    /* Called before injection              */
    dxwifi_tx_frame_handler __postinjection[DXWIFI_TX_FRAME_HANDLER_MAX];
                                    /* Called after injection               */
    volatile bool   __activated;    /* Currently transmitting?              */
    pcap_t*         __handle;       /* Session handle for Pcap              */

#if defined(DXWIFI_TESTS)
    const char*     savefile;       /* File to dump packet data to          */
    pcap_dumper_t*  dumper;         /* Handle to dump file                  */
#endif

} dxwifi_transmitter;


#define DXWIFI_TRANSMITTER_DFLT_INITIALIZER {\
    .blocksize              = 1024,\
    .transmit_timeout       = -1,\
    .redundant_ctrl_frames  = 0,\
    .rtap_flags             = IEEE80211_RADIOTAP_F_FCS,\
    .rtap_rate_mbps         = 1,\
    .rtap_tx_flags          = IEEE80211_RADIOTAP_F_TX_NOACK,\
    .fctl = {\
        .protocol_version   = IEEE80211_PROTOCOL_VERSION,\
        .type               = IEEE80211_FTYPE_DATA,\
        .stype              = { IEEE80211_STYPE_DATA },\
        .to_ds              = false,\
        .from_ds            = true,\
        .more_frag          = false,\
        .retry              = false,\
        .power_mgmt         = false,\
        .more_data          = true,\
        .wep                = false,\
        .order              = false\
    },\
    .address = {0xAA, 0xAA ,0xAA, 0xAA, 0xAA, 0xAA },\
}\


/************************
 *  Functions
 ***********************/


/**
 *  DESCRIPTION:    Initializes the transmitter object
 * 
 *  ARGUMENTS:
 * 
 *     transmitter: pointer to an allocated transmitter object
 * 
 *     device_name: Name of the WiFi device to send packets over. The 
 *                  specified device must be enabled in monitor mode.
 *  
 */
void init_transmitter(dxwifi_transmitter* transmitter, const char* device_name);


/**
 *  DESCRIPTION:    Tearsdown any resources associated with the transmitter
 * 
 *  ARGUMENTS: 
 * 
 *      transmitter: pointer to an allocated transmitter object
 * 
 */
void close_transmitter(dxwifi_transmitter* transmitter);


/**
 *  DESCRIPTION:    Attaches a frame handler to the first available slot in the 
 *                  preinjection pipeline
 * 
 *  ARGUMENTS: 
 * 
 *      tx:         pointer to an allocated transmitter object
 * 
 *      callback:   function pointer to callback function
 * 
 *      user:       pointer to user allocated callback parameters
 * 
 *  RETURNS:
 * 
 *      int:        index to the handler for reference or -1 if the 
 *                  pipeline is full
 * 
 *  NOTES: Attached handlers will be called in the order they were attached!
 *  If you have a dependency between handlers, ensure that you attach them 
 *  in the order you wish them to be called.
 * 
 */
int attach_preinject_handler(dxwifi_transmitter* tx, dxwifi_tx_frame_cb callback, void* user);


/**
 *  DESCRIPTION:    Removes the specified frame handler from the preinjection pipeline
 * 
 *  ARGUMENTS:
 *  
 *      tx:         pointer to an allocated transmitter object
 * 
 *      index:      index to the handler to be removed. Note, a negative value 
 *                  will instruct the transmitter to remove all preinject 
 *                  handlers
 * 
 *  RETURNS:       
 * 
 *      bool:       true if the handler was successfully removed
 * 
 */
bool remove_preinject_handler(dxwifi_transmitter* tx, int index);


/**
 *  DESCRIPTION:    Attaches a frame handler to the first available slot in the 
 *                  postinjection pipeline
 * 
 *  ARGUMENTS:
 * 
 *      tx:         pointer to an allocated transmitter object
 * 
 *      callback:   function pointer to callback function
 * 
 *      user:       pointer to user allocated callback parameters
 * 
 *  RETURNS:
 * 
 *      int:        index to the handler for reference or -1 if the 
 *                  pipeline is full
 * 
 *  NOTES: Attached handlers will be called in the order they were attached!
 *  If you have a dependency between handlers, ensure that you attach them 
 *  in the order you wish them to be called
 * 
 */
int attach_postinject_handler(dxwifi_transmitter* tx, dxwifi_tx_frame_cb callback, void* user);


/**
 *  DESCRIPTION:    Removes the specified frame handler from the postinjection pipeline
 * 
 *  ARGUMENTS:
 *  
 *      tx:         pointer to an allocated transmitter object
 * 
 *      index:      index to the handler to be removed. Note, a negative value 
 *                  will instruct the transmitter to remove all preinject 
 *                  handlers
 * 
 *  RETURNS:       
 * 
 *      bool:       true if the handler was successfully removed
 * 
 */
bool remove_postinject_handler(dxwifi_transmitter* tx, int index);


/**
 *  DESCRIPTION:    Reads blocks of data from @fd and transmits data until 
 *                  transmission is stopped via stop_transmission(), timeout 
 *                  occurs, or end of file is reached
 * 
 *  ARGUMENTS:
 * 
 *      transmitter:    Pointer to an initialized transmitter object
 * 
 *      fd:             File descriptor of the data to be sent. 
 * 
 *      out:            Pointer to an allocated stats object or NULL if stats
 *                      aren't needed.
 * 
 * 
 *  NOTES: fd can be any unix file descriptor. device, stdin, regular file, etc. 
 *  In the case of a stream like stdin, unless a timeout is specified in the 
 *  transmitter this function will never return and will block until a read is
 *  available. Install a signal handler to stop the transmission or run the 
 *  transmitter on a different thread if blocking is unacceptable.
 * 
 */
void start_transmission(dxwifi_transmitter* transmitter, int fd, dxwifi_tx_stats* out);


/**
 *  DESCRIPTION:        Transmit nbytes from data, 
 * 
 *  ARGUMENTS:
 * 
 *      transmitter:    pointer to an initialized transmitter object
 * 
 *      data:           Bytes to transmit
 * 
 *      nbytes:         Number of bytes to transmit
 * 
 *      out:            Pointer to an allocated stats object or NULL if stats
 *                      aren't needed. 
 * 
 */
void transmit_bytes(dxwifi_transmitter* transmitter, const void* data, size_t nbytes, dxwifi_tx_stats* out);


/**
 *  DESCRIPTION:    Signals to the transmitter to stop transmitting packets
 * 
 *  ARGUMENTS:
 *      transmitter:    pointer to an allocated transmitter object
 * 
 *  NOTES: There are no guarantees that no more packets will be transmitted. At 
 *  most one more packet may be transmitted. Also, this function is idempotent.
 * 
 */
void stop_transmission(dxwifi_transmitter* transmitter);

#endif // LIBDXWIFI_TRANSMITTER_H