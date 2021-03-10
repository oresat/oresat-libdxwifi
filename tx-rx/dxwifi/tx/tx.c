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
#include <stdarg.h>

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
#include <libdxwifi/details/logging.h>
#include <libdxwifi/details/dirwatch.h>


dirwatch* dirwatch_handle = NULL;
dxwifi_transmitter* transmitter = NULL;


// Forward declare
void transmit(cli_args* args, dxwifi_transmitter* tx);


int main(int argc, char** argv) {

    cli_args args = {
        .tx_mode                    = TX_STREAM_MODE,
        .verbosity                  = DXWIFI_LOG_INFO,
        .quiet                      = false,
        .file_count                 = 0,
        .file_filter                = "*",
        .transmit_current_files     = false,
        .listen_for_new_files       = true,
        .dirwatch_timeout           = -1,
        .tx_delay                   = 0,
        .file_delay                 = 0,
        .device                     = "mon0",

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

    transmit(&args, transmitter);

    close_transmitter(transmitter);

    exit(0);
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


/**
 *  DESCRIPTION:    Called everytime a frame is injected, logs stats about the 
 *                  transmitted frame.
 * 
 *  ARGUMENTS: 
 * 
 *      See definition of dxwifi_tx_frame_cb in transmitter.h
 * 
 */
size_t log_frame_stats(dxwifi_tx_frame* frame, size_t payload_size, dxwifi_tx_stats stats, void* user) {
    log_debug("Frame: %d - (Read: %ld, Sent: %ld)", stats.frame_count, stats.prev_bytes_read, stats.prev_bytes_sent);
    log_hexdump(frame->__frame, DXWIFI_TX_HEADER_SIZE + payload_size + IEEE80211_FCS_SIZE);
    return payload_size;
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
size_t delay_transmission(dxwifi_tx_frame* frame, size_t payload_size, dxwifi_tx_stats stats, void* user) {
    unsigned delay_ms = *(unsigned*) user;

    msleep(delay_ms, false);

    return payload_size;
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
size_t attach_frame_number(dxwifi_tx_frame* frame, size_t payload_size, dxwifi_tx_stats stats, void* user) {
    uint32_t* frame_no = (uint32_t*)&frame->mac_hdr->addr1[2];

    *frame_no = htonl(stats.frame_count);

    return payload_size;
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

    struct sigaction action, prev_action;

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
 *  RETURNS:
 *      
 *      dxwifi_tx_state_t: The last reported state of the transmitter
 * 
 */
dxwifi_tx_state_t transmit_files(dxwifi_transmitter* tx, char** files, size_t num_files, unsigned delay) {
    int fd = 0;
    dxwifi_tx_state_t state = DXWIFI_TX_NORMAL;

    for(size_t i = 0; i < num_files && state == DXWIFI_TX_NORMAL; ++i) {
        if((fd = open(files[i], O_RDONLY)) < 0) {
            log_error("Failed to open file: %s - %s", files[i], strerror(errno));
        }
        else {
            log_info("Opened %s for transmission", files[i]);
            state = setup_handlers_and_transmit(tx, fd);
            close(fd);
            msleep(delay, false);
        }
    }
    return state;
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
 */
void transmit_directory_contents(dxwifi_transmitter* tx, const char* filter, const char* dirname, unsigned delay) {
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
                    state = transmit_files(tx, &path_buffer, 1, delay);
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

    transmit_files(&args->tx, &path_buffer, 1, args->file_delay);

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
        transmit_directory_contents(tx, args->file_filter, dirname, args->file_delay);
    }
    if(args->listen_for_new_files) {

        dirwatch_handle = dirwatch_init();

        dirwatch_add(dirwatch_handle, dirname, args->file_filter, DW_CREATE_AND_CLOSE, true);

        // Setup handlers for exiting loop
        struct sigaction action, prev_action;
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

    if(args->tx_delay > 0 ) {
        attach_preinject_handler(transmitter, delay_transmission, &args->tx_delay);
    }
    if(args->tx.rtap_tx_flags & IEEE80211_RADIOTAP_F_TX_ORDER) {
        attach_preinject_handler(transmitter, attach_frame_number, NULL);
    }
    if(args->verbosity > DXWIFI_LOG_INFO ) {
        attach_postinject_handler(transmitter, log_frame_stats, NULL);
    }

    switch (args->tx_mode)
    {
    case TX_STREAM_MODE:
        setup_handlers_and_transmit(tx, STDIN_FILENO);
        break;

    case TX_FILE_MODE:
        transmit_files(tx, args->files, args->file_count, args->file_delay);
        break;

    case TX_DIRECTORY_MODE:
        transmit_directory(args, tx);
        break;
    
    default:
        break;
    }
}
