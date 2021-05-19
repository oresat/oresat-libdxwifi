/**
 *  tx.c
 *
 *  DESCRIPTION: DxWiFi Transmission program
 *
 *  https://github.com/oresat/oresat-dxwifi-software
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <poll.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <dirent.h>
#include <fnmatch.h>

#include <arpa/inet.h>
#include <linux/limits.h>

#include <dxwifi/tx/cli.h>

#include <libdxwifi/dxwifi.h>
#include <libdxwifi/transmitter.h>
#include <libdxwifi/details/utils.h>
#include <libdxwifi/details/daemon.h>
#include <libdxwifi/details/logging.h>
#include <libdxwifi/details/dirwatch.h>
#include <libdxwifi/details/syslogger.h>

//Syscalls for Memory Mapping
#include <sys/mman.h>


typedef struct {
    float packet_loss_rate;
    unsigned count;
} packet_loss_stats;

dirwatch* dirwatch_handle = NULL;
dxwifi_transmitter* transmitter = NULL;


// Forward declare
void terminate(int signum);
void transmit(cli_args* args, dxwifi_transmitter* tx);

int main(int argc, char** argv) {

    cli_args args = DEFAULT_CLI_ARGS;
    transmitter = &args.tx;

    parse_args(argc, argv, &args);

    set_log_level(DXWIFI_LOG_ALL_MODULES, args.verbosity);

    if(args.use_syslog) {
        set_logger(DXWIFI_LOG_ALL_MODULES, syslogger);
    }
    if(args.daemon) {
        daemon_run(args.pid_file, args.daemon);
        signal(SIGTERM, terminate);
    }

    srand(time(0)); // Seed random number generator

    init_transmitter(transmitter, args.device);

    transmit(&args, transmitter);

    close_transmitter(transmitter);

    if(args.daemon == DAEMON_START) { // This process is the daemon, tear it down
        stop_daemon(args.pid_file);
    }

    exit(0);
}

/**
 *  DESCRIPTION:    SIGTERM handler for daemonized process. Ensures transmitter
 *                  is closed.
 *
 *  ARGUMENTS:
 *
 *      signum:     Received signal
 *
 */
void terminate(int signum) {
    stop_transmission(transmitter);
    close_transmitter(transmitter);
    exit(signum);
}


/**
 *  DESCRIPTION:    Signals to the transmitter to stop transmission
 *
 *  ARGUMENTS:
 *
 *      signum:     Received signal
 *
 */
void tx_sigint_handler(int signum) {
    stop_transmission(transmitter);
}


/**
 *  DESCRIPTION:    Signals to the watch loop to close out
 *
 *  ARGUMENTS:
 *
 *      signum:     Received signal
 *
 */
void watchdir_sigint_handler(int signum) {
    dirwatch_stop(dirwatch_handle);
}


/**
 *  DESCRIPTION:    Log info about the transmitted file
 *
 *  ARGUMENTS:
 *
 *      stats:      Accumulated statistics about the transmission
 *
 */
void log_tx_stats(dxwifi_tx_stats stats) {
    log_debug(
        "Transmission Stats\n"
        "\tTotal Bytes Read:    %d\n"
        "\tTotal Bytes Sent:    %d\n"
        "\tData Frames Sent:    %d\n"
        "\tCtrl Frames Sent:    %d\n",
        stats.total_bytes_read,
        stats.total_bytes_sent,
        stats.data_frame_count,
        stats.ctrl_frame_count
    );
}


/**
 *  DESCRIPTION:    Called everytime a frame is injected, logs stats about the
 *                  transmitted frame.
 *
 *  ARGUMENTS:
 *
 *      See definition of dxwifi_tx_frame_cb in transmitter.h
 *
 */
bool log_frame_stats(dxwifi_tx_frame* frame, dxwifi_tx_stats stats, void* user) {
    if(stats.frame_type == DXWIFI_CONTROL_FRAME_NONE) {
        log_debug("Frame: %d - (Read: %ld, Sent: %ld)", stats.data_frame_count, stats.prev_bytes_read, stats.prev_bytes_sent);
    }
    else {
        log_debug("%s Frame Sent: %d", control_frame_type_to_str(stats.frame_type), stats.prev_bytes_sent);
    }
    log_hexdump(frame, DXWIFI_TX_FRAME_SIZE);
    return true;
}


