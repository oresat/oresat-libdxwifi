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

#include <libdxwifi/dxwifi.h>
#include <libdxwifi/receiver.h>
#include <libdxwifi/details/heap.h>
#include <libdxwifi/details/assert.h>
#include <libdxwifi/details/logging.h>


#define DXWIFI_RX_PACKET_HEAP_CAPACITY ((DXWIFI_RX_PACKET_BUFFER_SIZE_MAX / DXWIFI_BLOCK_SIZE_MIN) + 1)


typedef struct {
    int32_t     frame_number;   /* Number of the frame was sent with        */
    uint8_t*    data;           /* pointer to data inside the packet buffer */
    ssize_t     size;           /* Size of the data frame                   */
    bool        crc_valid;      /* Was the attached crc correct?            */
} packet_heap_node;


typedef struct {
    binary_heap     packet_heap;    /* Tracks packet frame number       */
    uint8_t*        packet_buffer;  /* Buffer to copy captured packets  */
    size_t          pb_size;        /* Size of packet buffer            */
    size_t          index;          /* Index to next write position     */
    dxwifi_rx_stats rx_stats;       /* Capture statistics               */
    int             fd;             /* Sink to write out data           */
} frame_controller;


static bool order_by_frame_number_desc(const uint8_t* lhs, const uint8_t* rhs) {
    packet_heap_node* node1 = (packet_heap_node*) lhs;
    packet_heap_node* node2 = (packet_heap_node*) rhs;

    return node1->frame_number < node2->frame_number;
}


static uint32_t extract_frame_number(ieee80211_hdr* mac_hdr) {
    // Packed Frame number is in the last four bytes of address 1 field
    return ntohl(*(uint32_t*)(mac_hdr->addr1 + 2));
}


static void init_frame_controller(frame_controller* fc, size_t buffsize, int fd) {
    debug_assert(fc);

    fc->index       = 0;
    fc->fd          = fd;
    fc->pb_size     = buffsize;

    memset(&fc->rx_stats, 0x00, sizeof(dxwifi_rx_stats));
    fc->packet_buffer = calloc(fc->pb_size, sizeof(uint8_t));
    assert_M(fc->packet_buffer, "Failed to allocate Packet Buffer of size: %ld", fc->pb_size);

    init_heap(&fc->packet_heap, DXWIFI_RX_PACKET_HEAP_CAPACITY, sizeof(packet_heap_node), order_by_frame_number_desc);
}


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


static dxwifi_rx_frame* parse_rx_frame_fields(const struct pcap_pkthdr* pkt_stats, uint8_t* data) {
    dxwifi_rx_frame* frame = (dxwifi_rx_frame*) data;

    frame->__frame   = data;
    frame->rtap_hdr  = (ieee80211_radiotap_hdr*) data;
    frame->mac_hdr   = (ieee80211_hdr*)(data + frame->rtap_hdr->it_len);
    frame->payload   = data + frame->rtap_hdr->it_len + sizeof(ieee80211_hdr);
    frame->fcs       = data + pkt_stats->caplen - IEEE80211_FCS_SIZE;
    return frame;
}


static void log_frame_stats(dxwifi_rx_frame* frame, dxwifi_rx_stats* rx_stats) {

    char timestamp[256];
    struct tm *time;

    uint32_t frame_number = extract_frame_number(frame->mac_hdr);

    time = gmtime(&rx_stats->pkt_stats.ts.tv_sec);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", time);

    log_debug(
        "%d: (%s:%d) - (Capture Length, Packet Length) = (%d, %d)", 
        frame_number,
        timestamp, 
        rx_stats->pkt_stats.ts.tv_usec,
        rx_stats->pkt_stats.caplen, 
        rx_stats->pkt_stats.len
        );
    log_hexdump(frame->__frame, rx_stats->pkt_stats.caplen);
}


static void dump_packet_buffer(frame_controller* fc) {
    debug_assert(fc);

    int nbytes = 0;
    packet_heap_node node;
    int32_t expected_frame = ((packet_heap_node*)fc->packet_heap.tree)->frame_number;

    while(heap_pop(&fc->packet_heap, &node)) {

        // Data block is missing
        if(expected_frame != node.frame_number) { 

            // Add noise 

            int missing_blocks = (node.frame_number - expected_frame);

            fc->rx_stats.total_blocks_lost += missing_blocks;
        }

        nbytes = write(fc->fd, node.data, node.size);
        debug_assert_continue(nbytes == node.size, "Warning, Partial write occured");

        fc->rx_stats.total_writelen += nbytes;
        expected_frame = node.frame_number + 1;
    }
    fc->index = 0; // Reset the write position and reuse the buffer
}


