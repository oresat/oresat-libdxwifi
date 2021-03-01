/**
 *  transmitter.c
 *  
 *  DESCRIPTION: see transmitter.h for description
 * 
 *  https://github.com/oresat/oresat-dxwifi-software
 * 
 */


#include <string.h>
#include <stdbool.h>

#include <poll.h>
#include <errno.h>
#include <unistd.h>
#include <endian.h>

#include <arpa/inet.h>

#include <libdxwifi/dxwifi.h>
#include <libdxwifi/transmitter.h>
#include <libdxwifi/details/utils.h>
#include <libdxwifi/details/assert.h>
#include <libdxwifi/details/logging.h>


static void setup_dxwifi_tx_frame(dxwifi_tx_frame* frame) {
    debug_assert(frame);

    memset(frame->__frame, 0x00, DXWIFI_TX_FRAME_SIZE_MAX);

    frame->radiotap_hdr = (dxwifi_tx_radiotap_hdr*) frame->__frame;
    frame->mac_hdr      = (ieee80211_hdr*) (frame->__frame + sizeof(dxwifi_tx_radiotap_hdr));
    frame->payload      = frame->__frame + DXWIFI_TX_HEADER_SIZE;
}


static void construct_radiotap_header(dxwifi_tx_radiotap_hdr* radiotap_hdr, uint8_t flags, uint8_t rate_mbps, uint16_t tx_flags) {
    debug_assert(radiotap_hdr);

    radiotap_hdr->hdr.it_version    = IEEE80211_RADIOTAP_MAJOR_VERSION;
    radiotap_hdr->hdr.it_len        = htole16(sizeof(dxwifi_tx_radiotap_hdr));
    radiotap_hdr->hdr.it_present    = htole32(DXWIFI_TX_RADIOTAP_PRESENCE_BIT_FIELD);

    radiotap_hdr->flags     = flags;
    radiotap_hdr->rate      = rate_mbps * 2;  // Radiotap units are 500Kbps. Multiply by 2 to convert to Mbps
    radiotap_hdr->tx_flags  = tx_flags;
}


static void construct_ieee80211_header( ieee80211_hdr* mac, ieee80211_frame_control fcntl, uint16_t duration_id, uint8_t* sender_address) {
    debug_assert(mac && sender_address);

    uint16_t frame_control = 0x00;

    set_bits16(&frame_control, IEEE80211_FCTL_VERS,     fcntl.protocol_version);
    set_bits16(&frame_control, IEEE80211_FCTL_FTYPE,    fcntl.type);
    set_bits16(&frame_control, IEEE80211_FCTL_STYPE,    fcntl.stype.data);
    set_bits16(&frame_control, IEEE80211_FCTL_TODS,     (fcntl.to_ds      ? IEEE80211_FCTL_TODS       : 0));
    set_bits16(&frame_control, IEEE80211_FCTL_FROMDS,   (fcntl.from_ds    ? IEEE80211_FCTL_FROMDS     : 0));
    set_bits16(&frame_control, IEEE80211_FCTL_RETRY,    (fcntl.retry      ? IEEE80211_FCTL_RETRY      : 0));
    set_bits16(&frame_control, IEEE80211_FCTL_PM,       (fcntl.power_mgmt ? IEEE80211_FCTL_PM         : 0));
    set_bits16(&frame_control, IEEE80211_FCTL_MOREDATA, (fcntl.more_data  ? IEEE80211_FCTL_MOREDATA   : 0));
    set_bits16(&frame_control, IEEE80211_FCTL_PROTECTED,(fcntl.wep        ? IEEE80211_FCTL_PROTECTED  : 0));
    set_bits16(&frame_control, IEEE80211_FCTL_ORDER,    (fcntl.order      ? IEEE80211_FCTL_ORDER      : 0));

    mac->frame_control = frame_control;

    mac->duration_id = htons(duration_id);

    memset(mac->addr1, 0xff, IEEE80211_MAC_ADDR_LEN);
    memset(mac->addr3, 0xff, IEEE80211_MAC_ADDR_LEN);

    // Note to future developers, for some reason if the first two bytes of 
    // addr1 are 0x00 then the ath9k_htc driver will attempt to retransmit the
    // packet multiple times. The debug assert statement is here to help you not
    // go down the hours-long rabbit hole of figuring out why the transmitter is
    // broken after the seemingly innocuous change of modifying the address field
    debug_assert(mac->addr1[0] && mac->addr1[1]);

    memcpy(mac->addr2, sender_address, IEEE80211_MAC_ADDR_LEN);

    mac->seq_ctrl = 0;
}


