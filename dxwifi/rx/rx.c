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
void log_rx_stats(dxwifi_rx_stats stats) {
    char* channel_flags_str = radiotap_channel_flags_to_str(stats.rtap.channel.flags);

    log_debug(
        "Receiver Capture Stats\n"
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
        "\tBlocks lost is only tracked when `ordered` flag is set",
        stats.total_payload_size,
        stats.total_writelen,
        stats.total_caplen,
        stats.total_blocks_lost,
        stats.total_noise_added,
        stats.bad_crcs,
        stats.rtap.channel.frequency,
        channel_flags_str,
        stats.rtap.antenna,
        stats.rtap.ant_signal,
        stats.num_packets_processed,
        stats.pcap_stats.ps_recv,
        stats.packets_dropped,
        stats.pcap_stats.ps_drop,
        stats.pcap_stats.ps_ifdrop
    );
    if((stats.rtap.mcs.flags & 0x03) == 0){
        log_debug("MCS Bandwidth = 20");
    }
    if((stats.rtap.mcs.flags & 0x03) == 1){
        log_debug("MCS Bandwith = 40");
    }
    if((stats.rtap.mcs.flags & 0x03) == 2){
        log_debug("MCS Bandwith = 20L");
    }
    if((stats.rtap.mcs.flags & 0x03) == 3){
        log_debug("MCS Bandwidth = 20U");
    }
    if((stats.rtap.mcs.flags & 0x04) == 0){
        log_debug("MCS Guard Interval: Long");
    }
    if((stats.rtap.mcs.flags & 0x04) != 0){
        log_debug("MCS Guard Interval: Short");
    }
    if((stats.rtap.mcs.flags & 0x08) == 0){
        log_debug("MCS HT Format: Mixed");
    }
    if((stats.rtap.mcs.flags & 0x08) != 0){
        log_debug("MCS HT Format: Greenfield");
    }
    if((stats.rtap.mcs.flags & 0x10) == 0){
        log_debug("MCS FEC Type: BCC");
    }
    if((stats.rtap.mcs.flags & 0x10) != 0){
        log_debug("MCS FEC Type: LDPC");
    }
    if((stats.rtap.mcs.known & 0x20) && ((stats.rtap.mcs.flags & 0x60) != 0)){
        log_debug("Number of STBC Streams: %u", (stats.rtap.mcs.flags & 0x60));
    }
    if((stats.rtap.mcs.known & 0x40) && ((stats.rtap.mcs.flags & 0x80) != 0)){
        log_debug("Number of Extension Spatial Streams: %u", (stats.rtap.mcs.flags & 0x80));
    }
    log_debug(
        "MCS Rate Index Data (Flags): 0x%02x",   
        stats.rtap.mcs.flags
    );
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
void sigint_handler(int signum) {
    receiver_stop_capture(receiver);
}


/**
 *  DESCRIPTION:    Setups and tearsdown SIGINT handlers to control capture
 * 
 *  ARGUMENTS: 
 *      
 *      rx:         Initialized receiver
 * 
 *      fd:         Opened file descriptor to output capture data
 * 
 */
dxwifi_rx_state_t setup_handlers_and_capture(dxwifi_receiver* rx, int fd) {
    dxwifi_rx_stats stats;

    struct sigaction action = { 0 }, prev_action = { 0 };
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
 *      append:     Oppen file in append mode?
 * 
 *  RETURNS:
 *     
 *      dxwifi_rx_state_t:  Last reported state of the receiver
 * 
 */
dxwifi_rx_state_t open_file_and_capture(const char* path, dxwifi_receiver* rx, bool append) {
    int fd_out      = 0;
    int temp_fd     = 0;

    int temp_flags  = O_RDWR   | O_CREAT | O_TRUNC;
    int open_flags  = O_WRONLY | O_CREAT | (append ? O_APPEND : O_TRUNC);
    mode_t mode     = S_IRUSR  | S_IWUSR | S_IROTH | S_IWOTH; 
    
    dxwifi_rx_state_t state = DXWIFI_RX_ERROR;

    if((temp_fd = open(RX_TEMP_FILE, temp_flags, mode)) < 0) {
        log_error("Failed to open temp file for capture");
    }
    else {

        state = setup_handlers_and_capture(rx, temp_fd);
        off_t temp_file_size = get_file_size(RX_TEMP_FILE);

        if(temp_file_size > 0) {
            //Map the encoded file to memory
            void* encoded_data = mmap(NULL, temp_file_size, PROT_WRITE, MAP_SHARED, temp_fd, 0);
            assert_M(encoded_data != MAP_FAILED, "Failed to map file to memory - %s", strerror(errno));
            
            if(state != DXWIFI_RX_ERROR) {
                if((fd_out = open(path, open_flags, mode)) < 0) {
                    log_error("Failed to open file: %s", path);
                }
                else {

                    void *decoded_message = NULL;
                    ssize_t decoded_size = dxwifi_decode(encoded_data, temp_file_size, &decoded_message);

                    if(decoded_size > 0) {
                        log_info("Decoding Success for RX'd file, File Size: %d", decoded_size);

                        ssize_t nbytes = write(fd_out, decoded_message, decoded_size);
                        assert_M(decoded_size == nbytes, "Partial write occured: %d/%d - %s", nbytes, decoded_size, strerror(errno));
                        free(decoded_message);
                    }
                    else{
                        log_error("Failed to Decode Rx'd file, Error: %s", dxwifi_fec_error_to_str(decoded_size));
                    }
                    close(fd_out);
                }
            }
            munmap(encoded_data, temp_file_size);
        }
        else {
            log_warning("No packets were captured. Verify capture parameters");
        }
        close(temp_fd);
        remove(RX_TEMP_FILE);
    }
    return state;
}


/**
 *  DESCRIPTION:    Attempts to open a directory and create files for capture output
 * 
 *  ARGUMENTS: 
 *      
 *      args:       Parsed command line arguments
 * 
 *      rx:         Initialized receiver
 * 
 */
void capture_in_directory(cli_args* args, dxwifi_receiver* rx) {
    int count = 0;
    char path[PATH_MAX]; 

    dxwifi_rx_state_t state = DXWIFI_RX_NORMAL;
    while(state == DXWIFI_RX_NORMAL) {
        snprintf(path, PATH_MAX, "%s/%s_%.5d.%s", args->output_path, args->file_prefix, count++, args->file_extension);

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
            setup_handlers_and_capture(rx, STDOUT_FILENO);
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
