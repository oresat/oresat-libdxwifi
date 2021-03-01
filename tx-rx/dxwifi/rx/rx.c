/**
 *  rx.c
 * 
 *  DESCRIPTION: DxWiFi Receiver program
 * 
 *  https://github.com/oresat/oresat-dxwifi-software
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

#include <fcntl.h>
#include <unistd.h>

#include <linux/limits.h>

#include <dxwifi/rx/cli.h>

#include <libdxwifi/dxwifi.h>
#include <libdxwifi/receiver.h>
#include <libdxwifi/details/logging.h>


dxwifi_receiver* receiver = NULL;


void sigint_handler(int signum);

void log_rx_stats(dxwifi_rx_stats stats);

void receive(cli_args* args, dxwifi_receiver* rx);

void capture_in_directory(cli_args* args, dxwifi_receiver* rx);

dxwifi_rx_state_t open_file_and_capture(const char* path, dxwifi_receiver* rx, bool append);


int main(int argc, char** argv) {
    cli_args args = {
        .rx_mode        = RX_STREAM_MODE,
        .verbosity      = DXWIFI_LOG_INFO,
        .append         = false,
        .device         = "mon0",
        .output_path    = ".",
        .file_prefix    = "rx",
        .file_extension = "cap",
        .rx = {
            .dispatch_count     = 32,
            .capture_timeout    = -1, // No timeout
            .packet_buffer_size = DXWIFI_RX_PACKET_BUFFER_SIZE_MAX,
            .ordered            = false,
            .add_noise          = false,
            .noise_value        = 0xff,
            .filter             = "wlan addr2 aa:aa:aa:aa:aa:aa",
            .optimize           = true,
            .snaplen            = DXWIFI_SNAPLEN_MAX,
            .pb_timeout         = DXWIFI_DFLT_PACKET_BUFFER_TIMEOUT
        }
    };
    receiver = &args.rx;

    parse_args(argc, argv, &args);

    init_logging();

    set_log_level(DXWIFI_LOG_ALL_MODULES, args.verbosity);

    init_receiver(receiver, args.device);

    receive(&args, receiver);

    close_receiver(receiver);

    exit(0);
}


dxwifi_rx_state_t open_file_and_capture(const char* path, dxwifi_receiver* rx, bool append) {
    int fd          = 0;
    int open_flags  = O_WRONLY | O_CREAT | (append ? O_APPEND : 0);
    mode_t mode     = S_IRUSR  | S_IWUSR | S_IROTH | S_IWOTH; 

    dxwifi_rx_stats stats;
    if((fd = open(path, open_flags, mode)) < 0) {
        log_error("Failed to open file: %s", path);
        stats.capture_state = DXWIFI_RX_ERROR;
    }
    else {
        signal(SIGINT, sigint_handler);
        receiver_activate_capture(rx, fd, &stats);
        signal(SIGINT, SIG_DFL);
        log_rx_stats(stats);
        close(fd);
    }
    return stats.capture_state;
}


void capture_in_directory(cli_args* args, dxwifi_receiver* rx) {
    int count = 0;
    char path[PATH_MAX]; 

    dxwifi_rx_state_t state = DXWIFI_RX_NORMAL;
    while(state == DXWIFI_RX_NORMAL) {
        sprintf(path, "%s/%s_%d.%s", args->output_path, args->file_prefix, count++, args->file_extension);

        state = open_file_and_capture(path, rx, args->append);
    }
}


void receive(cli_args* args, dxwifi_receiver* rx) {

    dxwifi_rx_stats stats;
    switch (args->rx_mode)
    {
    case RX_STREAM_MODE:
        signal(SIGINT, sigint_handler);
        receiver_activate_capture(rx, STDOUT_FILENO, &stats);
        signal(SIGINT, SIG_DFL);
        log_rx_stats(stats);
        break;

    case RX_FILE_MODE:
        open_file_and_capture(args->output_path, rx, args->append);
        break;

    case RX_DIRECTORY_MODE:
        capture_in_directory(args, rx);
        break;
    
    default:
        break;
    }

}


void log_rx_stats(dxwifi_rx_stats stats) {
    log_info(
        "Receiver Capture Stats\n"
        "\tTotal Payload Size:          %d\n"
        "\tTotal Write length:          %d\n"
        "\tTotal Capture Size:          %d\n"
        "\tTotal Blocks Lost:           %d\n"
        "\tTotal Noise Added:           %d\n"
        "\tPackets Received:            %d\n"
        "\tPackets Dropped (Kernel):    %d\n"
        "\tPackets Dropped (NIC):       %d\n"
        "\tNote: Packet drop data is platform dependent.\n"
        "\tBlocks lost is only valid when `ordered` flag is set\n",
        stats.total_payload_size,
        stats.total_writelen,
        stats.total_caplen,
        stats.total_blocks_lost,
        stats.total_noise_added,
        stats.pcap_stats.ps_recv,
        stats.pcap_stats.ps_drop,
        stats.pcap_stats.ps_ifdrop
    );
}


void sigint_handler(int signum) {
    // TODO need to come up with a better strategy to gracefully exit the application
    signal(SIGINT, SIG_IGN);
    receiver_stop_capture(receiver);
    signal(SIGINT, sigint_handler);
}