/**
 *  DESCRIPTION:    Called before every frame is injected, adds millisecond
 *                  delay to transmission
 *
 *  ARGUMENTS:
 *
 *      See definition of dxwifi_tx_frame_cb in transmitter.h
 *
 */
bool delay_transmission(dxwifi_tx_frame* frame, dxwifi_tx_stats stats, void* user) {
    unsigned delay_ms = *(unsigned*) user;

    msleep(delay_ms, false);
    return true;
}

/**
 *  DESCRIPTION:    Called before every frame is injected, will intentionally
 *                  drop packets according to packet loss rate
 *
 *  ARGUMENTS:
 *
 *      packet_loss_rate:        Float percentage of packets lost
 *
 */
bool packet_loss_sim(dxwifi_tx_frame* frame, dxwifi_tx_stats stats, void* user) {
    packet_loss_stats * plstats = (packet_loss_stats*) user;
    //generate random num withing range
    float random = (float)rand() / (float)RAND_MAX;

    if(plstats->packet_loss_rate > random){
        plstats->count++;
        return false;
    }
    return true;
}

/**
 *  DESCRIPTION:    Called before every frame is injected, will intentionally
 *                  flip bits in the frame excluding the Radiotap header
 *
 *  ARGUMENTS:
 *
 *     error_rate:      Float percentage of bits flipped
 *
 */
bool bit_error_rate_sim(dxwifi_tx_frame* frame, dxwifi_tx_stats stats, void* user) {
    float error_rate = *(float*) user;

    int frame_size = DXWIFI_TX_FRAME_SIZE - DXWIFI_TX_RADIOTAP_HDR_SIZE;
    int total_num_errors = DXWIFI_TX_FRAME_SIZE * 8 * error_rate; //Get total number of errors
    uint8_t bit_array[frame_size]; //Make an array of bits equal to the number in the frame

    // Initialize every bit to zero
    memset(bit_array, 0, frame_size);

    // Skip the bits in the radiotap header since this is discarded pre-flight anyways
    uint8_t* buffer = ((uint8_t*)frame) + DXWIFI_TX_RADIOTAP_HDR_SIZE;

    for(int i = 0; i < total_num_errors; ++i){
        uint32_t chosen_byte = rand() % frame_size;
        int chosen_bit = 1 << (rand() % 8);

        if((bit_array[chosen_byte] & chosen_bit) == 0) { //Flip bit if unseen
            buffer[chosen_byte] ^= chosen_bit; // Toggle bit in frame
            bit_array[chosen_byte] &= chosen_bit;   // Set bit in the set
        }
        else{ //Reroll for different bit
            --i;
        }
    }
    log_debug("Bits in frame: %d, bits flipped: %d", frame_size * 8, total_num_errors);
    return true;
}

/**
 *  DESCRIPTION:    Called before every frame is injected, packs the current
 *                  frame count into the last four bytes of the MAC headers
 *                  addr1 field
 *
 *  ARGUMENTS:
 *
 *      See definition of dxwifi_tx_frame_cb in transmitter.h
 *
 */
bool attach_frame_number(dxwifi_tx_frame* frame, dxwifi_tx_stats stats, void* user) {
    uint32_t frame_no = htonl(stats.data_frame_count);

    memcpy(frame->mac_hdr.addr1 + 2, &frame_no, sizeof(uint32_t));

    return true;
}


/**
 *  DESCRIPTION:    Setups and tearsdown SIGINT handlers to control transmission
 *
 *  ARGUMENTS:
 *
 *      tx:         Initialized transmitter
 *
 *      fd:         Opened file descriptor of the file to be transmitted
 *
 */
dxwifi_tx_state_t setup_handlers_and_transmit(dxwifi_transmitter* tx, int fd) {
    dxwifi_tx_stats stats;

    struct sigaction action = { 0 }, prev_action = { 0 };

    sigemptyset(&action.sa_mask);
    sigaddset(&action.sa_mask, SIGINT);
    action.sa_handler = tx_sigint_handler;

    sigaction(SIGINT, &action, &prev_action);
    start_transmission(tx, fd, &stats);
    sigaction(SIGINT, &prev_action, NULL);

    log_tx_stats(stats);
    return stats.tx_state;
}


