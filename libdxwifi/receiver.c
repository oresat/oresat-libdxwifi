/**
 *  receiver.c
 *  
 *  DESCRIPTION: see receiver.h for description
 * 
 *  https://github.com/oresat/oresat-dxwifi-software
 * 
 */

#include <string.h>

#include <time.h>
#include <poll.h>
#include <errno.h>
#include <unistd.h>

#include <arpa/inet.h>

#include <libdxwifi/dxwifi.h>
#include <libdxwifi/receiver.h>
#include <libdxwifi/transmitter.h>
#include <libdxwifi/details/heap.h>
#include <libdxwifi/details/assert.h>
#include <libdxwifi/details/logging.h>


#define DXWIFI_RX_PACKET_HEAP_CAPACITY ((DXWIFI_RX_PACKET_BUFFER_SIZE_MAX / DXWIFI_TX_BLOCKSIZE) + 1)


typedef struct {
    int32_t     frame_number;   /* Number of the frame was sent with          */
    uint8_t*    data;           /* pointer to data inside the packet buffer   */
    bool        crc_valid;      /* Was the attached crc correct?              */
} packet_heap_node;


/**
 *  Frame controller handles intra-capture state and contains flags that the 
 *  receiver uses to determine when to stop processing packets
 */
typedef struct {
    binary_heap             packet_heap;    /* Tracks packet frame number     */
    uint8_t*                packet_buffer;  /* Buffer to copy captured packets*/
    size_t                  pb_size;        /* Size of packet buffer          */
    size_t                  index;          /* Index to next write position   */
    bool                    eot_reached;    /* EOT signalled?                 */
    bool                    preamble_recv;  /* Received preamble?             */
    bool                    end_capture;    /* eot && preamble?               */
    const dxwifi_receiver*  rx;             /* Reference to owning receiver   */
    dxwifi_rx_stats         rx_stats;       /* Capture statistics             */
    int                     fd;             /* Sink to write out data         */
} frame_controller;


/**
 *  DESCRIPTION:    Ordering function for the packet heap
 * 
 *  ARGUMENTS:      
 * 
 *      lhs:        Left hand operand
 *      rhs:        Right hand operand
 * 
 *  RETURNS:
 *     
 *      bool:       true if the lhs frame number is less than the rhs
 *  
 */
static bool order_by_frame_number_desc(const uint8_t* lhs, const uint8_t* rhs) {
    packet_heap_node* node1 = (packet_heap_node*) lhs;
    packet_heap_node* node2 = (packet_heap_node*) rhs;

    return node1->frame_number < node2->frame_number;
}


/**
 *  DESCRIPTION:    Grabs the packed frame number from the correct field in the
 *                  MAC header
 * 
 *  ARGUMENTS:
 * 
 *      mac_hdr:    MAC layer header of the captured packet
 * 
 */
static uint32_t extract_frame_number(ieee80211_hdr* mac_hdr) {
    // Packed Frame number is in the last four bytes of address 1 field
    return ntohl(*(uint32_t*)(mac_hdr->addr1 + 2));
}

/**
 *  DESCRIPTION:    Initializes and allocates any frame controller resources
 * 
 *  ARGUMENTS:
 * 
 *      fc:         Pointer to Frame controller object
 * 
 *      rx:         Owning receiver object
 * 
 *      fd:         Sink to write out data to
 * 
 */
static void init_frame_controller(frame_controller* fc, const dxwifi_receiver* rx, int fd) {
    debug_assert(fc);

    fc->index           = 0;
    fc->rx              = rx;
    fc->fd              = fd;
    fc->end_capture     = 0;
    fc->eot_reached     = false;
    fc->preamble_recv   = false;
    fc->pb_size         = rx->packet_buffer_size;

    memset(&fc->rx_stats, 0x00, sizeof(dxwifi_rx_stats));
    fc->rx_stats.capture_state = DXWIFI_RX_NORMAL;
    
    fc->packet_buffer = calloc(fc->pb_size, sizeof(uint8_t));
    assert_M(fc->packet_buffer, "Failed to allocate Packet Buffer of size: %ld", fc->pb_size);

    init_heap(&fc->packet_heap, DXWIFI_RX_PACKET_HEAP_CAPACITY, sizeof(packet_heap_node), order_by_frame_number_desc);
}

