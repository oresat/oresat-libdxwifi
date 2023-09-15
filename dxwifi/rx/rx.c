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
#include <inttypes.h>   // PRIu8, etc.
#include <unistd.h>

#include <sys/mman.h>
#include <linux/limits.h>

#include <dxwifi/rx/cli.h>

#include <libdxwifi/dxwifi.h>
#include <libdxwifi/receiver.h>
#include <libdxwifi/details/assert.h>
#include <libdxwifi/details/logging.h>
#include <libdxwifi/details/syslogger.h>

#define RX_TEMP_FILE "/tmp/rx.raw"

static dxwifi_receiver *receiver = NULL;

/**
 *  DESCRIPTION:    Logs info about the current capture session
 *
 *  ARGUMENTS:
 *
 *      stats:      Stats about the entire capture session
 *
 */
static void
log_rx_stats(dxwifi_rx_stats stats)
{
    char *channel_flags_str = NULL;

    channel_flags_str = radiotap_channel_flags_to_str(stats.rtap.channel.flags);

    log_debug("Receiver Capture Stats\n"
              "\tTotal Payload Size:          %d\n"
              "\tTotal Write length:          %d\n"
              "\tTotal Capture Size:          %d\n"
              "\tTotal Blocks Lost:           %d\n"
              "\tTotal Noise Added:           %d\n"
              "\tBad CRC Count:               %d\n"
              "\tChannel Frequency:           %d\n"
              "\tChannel Mode:                %s\n"
              "\tAntenna:                     %d\n"
              "\tAntenna Signal:              %ddBm\n"
              "\tPackets Processed:           %d\n"
              "\tPackets Received:            %d\n"
              "\tPackets Dropped (receiver):  %d\n"
              "\tPackets Dropped (Kernel):    %d\n"
              "\tPackets Dropped (NIC):       %d\n"
              "\tNote: Packet drop data is platform dependent.\n"
              "\tBlocks lost is tracked only when 'ordered' flag is set",
              stats.total_payload_size, stats.total_writelen,
              stats.total_caplen, stats.total_blocks_lost,
              stats.total_noise_added, stats.bad_crcs,
              stats.rtap.channel.frequency, channel_flags_str,
              stats.rtap.antenna, stats.rtap.ant_signal,
              stats.num_packets_processed, stats.pcap_stats.ps_recv,
              stats.packets_dropped, stats.pcap_stats.ps_drop,
              stats.pcap_stats.ps_ifdrop);

    // Key for known and flags: https://www.radiotap.org/fields/MCS.html

    if ((stats.rtap.mcs.flags & 0x03) == 0) {
        log_debug("MCS bandwidth = 20");
    }
    if ((stats.rtap.mcs.flags & 0x03) == 1) {
        log_debug("MCS bandwidth = 40");
    }
    if ((stats.rtap.mcs.flags & 0x03) == 2) {
        log_debug("MCS bandwidth = 20L");
    }
    if ((stats.rtap.mcs.flags & 0x03) == 3) {
        log_debug("MCS bandwidth = 20U");
    }

    if ((stats.rtap.mcs.flags & 0x04) != 0) {
        log_debug("MCS guard interval: Short");
    } else {
        log_debug("MCS guard interval: Long");
    }

    if ((stats.rtap.mcs.flags & 0x08) != 0) {
        log_debug("MCS HT format: greenfield");
    } else {
        log_debug("MCS HT format: mixed");
    }

    if ((stats.rtap.mcs.flags & 0x10) != 0) {
        log_debug("MCS FEC type: LDPC");
    } else {
        log_debug("MCS FEC type: BCC");
    }

    if (((stats.rtap.mcs.known & 0x20) != 0)
        && ((stats.rtap.mcs.flags & 0x60) != 0)) {

        log_debug("Number of STBC streams: %" PRIu8,
                  (stats.rtap.mcs.flags & 0x60));
    }

    if (((stats.rtap.mcs.known & 0x40) != 0)
        && ((stats.rtap.mcs.flags & 0x80) != 0)) {

        log_debug("Number of extension spatial streams: %" PRIu8,
                  (stats.rtap.mcs.flags & 0x80));
    }

    log_debug("MCS rate index data (flags): 0x%02x", stats.rtap.mcs.flags);
    free(channel_flags_str);
}

/**
 *  DESCRIPTION:    Signals to the receiver to stop capture
 *
 *  ARGUMENTS:
 *
 *      signum:     Received signal
 *
 */
static void
sigint_handler(int signum)
{
    receiver_stop_capture(receiver);
}

/**
 *  DESCRIPTION:    Sets SIGINT handler, captures packets, and restores original
 *                  handler
 *
 *  ARGUMENTS:
 *
 *      rx:         Initialized receiver
 *
 *      fd:         Open file descriptor to output capture data
 *
 */
static dxwifi_rx_state_t
set_handler_and_capture(dxwifi_receiver *rx, int fd)
{
    dxwifi_rx_stats stats = { 0 };
    struct sigaction action = { 0 };
    struct sigaction prev_action = { 0 };

    sigemptyset(&action.sa_mask);
    sigaddset(&action.sa_mask, SIGINT);
    action.sa_handler = sigint_handler;

    sigaction(SIGINT, &action, &prev_action);
    receiver_activate_capture(rx, fd, &stats);
    sigaction(SIGINT, &prev_action, NULL);

    log_rx_stats(stats);
    return stats.capture_state;
}