static void process_frame(uint8_t* args, const struct pcap_pkthdr* pkt_stats, const uint8_t* frame) { 
    frame_controller* fc = (frame_controller*) args;

    // Buffer is full, write it out first
    if( fc->index + pkt_stats->caplen >= fc->pb_size ) {
        dump_packet_buffer(fc);
    }

    uint8_t* buffer_slot = fc->packet_buffer + fc->index;

    memcpy(buffer_slot, frame, pkt_stats->caplen);

    dxwifi_rx_frame* rx_frame = parse_rx_frame_fields(pkt_stats, buffer_slot);

    ssize_t payload_size = rx_frame->fcs - rx_frame->payload;

    // Add node to heap
    packet_heap_node node = {
        .frame_number   = extract_frame_number(rx_frame->mac_hdr),
        .data           = rx_frame->payload,
        .size           = payload_size,
        .crc_valid      = false // TODO verify CRC
    };

    heap_push(&fc->packet_heap, &node);

    // Update next write position and stats
    fc->index += payload_size; 
    fc->rx_stats.total_caplen += pkt_stats->caplen;
    fc->rx_stats.total_payload_size += payload_size;
    memcpy(&fc->rx_stats.pkt_stats, pkt_stats, sizeof(struct pcap_pkthdr));

    log_frame_stats(rx_frame, &fc->rx_stats);
}


static void log_rx_configuration(const dxwifi_receiver* rx, const char* dev_name) {
    int datalink = pcap_datalink(rx->__handle);
    log_info(
            "DxWifi Receiver Settings\n"
            "\tDevice:                   %s\n"
            "\tCapture Timeout:          %ds\n"
            "\tFilter:                   %s\n"
            "\tOptimize:                 %d\n"
            "\tSnapshot Length:          %d\n"
            "\tPacket Buffer Timeout:    %dms\n"
            "\tDispatch Count:           %d\n"
            "\tDatalink Type:            %s\n",
            dev_name,
            rx->capture_timeout,
            rx->filter,
            rx->optimize,
            rx->snaplen,
            rx->pb_timeout,
            rx->dispatch_count,
            pcap_datalink_val_to_description(datalink)
    );
}


void init_receiver(dxwifi_receiver* rx, const char* device_name) {
    debug_assert(rx && rx->filter);

    int status = 0;
    struct bpf_program filter;
    char err_buff[PCAP_ERRBUF_SIZE];

    rx->__activated = false;
    rx->__handle = pcap_open_live(
                        device_name,
                        rx->snaplen,
                        true, 
                        rx->pb_timeout,
                        err_buff
                    );
    assert_M(rx->__handle != NULL, err_buff);

    status = pcap_set_datalink(rx->__handle, DLT_IEEE802_11_RADIO);
    assert_M(status != PCAP_ERROR, "Failed to set datalink: %s", pcap_statustostr(status));

    status = pcap_setnonblock(rx->__handle, true, err_buff);
    assert_M(status != PCAP_ERROR, "Failed to set nonblocking mode: %s", err_buff);

    status = pcap_compile(rx->__handle, &filter, rx->filter, rx->optimize, PCAP_NETMASK_UNKNOWN);
    assert_M(status != PCAP_ERROR, "Failed to compile filter %s: %s", rx->filter, pcap_statustostr(status));

    status = pcap_setfilter(rx->__handle, &filter);
    assert_M(status != PCAP_ERROR, "Failed to set filter: %s", pcap_statustostr(status));

    log_rx_configuration(rx, device_name);
}


void close_receiver(dxwifi_receiver* receiver) {
    debug_assert(receiver && receiver->__handle);

    pcap_close(receiver->__handle);

    log_info("DxWiFi receiver clossed");
}


void receiver_activate_capture(dxwifi_receiver* rx, int fd, dxwifi_rx_stats* out) {
    debug_assert(rx && rx->__handle);

    int status = 0;
    frame_controller fc;
    struct pollfd request;

    init_frame_controller(&fc, rx->packet_buffer_size, fd);

    request.fd = pcap_get_selectable_fd(rx->__handle);
    assert_M(request.fd > 0, "Receiver handle cannot be polled");

    request.events = POLLIN; // Listen for read events only

    log_info("Starting packet capture...");
    rx->__activated = true;
    while(rx->__activated) {
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
            status = pcap_dispatch(rx->__handle, rx->dispatch_count, process_frame, NULL);

            debug_assert_continue(status != PCAP_ERROR, "Capture failure: %s", pcap_statustostr(status));

            fc.rx_stats.capture_state = DXWIFI_RX_NORMAL;
        }
    }
    log_info("DxWiFi Reciever capture ended");

    dump_packet_buffer(&fc); // Flush out whatever's leftover in the buffer

    if( pcap_stats(rx->__handle, &fc.rx_stats.pcap_stats) == PCAP_ERROR) {
        log_info("Failed to gather capture stats from PCAP");
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
