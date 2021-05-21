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
#include <libdxwifi/details/assert.h>
#include <libdxwifi/details/logging.h>
#include <libdxwifi/details/syslogger.h>

//Syscalls for Memory Mapping
#include <sys/mman.h>


dxwifi_receiver* receiver = NULL;


void receive(cli_args* args, dxwifi_receiver* rx);


int main(int argc, char** argv) {
    cli_args args = DEFAULT_CLI_ARGS;
    receiver = &args.rx;

    parse_args(argc, argv, &args);

    if(args.use_syslog) {
        set_logger(DXWIFI_LOG_ALL_MODULES, syslogger);
    }

    set_log_level(DXWIFI_LOG_ALL_MODULES, args.verbosity);

    init_receiver(receiver, args.device);

    receive(&args, receiver);

    close_receiver(receiver);

    exit(0);
}


/**
 *  DESCRIPTION:    Logs info about the current capture session
 * 
 *  ARGUMENTS: 
 *      
 *      stats:      Stats about the entire capture session
 * 
 */
void log_rx_stats(dxwifi_rx_stats stats) {
    log_debug(
        "Receiver Capture Stats\n"
        "\tTotal Payload Size:          %d\n"
        "\tTotal Write length:          %d\n"
        "\tTotal Capture Size:          %d\n"
        "\tTotal Blocks Lost:           %d\n"
        "\tTotal Noise Added:           %d\n"
        "\tPackets Processed:           %d\n"
        "\tPackets Received:            %d\n"
        "\tPackets Dropped (receiver):  %d\n"
        "\tPackets Dropped (Kernel):    %d\n"
        "\tPackets Dropped (NIC):       %d\n"
        "\tNote: Packet drop data is platform dependent.\n"
        "\tBlocks lost is only valid when `ordered` flag is set\n",
        stats.total_payload_size,
        stats.total_writelen,
        stats.total_caplen,
        stats.total_blocks_lost,
        stats.total_noise_added,
        stats.num_packets_processed,
        stats.pcap_stats.ps_recv,
        stats.packets_dropped,
        stats.pcap_stats.ps_drop,
        stats.pcap_stats.ps_ifdrop
    );
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
 *      rx:         Initialized reciever
 * 
 *      append:     Oppen file in append mode?
 * 
 *  RETURNS:
 *     
 *      dxwifi_rx_state_t:  Last reported state of the receiver
 * 
 */
dxwifi_rx_state_t open_file_and_capture(const char* path, dxwifi_receiver* rx, bool append) {
    int fd          = 0; //output file descriptor
    int temp_fd     = 0; //temp file descriptor
    int open_flags  = O_WRONLY | O_CREAT | (append ? O_APPEND : 0);
    mode_t mode     = S_IRUSR  | S_IWUSR | S_IROTH | S_IWOTH; 

    dxwifi_rx_state_t state = DXWIFI_RX_ERROR;
    //Open temp file, with RW + Create
    //Error if unable to open
    if((temp_fd = open("/var/tmp/EncodedFile", O_RDWR | O_CREAT, mode)) < 0) {
        log_error("Failed to open file: /var/tmp/EncodedFile");
    }
    //Otherwise, begin capture and decoding.
    else {
        state = setup_handlers_and_capture(rx, temp_fd);

        off_t temp_file_size = get_file_size("/var/tmp/EncodedFile");
        
        //Map the encoded file to memory
        void* encoded_data = mmap(NULL, temp_file_size, PROT_READ, MAP_SHARED, temp_fd, 0);
        assert_M(encoded_data != MAP_FAILED, "Failed to map file to memory - %s", strerror(errno));

        //If the file was transferred correctly and mapped to memory without errors...
        if(state == DXWIFI_RX_NORMAL) {
            //Open final file
            //Error if file failed to open
            if((fd = open(path, open_flags, mode)) < 0) {
                log_error("Failed to open file: %s", path);
            }
            //Otherwise run FEC decoding.
            else {
                void *decoded_message = NULL;
                size_t decoded_size = dxwifi_decode(encoded_data, temp_file_size, &decoded_message);
                //write to output file and close
                ssize_t written_data = write(fd, decoded_message, decoded_size);
                if(written_data == -0) { log_error("No data written."); }
                if(written_data == -1) { log_error("An error occured, ErrNo: %d", errno); }
                //Do we need a condition for when written data < temp_file_size?
                close(fd);
                //now free and unmap memory assigned
                free(decoded_message);
                munmap(decoded_message, decoded_size);
            }
        }
        //now close and remove temp file
        close(temp_fd);
        remove("/var/tmp/EncodedFile");
        //Unmap memory assigned to the encoded temp file.
        munmap(encoded_data, temp_file_size);
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
 *      rx:         Initialized reciever
 * 
 */
void capture_in_directory(cli_args* args, dxwifi_receiver* rx) {
    int count = 0;
    char path[PATH_MAX]; 

    dxwifi_rx_state_t state = DXWIFI_RX_NORMAL;
    while(state == DXWIFI_RX_NORMAL) {
        snprintf(path, PATH_MAX, "%s/%s_%d.%s", args->output_path, args->file_prefix, count++, args->file_extension);

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
 *      rx:         Initialized reciever
 * 
 */
void receive(cli_args* args, dxwifi_receiver* rx) {

    switch (args->rx_mode)
    {
    case RX_STREAM_MODE: // Capture everything and output to stdout
        setup_handlers_and_capture(rx, STDOUT_FILENO);
        break;

    case RX_FILE_MODE: // Capture everything into a single file
        open_file_and_capture(args->output_path, rx, args->append);
        break;

    case RX_DIRECTORY_MODE: // Create new files whenever an EOT is signalled
        capture_in_directory(args, rx);
        break;
    
    default:
        break;
    }

}