static int attach_handler(dxwifi_tx_frame_handler* pipeline, dxwifi_tx_frame_cb callback, void* user) {
    debug_assert(pipeline && callback);

    dxwifi_tx_frame_handler handler = {
        .callback   = callback,
        .user_args  = user
    };

    for(int i = 0; i < DXWIFI_TX_FRAME_HANDLER_MAX; ++i) {
        if(pipeline[i].callback == NULL) {
            pipeline[i] = handler;
            return i;
        }
    }
    return -1;
}


static bool remove_handler(dxwifi_tx_frame_handler* pipeline, int index) {
    debug_assert(pipeline);

    bool success = false;
    if(index < 0) {
        memset(pipeline, 0x00, sizeof(dxwifi_tx_frame_handler) * DXWIFI_TX_FRAME_HANDLER_MAX);
        success = true;
    }
    else if (index < DXWIFI_TX_FRAME_HANDLER_MAX) {
        dxwifi_tx_frame_handler* handler = &pipeline[index];

        success = handler->callback != NULL;

        handler->callback   = NULL;
        handler->user_args  = NULL;
    }
    return success;
}


static size_t invoke_handlers(dxwifi_tx_frame_handler* pipeline, dxwifi_tx_frame* frame, dxwifi_tx_stats tx_stats) {
    debug_assert(pipeline && frame);

    size_t sz = tx_stats.prev_bytes_read;
    for(int i = 0; i < DXWIFI_TX_FRAME_HANDLER_MAX; ++i) {
        if(pipeline[i].callback != NULL) {
            sz = pipeline[i].callback(
                frame, 
                sz, 
                tx_stats, 
                pipeline[i].user_args
                );

            assert_continue(
                0 < sz && sz < DXWIFI_TX_PAYLOAD_SIZE_MAX, 
                "Payload size: %d, exceeds defined bounds", 
                sz
                );
        }
    }
    return sz;
}


static void send_control_frame(dxwifi_transmitter* tx, dxwifi_tx_frame* frame, dxwifi_control_frame_t type) {
    debug_assert(tx && tx->__handle && frame && frame->__frame);

    uint8_t control_data[DXWIFI_FRAME_CONTROL_DATA_SIZE];

    memset(control_data, type, DXWIFI_FRAME_CONTROL_DATA_SIZE);

    for (int i = 0; i < tx->redundant_ctrl_frames + 1; ++i) {
        int status = pcap_inject(tx->__handle, frame->__frame, DXWIFI_TX_HEADER_SIZE + sizeof(control_data) + IEEE80211_FCS_SIZE);
        log_info("%s Frame Sent: %d", control_frame_type_to_str(type), status);
        log_hexdump(frame->__frame, DXWIFI_TX_HEADER_SIZE + sizeof(control_data) + IEEE80211_FCS_SIZE);
    }
}


static void log_tx_configuration(const dxwifi_transmitter* tx, const char* device_name) {
    log_info(
            "DxWifi Transmitter Settings\n"
            "\tDevice:              %s\n"
            "\tBlock Size:          %ld\n"
            "\tTransmit Timeout:    %d\n"
            "\tRedundant Ctrl:      %d\n"
            "\tData Rate:           %dMbps\n"
            "\tRTAP flags:          0x%x\n"
            "\tRTAP Tx flags:       0x%x\n",
            device_name,
            tx->blocksize,
            tx->transmit_timeout,
            tx->redundant_ctrl_frames,
            tx->rtap_rate_mbps,
            tx->rtap_flags,
            tx->rtap_tx_flags
    );
}


void init_transmitter(dxwifi_transmitter* tx, const char* device_name) {
    debug_assert(tx);

    char err_buff[PCAP_ERRBUF_SIZE];

    tx->__activated = false;

    memset(tx->__preinjection,  0x00, sizeof(dxwifi_tx_frame_handler) * DXWIFI_TX_FRAME_HANDLER_MAX);
    memset(tx->__postinjection, 0x00, sizeof(dxwifi_tx_frame_handler) * DXWIFI_TX_FRAME_HANDLER_MAX);

    tx->__handle = pcap_open_live(
                        device_name,
                        DXWIFI_SNAPLEN_MAX, 
                        true,
                        DXWIFI_DFLT_PACKET_BUFFER_TIMEOUT, 
                        err_buff
                    );

    // Hard assert here because if pcap fails it's all FUBAR anyways
    assert_M(tx->__handle != NULL, err_buff);

    log_tx_configuration(tx, device_name);
}