/**
 *  DESCRIPTION:    Tearsdown any resources associated with the frame controller
 * 
 *  ARGUMENTS:
 * 
 *      fc:         Initialized frame controller
 * 
 */
static void teardown_frame_controller(frame_controller* fc) {
    debug_assert(fc);

    teardown_heap(&fc->packet_heap);
    free(fc->packet_buffer);
    fc->packet_buffer   = NULL;
    fc->pb_size         = 0;
    fc->index           = 0;
    fc->fd              = 0;
    memset(&fc->rx_stats, 0x00, sizeof(dxwifi_rx_stats));
}

/**
 *  DESCRIPTION:    Parses the raw captured data into the expected structure of 
 *                  the frame
 * 
 *  ARGUMENTS:
 * 
 *      pkt_stats:  Info about the current capture
 * 
 *      data:       Captured frame of data
 * 
 *  RETURNS:
 *      
 *      dxwifi_rx_frame: Structural representation of the data. All fields point
 *      into the provided data buffer and should not be freed or modified.
 *  
 */
static dxwifi_rx_frame parse_rx_frame_fields(const struct pcap_pkthdr* pkt_stats, uint8_t* data) {
    dxwifi_rx_frame frame;

    frame.__frame   = data;
    frame.rtap_hdr  = (ieee80211_radiotap_hdr*) data;
    frame.mac_hdr   = (ieee80211_hdr*)(data + frame.rtap_hdr->it_len);
    frame.payload   = data + frame.rtap_hdr->it_len + sizeof(ieee80211_hdr);
    frame.fcs       = data + pkt_stats->caplen - IEEE80211_FCS_SIZE;
    return frame;
}


/**
 *  DESCRIPTION:    Verify if the captured data is a control frame and determine
 *                  what kind of control frame it is
 * 
 *  ARGUMENTS:
 * 
 *      frame:      Captured frame of data
 * 
 *      frame_no:   Sequence data attached with the frame
 * 
 *      rx_stats:   State of the current capture session
 *  
 */
static void log_frame_stats(dxwifi_rx_frame* frame, int32_t frame_no, dxwifi_rx_stats* rx_stats) {
    debug_assert(frame && rx_stats);

    char timestamp[256];
    struct tm *time;

    time = gmtime(&rx_stats->pkt_stats.ts.tv_sec);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", time);

    log_debug(
        "%d - (%s) - (Capture Length=%d, Packet Length=%d)", 
        frame_no,
        timestamp, 
        rx_stats->pkt_stats.caplen, 
        rx_stats->pkt_stats.len
        );
    log_hexdump(frame->__frame, rx_stats->pkt_stats.caplen);
}


/**
 *  DESCRIPTION:    Verify if the captured data is a control frame and determine
 *                  what kind of control frame it is
 * 
 *  ARGUMENTS:
 * 
 *      frame:      Captured frame of data
 * 
 *      pkt_stats:  Information about the current capture
 * 
 *      check_threshold: Percentage of data that must match with a control data
 *                       value for us to consider this frame as a "control frame"
 * 
 *  RETURNS:
 *      dxwifi_control_frame_t: The type of the control frame
 *  
 */
static dxwifi_control_frame_t check_frame_control(const uint8_t* frame, float check_threshold) {
    // Get info we need from the raw data frame
    const ieee80211_radiotap_hdr* rtap = (const ieee80211_radiotap_hdr*)frame;
    const uint8_t* payload = frame + rtap->it_len + sizeof(ieee80211_hdr);

    unsigned eot                = 0;
    unsigned preamble           = 0;
    dxwifi_control_frame_t type = DXWIFI_CONTROL_FRAME_NONE;

    for(size_t i = 0; i < DXWIFI_TX_PAYLOAD_SIZE; ++i) {
        switch (payload[i])
        {
        case DXWIFI_CONTROL_FRAME_PREAMBLE:
            ++preamble;
            break;
        
        case DXWIFI_CONTROL_FRAME_EOT:
            ++eot;
            break;

        default:
            break;
        }
    } 
    if(((float)eot / DXWIFI_TX_PAYLOAD_SIZE) > check_threshold) {
        type = DXWIFI_CONTROL_FRAME_EOT;
    }
    else if (((float)preamble / DXWIFI_TX_PAYLOAD_SIZE) > check_threshold) {
        type = DXWIFI_CONTROL_FRAME_PREAMBLE;
    }
    return type;
}


