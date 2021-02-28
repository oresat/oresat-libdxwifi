/**
 *  tx.c
 * 
 *  DESCRIPTION: DxWiFi Transmission program
 * 
 *  https://github.com/oresat/oresat-dxwifi-software
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include <fcntl.h>
#include <unistd.h>
#include <signal.h>

#include <dxwifi/tx/cli.h>

#include <libdxwifi/dxwifi.h>
#include <libdxwifi/transmitter.h>
#include <libdxwifi/details/logging.h>


dxwifi_transmitter* transmitter = NULL;


void sigint_handler(int signum);

void log_tx_stats(dxwifi_tx_stats stats);

size_t log_frame_stats(dxwifi_tx_frame* frame, size_t payload_size, dxwifi_tx_stats stats, void* user);

size_t delay_transmission(dxwifi_tx_frame* frame, size_t payload_size, dxwifi_tx_stats stats, void* user);

void transmit(cli_args* args, dxwifi_transmitter* tx);

int main(int argc, char** argv) {

    cli_args args = {
        .tx_mode    = TX_STREAM_MODE,
        .verbosity  = DXWIFI_LOG_INFO,
        .tx_delay   = 0,
        .device     = "mon0",

        .tx = {
            .blocksize              = 1024,
            .transmit_timeout       = -1, 
            .redundant_ctrl_frames  = 0,
            .rtap_flags             = IEEE80211_RADIOTAP_F_FCS,
            .rtap_rate_mbps         = 1, 
            .rtap_tx_flags          = IEEE80211_RADIOTAP_F_TX_NOACK,

            .fctl = {
                .protocol_version   = IEEE80211_PROTOCOL_VERSION,
                .type               = IEEE80211_FTYPE_DATA,
                .stype              = { IEEE80211_STYPE_DATA },
                .to_ds              = false,
                .from_ds            = true,
                .more_frag          = false,
                .retry              = false,
                .power_mgmt         = false,
                .more_data          = true, 
                .wep                = false,
                .order              = false
            },

            .address = {0xAA, 0xAA ,0xAA, 0xAA, 0xAA, 0xAA },
        }
    };
    transmitter = &args.tx;

    parse_args(argc, argv, &args);

    init_logging();

    set_log_level(DXWIFI_LOG_ALL_MODULES, args.verbosity);

    init_transmitter(transmitter, args.device);

    if(args.tx_delay > 0 ) {
        attach_preinject_handler(transmitter, delay_transmission, &args.tx_delay);
    }
    if(args.verbosity > DXWIFI_LOG_INFO ) {
        attach_postinject_handler(transmitter, log_frame_stats, NULL);
    }

    transmit(&args, transmitter);

    close_transmitter(transmitter);

    exit(0);
}


void sigint_handler(int signum) {
    // TODO need to come up with a better strategy to gracefully exit the application
    signal(SIGINT, SIG_IGN);
    stop_transmission(transmitter);
    signal(SIGINT, sigint_handler);
}


void transmit(cli_args* args, dxwifi_transmitter* tx) {
    int fd = 0;
    dxwifi_tx_stats tx_stats = {
        .tx_state = DXWIFI_TX_NORMAL
    };

    switch (args->tx_mode)
    {
    case TX_STREAM_MODE:
        signal(SIGINT, sigint_handler);
        start_transmission(tx, STDIN_FILENO, &tx_stats);
        signal(SIGINT, SIG_DFL);
        log_tx_stats(tx_stats);
        break;

    case TX_FILE_MODE:
        for(int i = 0; i < TX_CLI_FILE_MAX && args->files[i] != NULL && tx_stats.tx_state == DXWIFI_TX_NORMAL; ++i) {
            if((fd = open(args->files[i], O_RDONLY)) < 0 ) {
                log_error("Failed to open file: %s", args->files[i]);
            }
            else {
                log_info("Sending %s", args->files[i]);
                signal(SIGINT, sigint_handler);
                start_transmission(tx, fd, &tx_stats);
                signal(SIGINT, SIG_DFL);
                log_tx_stats(tx_stats);
                close(fd);
            }
        }
        break;
    
    default:
        break;
    }
    signal(SIGINT, SIG_DFL);
}


void log_tx_stats(dxwifi_tx_stats stats) {
    log_info(
        "Transmission Stats\n"
        "\tTotal Bytes Read:    %d\n"
        "\tTotal Bytes Sent:    %d\n"
        "\tTotal Frames Sent:   %d\n",
        stats.total_bytes_read,
        stats.total_bytes_sent,
        stats.frame_count
    );
}


size_t log_frame_stats(dxwifi_tx_frame* frame, size_t payload_size, dxwifi_tx_stats stats, void* user) {
    log_debug("Frame: %d - (Read: %ld, Sent: %ld)", stats.frame_count, stats.prev_bytes_read, stats.prev_bytes_sent);
    log_hexdump(frame->__frame, DXWIFI_TX_HEADER_SIZE + payload_size + IEEE80211_FCS_SIZE);
    return payload_size;
}


size_t delay_transmission(dxwifi_tx_frame* frame, size_t payload_size, dxwifi_tx_stats stats, void* user) {
    unsigned delay = *(unsigned*) user;

    usleep(delay);

    return payload_size;
}