/**
 *  DESCRIPTION:    Attempts to open or create a file and listen for activate
 *                  packet capture
 *
 *  ARGUMENTS:
 *
 *      path:       Path to the file to be opened or created
 *
 *      rx:         Initialized receiver
 *
 *      append:     If true, open file in append mode; otherwise, truncate file
 *
 *  RETURNS:
 *
 *      dxwifi_rx_state_t:  Last reported state of the receiver
 *
 */
static dxwifi_rx_state_t
open_file_and_capture(const char *path, dxwifi_receiver *rx, bool append)
{
    const mode_t mode = S_IRUSR|S_IWUSR|S_IROTH|S_IWOTH;

    dxwifi_rx_state_t state = DXWIFI_RX_ERROR;

    int encoded_fd = 0;
    void *encoded_data = NULL;
    off_t encoded_size = 0;

    int decoded_fd = 0;
    void *decoded_data = NULL;
    ssize_t decoded_size = 0;
    ssize_t bytes_written = 0;

    // Receive encoded data to a temp file and map to memory
    encoded_fd = open(RX_TEMP_FILE, O_RDWR|O_CREAT|O_TRUNC, mode);
    if (encoded_fd < 0) {
        log_error("Failed to open " RX_TEMP_FILE " for receiving encoded data: "
                  "%s",
                  strerror(errno));
        goto done;
    }

    state = set_handler_and_capture(rx, encoded_fd);
    if (state == DXWIFI_RX_ERROR) {
        log_error("Error in packet capture");
        goto done;
    }

    encoded_size = get_file_size(RX_TEMP_FILE);
    if (encoded_size <= 0) {
        log_warning("No packets were captured. Verify capture parameters");
        goto done;
    }

    encoded_data = mmap(NULL, encoded_size, PROT_WRITE, MAP_SHARED, encoded_fd,
                        0);
    if (encoded_data == MAP_FAILED) {
        log_error("Failed to map encoded data file " RX_TEMP_FILE " to memory: "
                  "%s",
                  strerror(errno));
        goto done;
    }

    // Decode received data and store in output file
    decoded_fd = open(path, O_WRONLY|O_CREAT|(append ? O_APPEND : O_TRUNC),
                      mode);
    if (decoded_fd < 0) {
        log_error("Failed to open %s for storing decoded data: %s",
                  path, strerror(errno));
        goto done;
    }

    decoded_size = dxwifi_decode(encoded_data, encoded_size, &decoded_data);
    if (decoded_size <= 0) {
        log_error("Failed to decode received data: %s",
                  dxwifi_fec_error_to_str(decoded_size));
        goto done;
    }

    log_info("Successfully decoded received data (size=%lld)",
             (long long) decoded_size);

    bytes_written = write(decoded_fd, decoded_data, decoded_size);
    if (bytes_written < 0) {
        log_error("Write of decoded data to %s failed: %s",
                  path, strerror(errno));
        goto done;
    }

    // @TODO If the event of a partial write, try to write the remaining data
    if (bytes_written < decoded_size) {
        log_error("Partial write: %lld of %lld decoded bytes written to %s",
                  (long long) bytes_written, (long long) decoded_size, path);
    }

done:
    if (encoded_fd >= 0) {
        close(encoded_fd);
        if ((encoded_data != NULL) && (encoded_data != MAP_FAILED)) {
            munmap(encoded_data, encoded_size);
            remove(RX_TEMP_FILE);
        }
    }

    if (decoded_fd >= 0) {
        close(decoded_fd);
        free(decoded_data);
    }

    return state;
}

/**
 *  DESCRIPTION:    Attempts to open a directory and create files for capture
 *                  output
 *
 *  ARGUMENTS:
 *
 *      args:       Parsed command line arguments
 *
 *      rx:         Initialized receiver
 *
 */
static void
capture_in_directory(cli_args *args, dxwifi_receiver *rx)
{
    dxwifi_rx_state_t state = DXWIFI_RX_NORMAL;
    char path[PATH_MAX];
    int count = 0;

    while (state == DXWIFI_RX_NORMAL) {
        snprintf(path, PATH_MAX, "%s/%s_%.5d.%s",
                 args->output_path, args->file_prefix, count++,
                 args->file_extension);

        state = open_file_and_capture(path, rx, args->append);
    }
}

/**
 *  DESCRIPTION:    Determine receive mode and activate packet capture
 *
 *  ARGUMENTS:
 *
 *      args:       Parsed command line arguments
 *
 *      rx:         Initialized receiver
 *
 */
static void
receive(cli_args *args, dxwifi_receiver *rx)
{
    switch (args->rx_mode) {
        case RX_STREAM_MODE:
            // Capture everything and output to stdout
            set_handler_and_capture(rx, STDOUT_FILENO);
            break;

        case RX_FILE_MODE:
            // Capture everything into a single file
            open_file_and_capture(args->output_path, rx, args->append);
            break;

        case RX_DIRECTORY_MODE:
            // Create new files whenever an EOT is signalled
            capture_in_directory(args, rx);
            break;

        default:
            break;
    }
}

int
main(int argc, char **argv)
{
    cli_args args = DEFAULT_CLI_ARGS;
    receiver = &args.rx;

    parse_args(argc, argv, &args);

    if (args.use_syslog) {
        set_logger(DXWIFI_LOG_ALL_MODULES, syslogger);
    }
    set_log_level(DXWIFI_LOG_ALL_MODULES, args.verbosity);

    init_receiver(receiver, args.device);
    receive(&args, receiver);
    close_receiver(receiver);

    exit(0);
}