/**
 *  DESCRIPTION:    Perform an action based on what type of control frame was 
 *                  received
 * 
 *  ARGUMENTS:
 * 
 *      fc:         Frame controller contains state information about the current
 *                  capture
 * 
 *      type:       The type of control frame recieved
 *  
 */
static void handle_frame_control(frame_controller* fc, dxwifi_control_frame_t type) {
    debug_assert(fc);

    switch (type) 
    {
    // TODO if the dispatch count is greater than 1 then the reciever will 
    // continue to process packets until number of packets processed is greater
    // than the dispatch count. If we encounter the EOT before we've processed 
    // all the packets we will end up processing the next files packets as 
    // part of this capture. This can ruin file boundaries, so we need to find 
    // a way to short circuit the loop or maybe just callback to the user that 
    // the EOT was found so that they can do some action like opening a new 
    // file for capture.
    case DXWIFI_CONTROL_FRAME_PREAMBLE:
        if(fc->rx_stats.num_packets_processed > 0) {
            fc->end_capture = true;
        }
        else if(!fc->preamble_recv){
            log_info("Uplink established!");
        }
        fc->preamble_recv = true;
        break;

    case DXWIFI_CONTROL_FRAME_EOT:
        if(!fc->eot_reached) {
            log_info("End-Of-Transmission signalled");
        }
        fc->eot_reached = true;
        break;
    
    default:
        debug_assert_always("Unkown control type");
        break;
    }
}


/**
 *  DESCRIPTION:    Write all the payload data recieved into a sink
 * 
 *  ARGUMENTS:
 * 
 *      fc:         Frame controller with allocated packet buffer
 *  
 */
static void dump_packet_buffer(frame_controller* fc) {
    debug_assert(fc);

    int nbytes = 0;
    packet_heap_node node;
    int32_t expected_frame = ((packet_heap_node*)fc->packet_heap.tree)->frame_number;

    while(heap_pop(&fc->packet_heap, &node)) {

        // Data block is missing
        if(fc->rx->ordered && (expected_frame != node.frame_number)) { 

            int missing_blocks = (node.frame_number - expected_frame);

            if(fc->rx->add_noise) {
                uint8_t noise[DXWIFI_TX_PAYLOAD_SIZE];

                memset(noise, fc->rx->noise_value, sizeof(noise));

                for(int i = 0; i < missing_blocks; ++i) {
                    fc->rx_stats.total_noise_added += write(fc->fd, noise, sizeof(noise));
                }
            }

            fc->rx_stats.total_blocks_lost += missing_blocks;
        }

        nbytes = write(fc->fd, node.data, DXWIFI_TX_PAYLOAD_SIZE);
        debug_assert_continue(nbytes == DXWIFI_TX_PAYLOAD_SIZE, "Partial write: %d - %s", nbytes, strerror(errno));

        fc->rx_stats.total_writelen += nbytes;
        expected_frame = node.frame_number + 1;
    }
    fc->index = 0; // Reset the write position and reuse the buffer
}


/**
 *  DESCRIPTION:    Checks the IEEE header address fields to verify that the 
 *                  packet orignated from OreSat
 * 
 *  ARGUMENTS:
 * 
 *      frame:      Captured data frame
 * 
 *      expected_address:  MAC address field that was stuffed by the transmitter
 * 
 *  RETURNS:
 *  
 *      bool:       True if any address is under the maximum hamming distance 
 *                  threshold
 * 
 */
static bool verify_sender(const uint8_t* frame, const uint8_t* expected_address, uint32_t threshold) {
    debug_assert(frame && expected_address);

    const ieee80211_radiotap_hdr* rtap = (const ieee80211_radiotap_hdr*)frame;
    const ieee80211_hdr* mac_hdr = (ieee80211_hdr*)(frame + rtap->it_len);

    uint32_t addr1_top      = *(uint32_t*)mac_hdr->addr1;
    uint16_t addr1_bottom   = *((uint16_t*)(mac_hdr->addr1 + sizeof(uint32_t)));

    uint32_t addr2_top      = *(uint32_t*)mac_hdr->addr2;
    uint16_t addr2_bottom   = *((uint16_t*)(mac_hdr->addr2 + sizeof(uint32_t)));

    uint32_t addr3_top      = *(uint32_t*)mac_hdr->addr3;
    uint16_t addr3_bottom   = *((uint16_t*)(mac_hdr->addr3 + sizeof(uint32_t)));

    uint32_t expected_top    = *(uint32_t*)expected_address;
    uint16_t expected_bottom = *((uint16_t*)(expected_address + sizeof(uint32_t)));

    uint32_t addr1_dist = hamming_dist32(addr1_top, expected_top) 
                        + hamming_dist32(addr1_bottom, expected_bottom);

    uint32_t addr2_dist = hamming_dist32(addr2_top, expected_top) 
                        + hamming_dist32(addr2_bottom, expected_bottom);

    uint32_t addr3_dist = hamming_dist32(addr3_top, expected_top) 
                        + hamming_dist32(addr3_bottom, expected_bottom);

    return addr1_dist < threshold || addr2_dist < threshold || addr3_dist < threshold;
}