/**
 *  DESCRIPTION:    Iterates through a list of file names, opens them, and
 *                  transmits them
 *
 *  ARGUMENTS:
 *
 *      tx:         Initialized transmitter
 *
 *      files:      List of files to transmit
 *
 *      delay:      Millisecond delay to add between file transmission.
 *
 *      retransmit_count:
 *                  Number of times to retransmit the file. If the count is -1
 *                  then the file will be retransmitted forever or until the
 *                  transmitter reports a timeout or error
 *		coderate:
 *					Coderate for FEC encoding.
 *  RETURNS:
 *
 *      dxwifi_tx_state_t: The last reported state of the transmitter
 *
 */
dxwifi_tx_state_t transmit_files(dxwifi_transmitter* tx, char** files, size_t num_files, unsigned delay, int retransmit_count, float coderate) {
    int fd = 0;
    dxwifi_tx_stats stats = { .tx_state = DXWIFI_TX_NORMAL };

    for(size_t i = 0; i < num_files && stats.tx_state == DXWIFI_TX_NORMAL; ++i) {
        if((fd = open(files[i], O_RDONLY)) < 0) {
            log_error("Failed to open file: %s - %s", files[i], strerror(errno));
        }
        else {
            log_info("Opened %s for transmission", files[i]);
            off_t file_size = get_file_size(files[i]);

            //Map the unencoded file to memory
            void* unencoded_data = mmap(NULL, file_size, PROT_READ, MAP_SHARED, fd, 0);
            assert_M(unencoded_data != MAP_FAILED, "Failed to map file to memory - %s", strerror(errno));

            //FEC Encoding Below
            void *FECd_message = NULL;
            size_t FECd_size = dxwifi_encode(unencoded_data, file_size, coderate, &FECd_message);

            //On Encode Success
            if(FECd_size > 0){
            	log_info("Encoding Success for file: [%s], Filesize: %d", files[i], FECd_size);

            	int count = retransmit_count;
            	bool transmit_forever = (retransmit_count == -1);
            	
            	while((count >= 0 || transmit_forever) && stats.tx_state == DXWIFI_TX_NORMAL) {
                	
                	//Transmit the message
                	transmit_bytes(tx, FECd_message, FECd_size, &stats);
                	
                	msleep(delay, false);

                	//Release memory used for the FEC'd block
                	free(FECd_message);
                	--count;
                }
            }
            //On Encode Failure
            else {	log_error("Unable to FEC Encode File [%s]", files[i]);	}
            //Close and unmap mapped memory for next file
            close(fd);
            munmap(FECd_message, file_size);
        }
    }
    return stats.tx_state;
}


/**
 *  DESCRIPTION:    Transmit all files in a directory that matches a filter
 *
 *  ARGUMENTS:
 *
 *      tx:         Initialized transmitter
 *
 *      filter:     Glob pattern to filter which files should be transmitted
 *
 *      dirname:    Name of target directory
 *
 *      delay:      Inter-file transmission delay in milliseconds
 *
 *		coderate:   Coderate for FEC Encoding
 *
 */
void transmit_directory_contents(dxwifi_transmitter* tx, const char* filter, const char* dirname, unsigned delay, int retransmit_count, float coderate) {
    DIR* dir;
    struct dirent* file;
    dxwifi_tx_state_t state = DXWIFI_TX_NORMAL;
    char* path_buffer = calloc(PATH_MAX, sizeof(char));

    if((dir = opendir(dirname)) == NULL) {
        log_error("Failed to open directory: %s - %s", dirname, strerror(errno));
    }
    else {
        while((file = readdir(dir)) && state == DXWIFI_TX_NORMAL) {
            if(fnmatch(filter, file->d_name, 0) == 0) {
                combine_path(path_buffer, PATH_MAX, dirname, file->d_name);
                if(is_regular_file(path_buffer)) {
                    state = transmit_files(tx, &path_buffer, 1, delay, retransmit_count, coderate);
                }
            }
        }
        closedir(dir);
    }
    free(path_buffer);
}


/**
 *  DESCRIPTION:    Dirwatch callback, transmits newly created file
 *
 *  ARGUMENTS:
 *
 *      event:      Creation and close event
 *
 *      user:       Command line arguments
 *
 */
static void transmit_new_file(const dirwatch_event* event, void* user) {
    cli_args* args = (cli_args*) user;

    char* path_buffer = calloc(PATH_MAX, sizeof(char));

    combine_path(path_buffer, PATH_MAX, event->dirname, event->filename);

    transmit_files(&args->tx, &path_buffer, 1, args->file_delay, args->retransmit_count, args-> coderate);

    free(path_buffer);
}