void close_transmitter(dxwifi_transmitter* transmitter) {
    debug_assert(transmitter && transmitter->__handle);

    pcap_close(transmitter->__handle);

    log_info("DxWifi transmitter closed");
}


int attach_preinject_handler(dxwifi_transmitter* tx, dxwifi_tx_frame_cb callback, void* user) {
    debug_assert(tx && callback);

    return attach_handler(tx->__preinjection, callback, user);
}


bool remove_preinject_handler(dxwifi_transmitter* tx, int index) {
    debug_assert(tx);

    return remove_handler(tx->__preinjection, index);
}


int attach_postinject_handler(dxwifi_transmitter* tx, dxwifi_tx_frame_cb callback, void* user) {
    debug_assert(tx && callback);

    return attach_handler(tx->__postinjection, callback, user);
}


bool remove_postinject_handler(dxwifi_transmitter* tx, int index) {
    debug_assert(tx);

    return remove_handler(tx->__postinjection, index);
}


void start_transmission(dxwifi_transmitter* tx, int fd, dxwifi_tx_stats* out) {
    debug_assert(tx && tx->__handle);

    int status = 0;

    struct pollfd   request = {
        .fd         = fd,
        .events     = POLLIN, // Listen for read events only
        .revents    = 0
    };

    dxwifi_tx_stats stats = {
        .frame_count        = 0,
        .total_bytes_read   = 0,
        .total_bytes_sent   = 0,
        .prev_bytes_read    = 0,
        .prev_bytes_sent    = 0,
        .tx_state           = DXWIFI_TX_NORMAL
    };

    dxwifi_tx_frame data_frame;

    setup_dxwifi_tx_frame(&data_frame);

    construct_radiotap_header(data_frame.radiotap_hdr, tx->rtap_flags, tx->rtap_rate_mbps, tx->rtap_tx_flags);

    construct_ieee80211_header(data_frame.mac_hdr, tx->fctl, 0xffff, tx->address);

    log_info("Starting DxWiFi Transmission...");

    tx->__activated = true;

    send_control_frame(tx, &data_frame, DXWIFI_CONTROL_FRAME_PREAMBLE);

    do {
        status = poll(&request, 1, tx->transmit_timeout * 1000);

        if(status == 0) {
            log_info("Transmitter timeout occured");
            stats.tx_state = DXWIFI_TX_TIMED_OUT;
            tx->__activated = false;
        } 
        else if (status < 0) {
            if(tx->__activated) {
                log_error("Error occured: %s", strerror(errno));
                stats.tx_state = DXWIFI_TX_ERROR;
            }
            else {
                stats.tx_state = DXWIFI_TX_DEACTIVATED;
            }
        }
        else {
            stats.prev_bytes_read = read(fd, data_frame.payload, tx->blocksize);
            if(stats.prev_bytes_read > 0) {

                size_t payload_size = invoke_handlers(tx->__preinjection, &data_frame, stats);

                status = pcap_inject(tx->__handle, data_frame.__frame, DXWIFI_TX_HEADER_SIZE + payload_size + IEEE80211_FCS_SIZE);

                assert_continue(status > 0, "Injection failure: %s", pcap_statustostr(status));

                stats.prev_bytes_sent   = status;
                stats.total_bytes_read += stats.prev_bytes_read;
                stats.total_bytes_sent += stats.prev_bytes_sent;
                stats.frame_count      += 1;

                invoke_handlers(tx->__postinjection, &data_frame, stats);
            }
        }
    } while(tx->__activated && stats.prev_bytes_read > 0);

    log_info("DxWiFI Transmission stopped");

    send_control_frame(tx, &data_frame, DXWIFI_CONTROL_FRAME_EOT);

    if(out) {
        *out = stats;
    }
}


void stop_transmission(dxwifi_transmitter* tx) {
    if(tx) {
        tx->__activated = false;
    }
}