/**
 *  DESCRIPTION:    Callback for PCAP dispatch. Called each time a frame is
 *                  matching the BPF expression is captured
 * 
 *  ARGUMENTS:
 * 
 *      args:       Frame controller allocated in receiver_activate_capture()
 * 
 *      pkt_stats:  Information about the current capture
 * 
 *      frame:      Actual data that was captured. Memory is owned by pcap and 
 *                  is copied onto our own packet buffer.
 *  
 */
static void process_frame(uint8_t* args, const struct pcap_pkthdr* pkt_stats, const uint8_t* frame) { 
    frame_controller* fc = (frame_controller*) args;

    if(verify_sender(frame, fc->rx->sender_addr, fc->rx->max_hamming_dist)) {

        dxwifi_control_frame_t ctrl_frame = check_frame_control(frame, 0.66);

        if(ctrl_frame != DXWIFI_CONTROL_FRAME_NONE) {
            handle_frame_control(fc, ctrl_frame);
        }
        else {
            // Buffer is full, write it out first
            if( fc->index + pkt_stats->caplen >= fc->pb_size ) {
                dump_packet_buffer(fc);
            }

            // Next available slot in the packet buffer
            uint8_t* buffer_slot = fc->packet_buffer + fc->index;

            // Copy the entire frame into the packet buffer
            memcpy(buffer_slot, frame, pkt_stats->caplen);

            dxwifi_rx_frame rx_frame = parse_rx_frame_fields(pkt_stats, buffer_slot);

            // TODO parse radiotap header data and store provided info

            ssize_t payload_size = rx_frame.fcs - rx_frame.payload;

            int32_t frame_number = (fc->rx->ordered 
                ? extract_frame_number(rx_frame.mac_hdr) 
                : fc->rx_stats.num_packets_processed);

            // Heap node only points to the payload data
            packet_heap_node node = {
                .frame_number   = frame_number,
                .data           = rx_frame.payload,
                .crc_valid      = false // TODO verify CRC
            };
            heap_push(&fc->packet_heap, &node);

            // Update next write position and stats
            fc->index                           += pkt_stats->caplen; 
            fc->rx_stats.total_caplen           += pkt_stats->caplen;
            fc->rx_stats.total_payload_size     += payload_size;
            fc->rx_stats.num_packets_processed  += 1;
            memcpy(&fc->rx_stats.pkt_stats, pkt_stats, sizeof(struct pcap_pkthdr));

            log_frame_stats(&rx_frame, frame_number, &fc->rx_stats);
        }
    }
    else {
        ++fc->rx_stats.packets_dropped;
    }
}

//
// See receiver.h for description of non-static functions
//

static void log_rx_configuration(const dxwifi_receiver* rx, const char* dev_name) {
    int datalink = pcap_datalink(rx->__handle);
    log_info(
            "DxWifi Receiver Settings\n"
            "\tDevice:                   %s\n"
            "\tCapture Timeout:          %ds\n"
            "\tPacket Buffer Size:       %ld\n"
            "\tOrdered:                  %d\n"
            "\tAdd-noise:                %d\n"
            "\tFilter:                   %s\n"
            "\tOptimize:                 %d\n"
            "\tSnapshot Length:          %d\n"
            "\tPCAP Buffer Timeout:      %dms\n"
            "\tDispatch Count:           %d\n"
            "\tDatalink Type:            %s\n",
            dev_name,
            rx->capture_timeout,
            rx->packet_buffer_size,
            rx->ordered,
            rx->add_noise,
            rx->filter,
            rx->optimize,
            rx->snaplen,
            rx->pb_timeout,
            rx->dispatch_count,
            pcap_datalink_val_to_description(datalink)
    );
}


