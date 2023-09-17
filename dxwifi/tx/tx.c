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
#include <inttypes.h>   // PRIu32, etc.
#include <unistd.h>
#include <signal.h>
#include <stdint.h>     // uint32_t, etc.
#include <dirent.h>
#include <fnmatch.h>

#include <arpa/inet.h>
#include <linux/limits.h>
#include <sys/mman.h>

#include <dxwifi/tx/cli.h>

#include <libdxwifi/dxwifi.h>
#include <libdxwifi/transmitter.h>
#include <libdxwifi/details/utils.h>
#include <libdxwifi/details/daemon.h>
#include <libdxwifi/details/logging.h>
#include <libdxwifi/details/dirwatch.h>
#include <libdxwifi/details/syslogger.h>

typedef struct {
    float packet_loss_rate;
    unsigned count;
} packet_loss_stats;

static dirwatch *dirwatch_handle = NULL;
static dxwifi_transmitter *transmitter = NULL;

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
    int frame_size = DXWIFI_TX_FRAME_SIZE;
    if(stats.frame_type == DXWIFI_CONTROL_FRAME_NONE) {
        log_debug("Frame: %d - (Read: %ld, Sent: %ld)", stats.data_frame_count, stats.prev_bytes_read, stats.prev_bytes_sent);
    }
    else {
        frame_size = DXWIFI_FRAME_CONTROL_SIZE + DXWIFI_TX_HEADER_SIZE + IEEE80211_FCS_SIZE;
        log_debug("%s Frame Sent: %d", control_frame_type_to_str(stats.frame_type), stats.prev_bytes_sent);
    }
    log_hexdump(frame, frame_size);
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

    int frame_size = DXWIFI_TX_FRAME_SIZE - sizeof(dxwifi_tx_radiotap_hdr);
    int total_num_errors = DXWIFI_TX_FRAME_SIZE * 8 * error_rate; //Get total number of errors
    uint8_t bit_array[frame_size]; //Make an array of bits equal to the number in the frame

    // Initialize every bit to zero
    memset(bit_array, 0, frame_size);

    // Skip the bits in the radiotap header since this is discarded pre-flight anyways
    uint8_t* buffer = ((uint8_t*)frame) + sizeof(dxwifi_tx_radiotap_hdr);

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
 *  DESCRIPTION:    Packs the current frame count into the last four bytes of
 *                  the MAC header's addr1 field.
 *
 *  ARGUMENTS:
 *
 *      frame       Data frame
 *
 *      stats       Stats for the current transmission
 *
 *      user_data   Ignored
 *
 *  RETURNS:
 *
 *      bool        True (indicates to transmit the packet)
 *
 *  NOTES:          This is an implementation of dxwifi_tx_frame_cb.
 *
 */
static bool
attach_frame_number_cb(dxwifi_tx_frame *frame, dxwifi_tx_stats stats,
                       void *user_data)
{
    uint32_t frame_number = htonl(stats.data_frame_count);

    // @TODO Fix magic number (assumes IEEE80211_MAC_ADDR_LEN defined as 6)
    memcpy(frame->mac_hdr.addr1 + 2, &frame_number, sizeof(uint32_t));

    return true;
}

/**
 *  DESCRIPTION:    Sets SIGINT handler, transmits a file, and restores original
 *                  handler
 *
 *  ARGUMENTS:
 *
 *      tx:         Initialized transmitter
 *
 *      fd:         Open file descriptor of the file to be transmitted
 *
 */