/**
 *  DESCRIPTION:    Transmits current directory contents and listens for newly
 *                  created files to transmit
 *
 *  ARGUMENTS:
 *
 *      args:       Parsed command line arguments
 *
 *      tx:         Initialized transmitter
 *
 */
void transmit_directory(cli_args* args, dxwifi_transmitter* tx) {

    const char* dirname = args->files[0];

    if(args->transmit_current_files) {
        transmit_directory_contents(tx, args->file_filter, dirname, args->file_delay, args->retransmit_count, args->coderate);
    }
    if(args->listen_for_new_files) {

        dirwatch_handle = dirwatch_init();

        dirwatch_add(dirwatch_handle, dirname, args->file_filter, DW_CREATE_AND_CLOSE, true);

        // Setup handlers for exiting loop
        struct sigaction action = { 0 }, prev_action = { 0 };
        memset(&prev_action, 0x00, sizeof(sigaction));
        sigemptyset(&action.sa_mask);
        sigaddset(&action.sa_mask, SIGINT);
        action.sa_handler = watchdir_sigint_handler;
        sigaction(SIGINT, &action, &prev_action);

        dirwatch_listen(dirwatch_handle, args->dirwatch_timeout * 1000, transmit_new_file, args);

        sigaction(SIGINT, &prev_action, NULL);

        dirwatch_close(dirwatch_handle);
    }
}


/**
 *  DESCRIPTION:    Transmit a test sequence of bytes. The test sequence is a
 *                  10Kb block of data with the transmission count encoded in
 *                  every four bytes as the payload
 *
 *  ARGUMENTS:
 *
 *      tx:         Initialized transmitter
 *
 *      retransmit: Number of times to transmit test sequence
 *
 */
void transmit_test_sequence(dxwifi_transmitter* tx, int retransmit) {

    dxwifi_tx_stats stats;

    bool transmit_forever = (retransmit == -1);

    uint32_t count = 0;

    // TODO magic number
    uint32_t transmit_buffer[10240 / sizeof(uint32_t)];

    log_info("Transmitting test sequence...");
    while (count <= retransmit || transmit_forever) {

        for(size_t i = 0; i < NELEMS(transmit_buffer); ++i) {
            transmit_buffer[i] = count;
        }

        transmit_bytes(tx, transmit_buffer, 10240, &stats);

        log_tx_stats(stats);

        ++count;
    }
    log_info("Test sequence completed, transmitted %d times", count);
}



/**
 *  DESCRIPTION:    Determine the transmission mode and transmit files
 *
 *  ARGUMENTS:
 *
 *      args:       Parsed command line arguments
 *
 *      tx:         Initialized transmitter
 *
 */
void transmit(cli_args* args, dxwifi_transmitter* tx) {

    packet_loss_stats plstats = {
        .packet_loss_rate = args->packet_loss,
        .count = 0
    };

    if(args->tx_delay > 0 ) {
        attach_preinject_handler(transmitter, delay_transmission, &args->tx_delay);
    }
    if(args->tx.rtap_tx_flags & IEEE80211_RADIOTAP_F_TX_ORDER) {
        attach_preinject_handler(transmitter, attach_frame_number, NULL);
    }
    if(args->verbosity > DXWIFI_LOG_INFO ) {
        attach_postinject_handler(transmitter, log_frame_stats, NULL);
    }
    if(args->packet_loss > 0){
        attach_preinject_handler(transmitter, packet_loss_sim, &plstats);
    }
    if(args->error_rate > 0){
        attach_preinject_handler(transmitter, bit_error_rate_sim, &args->error_rate);
    }
    switch (args->tx_mode)
    {
    case TX_STREAM_MODE:
        setup_handlers_and_transmit(tx, STDIN_FILENO);
        break;

    case TX_FILE_MODE:
        transmit_files(tx, args->files, args->file_count, args->file_delay, args->retransmit_count, args->coderate);
        break;

    case TX_DIRECTORY_MODE:
        transmit_directory(args, tx);
        break;

    case TX_TEST_MODE:
        transmit_test_sequence(tx, args->retransmit_count);
        break;

    default:
        break;
    }

    if(plstats.count > 0){
        log_info("Number of packets dropped: %d", plstats.count);
    }
}