void init_receiver(dxwifi_receiver* rx, const char* device_name) {
    debug_assert(rx);

    int status = 0;
    char err_buff[PCAP_ERRBUF_SIZE];

    rx->__activated = false;
#if defined(DXWIFI_TESTS)
    if(rx->savefile) {
        rx->__handle = pcap_open_offline(rx->savefile, err_buff);
    }
    else {
        rx->__handle = pcap_fopen_offline(stdin, err_buff);
    }
    assert_M(rx->__handle != NULL, err_buff);
#else
    rx->__handle = pcap_open_live(
                        device_name,
                        rx->snaplen,
                        true, 
                        rx->pb_timeout,
                        err_buff
                    );
    assert_M(rx->__handle != NULL, err_buff);

    status = pcap_setnonblock(rx->__handle, true, err_buff);
    assert_M(status != PCAP_ERROR, "Failed to set nonblocking mode: %s", err_buff);
#endif // DXWIFI_TESTS

    status = pcap_set_datalink(rx->__handle, DLT_IEEE802_11_RADIO);
    assert_M(status != PCAP_ERROR, "Failed to set datalink: %s", pcap_statustostr(status));

    if(rx->filter != NULL) {
        struct bpf_program filter;
        status = pcap_compile(rx->__handle, &filter, rx->filter, rx->optimize, PCAP_NETMASK_UNKNOWN);
        assert_M(status != PCAP_ERROR, "Failed to compile filter %s: %s", rx->filter, pcap_statustostr(status));

        status = pcap_setfilter(rx->__handle, &filter);
        assert_M(status != PCAP_ERROR, "Failed to set filter: %s", pcap_statustostr(status));

        pcap_freecode(&filter);
    }

    log_rx_configuration(rx, device_name);
}


void close_receiver(dxwifi_receiver* receiver) {
    debug_assert(receiver && receiver->__handle);

    pcap_close(receiver->__handle);

    log_info("DxWiFi receiver closed");
}


void receiver_activate_capture(dxwifi_receiver* rx, int fd, dxwifi_rx_stats* out) {
    debug_assert(rx && rx->__handle);

    int status = 0;
    frame_controller fc;

    struct pollfd request = {
        .fd         = pcap_get_selectable_fd(rx->__handle),
        .events     = POLLIN,
        .revents    = 0
    };
    assert_M(request.fd >= 0, "Receiver handle cannot be polled");

    init_frame_controller(&fc, rx, fd);

    log_info("Starting packet capture...");
    rx->__activated = true;

    while(rx->__activated && !fc.end_capture) {

        status = poll(&request, 1, rx->capture_timeout * 1000);

        if(status == 0) {
            log_info("Reciever timeout occured");
            fc.rx_stats.capture_state = DXWIFI_RX_TIMED_OUT;
            rx->__activated = false;
        }
        else if(status < 0) {
            if(rx->__activated) { 
                log_error("Error occured: %s", strerror(errno));
                fc.rx_stats.capture_state = DXWIFI_RX_ERROR;
            }
            else {
                fc.rx_stats.capture_state = DXWIFI_RX_DEACTIVATED;
            }
        }
        else {
            status = pcap_dispatch(rx->__handle, rx->dispatch_count, process_frame, (uint8_t*)&fc);

#if defined(DXWIFI_TESTS)
            // When reading from a savefile, 0 denotes that there are no more packets
            if(status == 0) {
                rx->__activated = false;
                fc.rx_stats.capture_state = DXWIFI_RX_DEACTIVATED;
            }
#endif // DXWIFI_TESTS

            assert_continue(status != PCAP_ERROR, "Capture failure: %s", pcap_statustostr(status));
        }
    }
    log_info("DxWiFi Reciever capture ended");

    dump_packet_buffer(&fc); // Flush out whatever's leftover in the buffer

    if( pcap_stats(rx->__handle, &fc.rx_stats.pcap_stats) == PCAP_ERROR) {
        log_warning("Failed to gather capture stats from PCAP");
    }

    if(out) {
        *out = fc.rx_stats;
    }

    teardown_frame_controller(&fc);
}

void receiver_stop_capture(dxwifi_receiver* rx) {
    if(rx) {
        pcap_breakloop(rx->__handle);
        rx->__activated = false;
    }
}