static dxwifi_tx_state_t
set_handler_and_transmit(dxwifi_transmitter *tx, int fd)
{
    dxwifi_tx_stats stats = { 0 };
    struct sigaction action = { 0 };
    struct sigaction prev_action = { 0 };

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
 *  DESCRIPTION:            Opens and transmits a file
 *
 *  ARGUMENTS:
 *
 *      tx:                 Initialized transmitter
 *
 *      file_path:          Path of file to transmit
 *
 *      delay:              Millisecond delay to add between file transmissions
 *
 *      retransmit_count:   Number of times to retransmit each matching file (-1
 *                          to retransmit until a timeout or error occurs)
 *
 *      code_rate:          Code rate for FEC encoding
 *
 *  RETURNS:
 *
 *      dxwifi_tx_state_t:  Last reported state of the transmitter
 *
 */
static dxwifi_tx_state_t
transmit_file(dxwifi_transmitter *tx, char *file_path, unsigned delay,
              int retransmit_count, float code_rate)
{
    const bool transmit_forever = (retransmit_count == -1);

    int fd = 0;
    void *file_data = NULL;
    off_t file_size = 0;

    void *encoded_data = NULL;
    size_t encoded_size = 0;

    dxwifi_tx_stats stats = { .tx_state = DXWIFI_TX_NORMAL };

    // Open file and map to memory
    file_size = get_file_size(file_path);
    if (file_size <= 0) {
        log_error("File %s intended for transmission is %s",
                  ((file_size < 0) ? "not a regular file" : "empty"),
                  file_path);
        goto done;
    }

    fd = open(file_path, O_RDONLY);
    if (fd < 0) {
        log_error("Failed to open file %s for transmission: %s",
                  files[i], strerror(errno));
        goto done;
    }

    file_data = mmap(NULL, file_size, PROT_READ, MAP_SHARED, fd, 0);
    if (file_data == MAP_FAILED) {
        log_error("Failed to map file %s to memory for transmission: %s",
                  file_path, sterror(errno));
        goto done;
    }

    // Encode and transmit file
    encoded_size = dxwifi_encode(file_data, file_size, code_rate,
                                 &encoded_data);
    if (encoded_size <= 0) {
        log_error("Unable to FEC-encode file %s", file_path);
    }

    log_info("Successfully encoded file %s (file size=%lld)",
             file_path, (long long) encoded_size);

    for (int count = 0;
         (transmit_forever || (count <= retransmit_count))
         && (stats.tx_state == DXWIFI_TX_NORMAL);
         count++) {

        transmit_bytes(tx, encoded_data, encoded_size, &stats);
        msleep(delay, false);
    }

done:
    if (fd >= 0) {
        close(fd);
        if ((file_data != NULL) && (file_data != MAP_FAILED)) {
            munmap(file_data, file_size);
        }
    }

    free(encoded_data);
    return stats.tx_state;
}

/**
 *  DESCRIPTION:            Iterates through a list of file names, opens them,
 *                          and transmits them
 *
 *  ARGUMENTS:
 *
 *      tx:                 Initialized transmitter
 *
 *      file_paths:         Paths of files to transmit
 *
 *      num_files:          Number of items in file_paths
 *
 *      delay:              Millisecond delay to add between file transmissions
 *
 *      retransmit_count:   Number of times to retransmit each matching file (-1
 *                          to retransmit until a timeout or error occurs)
 *
 *      code_rate:          Code rate for FEC encoding
 *
 *  RETURNS:
 *
 *      dxwifi_tx_state_t:  Last reported state of the transmitter
 *
 */
static dxwifi_tx_state_t
transmit_files(dxwifi_transmitter *tx, char **file_paths, size_t num_files,
               unsigned delay, int retransmit_count, float code_rate)
{
    dxwifi_tx_state_t state = DXWIFI_TX_NORMAL;

    for (size_t i = 0; (i < num_files) && (state == DXWIFI_TX_NORMAL); i++) {

        state = transmit_file(tx, file_paths[i], delay, retransmit_count,
                              code_rate);
    }
    return state;
}


/**
 *  DESCRIPTION:            Transmit all files in a directory that match a
 *                          filter
 *
 *  ARGUMENTS:
 *
 *      tx:                 Initialized transmitter
 *
 *      filter:             Glob pattern to filter which files should be
 *                          transmitted
 *
 *      dirname:            Name of target directory
 *
 *      delay:              Inter-file transmission delay in milliseconds
 *
 *      retransmit_count:   Number of times to retransmit each matching file (-1
 *                          to retransmit until a timeout or error occurs)
 *
 *      code_rate:          Code rate for FEC encoding
 *
 */
static void
transmit_directory_contents(dxwifi_transmitter *tx, const char *filter,
                            const char *dirname, unsigned delay,
                            int retransmit_count, float code_rate)
{
    DIR *dir = NULL;
    struct dirent *file = NULL;
    dxwifi_tx_state_t state = DXWIFI_TX_NORMAL;

    char path_buffer[PATH_MAX] = { 0 };

    dir = opendir(dirname);
    if (dir == NULL) {
        log_error("Failed to open directory %s for transmission: %s",
                  dirname, strerror(errno));
        return;
    }

    file = readdir(dir);
    while ((file != NULL) && (state == DXWIFI_TX_NORMAL)) {
        if (fnmatch(filter, file->d_name, 0) == 0) {
            combine_path(path_buffer, PATH_MAX, dirname, file->d_name);

            if (is_regular_file(path_buffer)) {
                state = transmit_file(tx, path_buffer, delay, retransmit_count,
                                      code_rate);
            }
        }
        file = readdir(dir);
    }
    closedir(dir);
}

/**
 *  DESCRIPTION:    Transmits newly created file (used as dirwatch callback)
 *
 *  ARGUMENTS:
 *
 *      event:      Creation and close event
 *
 *      user_data:  Command line arguments
 *
 */
static void
transmit_new_file(const dirwatch_event *event, void *user_data)
{
    cli_args *args = user_data;
    char *path_buffer[PATH_MAX] = { 0 };

    combine_path(path_buffer, PATH_MAX, event->dirname, event->filename);

    transmit_file(&args->tx, &path_buffer, args->file_delay,
                  args->retransmit_count, args->coderate);
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
static void
transmit_directory(cli_args *args, dxwifi_transmitter *tx)
{
    const char *dirname = args->files[0];

    if (args->transmit_current_files) {
        transmit_directory_contents(tx, args->file_filter, dirname,
                                    args->file_delay, args->retransmit_count,
                                    args->coderate);
    }

    if (args->listen_for_new_files) {
        struct sigaction action = { 0 };
        struct sigaction prev_action = { 0 };

        dirwatch_handle = dirwatch_init();
        dirwatch_add(dirwatch_handle, dirname, args->file_filter,
                     DW_CREATE_AND_CLOSE, true);

        // Set up handlers for exiting loop
        sigemptyset(&action.sa_mask);
        sigaddset(&action.sa_mask, SIGINT);
        action.sa_handler = watchdir_sigint_handler;
        sigaction(SIGINT, &action, &prev_action);

        dirwatch_listen(dirwatch_handle, (args->dirwatch_timeout * 1000),
                        transmit_new_file, args);

        sigaction(SIGINT, &prev_action, NULL);
        dirwatch_close(dirwatch_handle);
    }
}

/**
 *  DESCRIPTION:    Transmits a test sequence of bytes. The test sequence is a
 *                  10Kb block of data with the transmission count encoded in
 *                  every four bytes as the payload
 *
 *  ARGUMENTS:
 *
 *      tx:         Initialized transmitter
 *
 *      retransmit: Number of times to retransmit test sequence (-1 to
 *                  retransmit until a timeout or error occurs)
 *
 */
static void
transmit_test_sequence(dxwifi_transmitter *tx, int retransmit)
{
    const bool transmit_forever = (retransmit == -1);

    // @TODO Remove magic number
    uint32_t transmit_buffer[10240 / sizeof(uint32_t)];

    dxwifi_tx_stats stats = { 0 };

    log_info("Transmitting test sequence...");

    for (uint32_t count = 0; transmit_forever || (count <= retransmit);
         count++) {

        for (size_t i = 0; i < NELEMS(transmit_buffer); i++) {
            transmit_buffer[i] = count;
        }
        transmit_bytes(tx, transmit_buffer, 10240, &stats);
        log_tx_stats(stats);
    }
    log_info("Test sequence completed, transmitted %" PRIu32 " times", count);
}

/**
 *  DESCRIPTION:    Setup handlers and transmit data
 *
 *  ARGUMENTS:
 *
 *      args:       Parsed command line arguments
 *
 *      tx:         Initialized transmitter
 *
 */
static void
transmit(cli_args *args, dxwifi_transmitter *tx)
{
    packet_loss_stats plstats = {
        .packet_loss_rate = args->packet_loss,
        .count = 0
    };

    if (args->tx_delay > 0) {
        attach_preinject_handler(transmitter, delay_transmission,
                                 &args->tx_delay);
    }

    if ((args->tx.rtap_tx_flags & IEEE80211_RADIOTAP_F_TX_ORDER) != 0) {
        attach_preinject_handler(transmitter, attach_frame_number_cb, NULL);
    }

    if (args->packet_loss > 0) {
        attach_preinject_handler(transmitter, packet_loss_sim, &plstats);
    }

    if (args->error_rate > 0) {
        attach_preinject_handler(transmitter, bit_error_rate_sim,
                                 &args->error_rate);
    }

    if (args->verbosity > DXWIFI_LOG_INFO) {
        attach_postinject_handler(transmitter, log_frame_stats, NULL);
    }

    switch (args->tx_mode) {
        case TX_STREAM_MODE:
            set_handler_and_transmit(tx, STDIN_FILENO);
            break;

        case TX_FILE_MODE:
            transmit_files(tx, args->files, args->file_count, args->file_delay,
                           args->retransmit_count, args->coderate);
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

    if (plstats.count > 0) {
        log_info("Number of packets dropped: %d", plstats.count);
    }
}

int
main(int argc, char **argv)
{
    cli_args args = DEFAULT_CLI_ARGS;
    transmitter = &args.tx;
#if defined(DXWIFI_TESTS)
    unsigned seed = 1621981756;
#else
    unsigned seed = time(0);
#endif

    parse_args(argc, argv, &args);

    set_log_level(DXWIFI_LOG_ALL_MODULES, args.verbosity);

    if (args.use_syslog) {
        set_logger(DXWIFI_LOG_ALL_MODULES, syslogger);
    }

    if (args.daemon != DAEMON_UNKNOWN_CMD) {
        // @TODO Do we really need to proceed if we're stopping the daemon?
        daemon_run(args.pid_file, args.daemon);
        signal(SIGTERM, terminate);
    }

    srand(seed);

    init_transmitter(transmitter, args.device);
    transmit(&args, transmitter);
    close_transmitter(transmitter);

    if (args.daemon == DAEMON_START) {
        // Tear down before exit
        stop_daemon(args.pid_file);
    }

    exit(0);
}
