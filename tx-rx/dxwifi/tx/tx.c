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
#include <sys/inotify.h>
#include <linux/limits.h>

#include <dxwifi/tx/cli.h>

#include <libdxwifi/dxwifi.h>
#include <libdxwifi/transmitter.h>
#include <libdxwifi/details/utils.h>
#include <libdxwifi/details/assert.h>
#include <libdxwifi/details/logging.h>


volatile bool listen_for_files  = false;
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
        .watchdir_timeout           = -1,
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
    listen_for_files = false;
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

        const int NUM_LISTENERS = 256;

        int status = 0;
        int watchdir = 0;
        int watchfiles[NUM_LISTENERS];
        char* path_buffer = calloc(PATH_MAX, sizeof(char));

        memset(watchfiles, 0x00, NUM_LISTENERS * sizeof(int));

        size_t buffsize = (sizeof(struct inotify_event)  + NAME_MAX + 1) * 16;
        char buffer[buffsize]; // Buffer can process 16 events at a time

        struct pollfd handle = {
            .fd         = inotify_init1(IN_NONBLOCK),
            .events     = POLLIN,
            .revents    = 0
        };
        assert_M(handle.fd > 0, "Failed to initialize inotify: %s", strerror(errno));

        watchdir = inotify_add_watch(handle.fd, dirname, IN_CREATE);
        assert_M(watchdir > 0, "Failed to add %s to watchlist: %s", dirname, strerror(errno));

        // Setup handlers for exiting loop
        struct sigaction action, prev_action;
        sigemptyset(&action.sa_mask);
        sigaddset(&action.sa_mask, SIGINT);
        action.sa_handler = watchdir_sigint_handler;
        sigaction(SIGINT, &action, &prev_action);

        log_info("Listening for new files in %s", dirname);
        listen_for_files = true;
        do {
            status = poll(&handle, 1, args->watchdir_timeout * 1000);
            if(status == 0) {
                log_info("Watch Directory timeout occured");
                listen_for_files = false;
            }
            else { // Events have occured, read them into the buffer
                status = read(handle.fd, buffer, buffsize);

                int pos = 0;
                while (pos < status) { // Process all events
                    struct inotify_event* event = (struct inotify_event*) &buffer[pos];

                    // New file was created, watch for file close
                    if((event->mask & IN_CREATE) && !(event->mask & IN_ISDIR)) {
                        if(fnmatch(args->file_filter, event->name, 0) == 0) {
                            int idx = get_index(watchfiles, NUM_LISTENERS, 0);
                            if( idx < 0) {
                                log_error("Watchfile list is full");
                            }
                            else {
                                combine_path(path_buffer, PATH_MAX, dirname, event->name);
                                watchfiles[idx] = inotify_add_watch(handle.fd, path_buffer, IN_CLOSE_WRITE);
                            }
                        }
                    }
                    // File was closed, check if we were watching it
                    else if(event->mask & IN_CLOSE_WRITE) {
                        int idx = get_index(watchfiles, NUM_LISTENERS, event->wd);
                        if(idx >= 0) {
                            combine_path(path_buffer, PATH_MAX, dirname, event->name);
                            transmit_files(tx, &path_buffer, 1, args->file_delay);
                            inotify_rm_watch(handle.fd, watchfiles[idx]);
                            watchfiles[idx] = 0;
                        }
                    }
                    pos += sizeof(struct inotify_event) + event->len;
                }
            }
        } while(listen_for_files);
        sigaction(SIGINT, &prev_action, NULL);

        free(path_buffer);

        log_info("Stopped listening for files");

        close(handle.fd);
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
