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


volatile bool listen_for_files = false;
dxwifi_transmitter* transmitter = NULL;


void transmit(cli_args* args, dxwifi_transmitter* tx);


int main(int argc, char** argv) {

    cli_args args = {
        .tx_mode                    = TX_STREAM_MODE,
        .verbosity                  = DXWIFI_LOG_INFO,
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


void sigint_handler(int signum) {
    signal(SIGINT, SIG_IGN);
    listen_for_files = false;
    stop_transmission(transmitter);
    signal(SIGINT, sigint_handler);
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
    unsigned delay_ms = *(unsigned*) user;

    msleep(delay_ms, false);

    return payload_size;
}


size_t attach_frame_number(dxwifi_tx_frame* frame, size_t payload_size, dxwifi_tx_stats stats, void* user) {
    uint32_t* frame_no = (uint32_t*)&frame->mac_hdr->addr1[2];

    *frame_no = htonl(stats.frame_count);

    return payload_size;
}


dxwifi_tx_state_t setup_handlers_and_transmit(dxwifi_transmitter* tx, int fd) {
    dxwifi_tx_stats stats;

    signal(SIGINT, sigint_handler);
    start_transmission(tx, fd, &stats);
    signal(SIGINT, SIG_DFL);

    log_tx_stats(stats);
    return stats.tx_state;
}


void transmit_files(dxwifi_transmitter* tx, char** files, size_t num_files, unsigned delay) {
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
}


void transmit_directory_contents(dxwifi_transmitter* tx, const char* filter, char* dirname, unsigned delay) {
    DIR* dir;
    struct dirent* file;
    char* path_buffer = calloc(PATH_MAX, sizeof(char));

    if((dir = opendir(dirname)) == NULL) {
        log_error("Failed to open directory: %s - %s", dirname, strerror(errno));
    }
    else {
        while(file = readdir(dir)) {
            if(fnmatch(filter, file->d_name, 0) == 0) {
                sprintf(path_buffer, "%s/%s", dirname, file->d_name);
                if(is_regular_file(path_buffer)) {
                    transmit_files(tx, &path_buffer, 1, delay);
                }
            }
        }
        closedir(dir);
    }
    free(path_buffer);
}


void transmit_directories(cli_args* args, dxwifi_transmitter* tx) {
    typedef struct {
        int fd;
        char* path;
    } watch_node;

    if(args->transmit_current_files) {
        for(int i = 0; i < args->file_count; ++i) {
            transmit_directory_contents(tx, args->file_filter, args->files[i], args->file_delay);
        }
    }
    if(args->listen_for_new_files) {

    // TODO this is the ugliest most profane block of code I have ever written. 
    // It needs to be burned with fire and refactored into something legible.
    // But it works. so . . .¯\_(ツ)_/¯
        int status = 0;
        const int watchfile_sz = 1024;
        watch_node watchdirs[args->file_count];
        watch_node watchfiles[watchfile_sz];

        memset(watchdirs, 0x00, args->file_count * sizeof(watch_node));

        for(int i = 0; i < watchfile_sz; ++i) {
            watchfiles[i].fd = 0;
            watchfiles[i].path = calloc(PATH_MAX, sizeof(char));
        }

        size_t buffsize = (sizeof(struct inotify_event)  + NAME_MAX + 1) * 16;
        char buffer[buffsize]; // Buffer can process 16 events at a time

        struct pollfd handle = {
            .fd         = inotify_init1(IN_NONBLOCK),
            .events     = POLLIN,
            .revents    = 0
        };
        assert_M(handle.fd > 0, "Failed to initialize inotify: %s", strerror(errno));

        for(int i = 0; i < args->file_count; ++i) { // Add all directories to watchlist
            if((watchdirs[i].fd = inotify_add_watch(handle.fd, args->files[i], IN_CREATE)) > 0) {
                watchdirs[i].path = args->files[i];
            }
            else {
                log_error("Failed to add %s to watchlist: %s", args->files[i], strerror(errno));
            }
        }

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
                        bool watch_added = false;
                        if(fnmatch(args->file_filter, event->name, 0) == 0) {
                            for(int i = 0; i < watchfile_sz && !watch_added; ++i) {
                                if(watchfiles[i].fd == 0) {
                                    for(int j = 0; j < args->file_count; ++j) {
                                        if(watchdirs[j].fd == event->wd) {
                                            sprintf(watchfiles[i].path, "%s/%s", watchdirs[j].path, event->name);
                                        }
                                    }
                                    watchfiles[i].fd = inotify_add_watch(handle.fd, watchfiles[i].path, IN_CLOSE_WRITE);
                                    assert_M(watchfiles[i].fd > 0, "Failed to add file to watchlist: %s", strerror(errno));
                                    watch_added = true;
                                }
                            }
                            if(!watch_added) {
                                log_error("Watch file list is full");
                            }
                        }
                    }
                    // File was closed, check if we were watching it
                    else if(event->mask & IN_CLOSE_WRITE) {
                        bool found = false;
                        for(int i = 0; i < watchfile_sz && !found; ++i) {
                            if(watchfiles[i].fd == event->wd) {
                                transmit_files(tx, &watchfiles[i].path, 1, args->file_delay);
                                inotify_rm_watch(handle.fd, watchfiles[i].fd);
                                watchfiles[i].fd = 0;
                                found = true;
                            }
                        }
                    }
                    pos += sizeof(struct inotify_event) + event->len;
                }
            }
        } while(listen_for_files);

        log_info("Stopped listening for files");

        for(int i = 0; i < watchfile_sz; ++i) {
            free(watchfiles[i].path);
        }
        close(handle.fd);
    }
}


void transmit(cli_args* args, dxwifi_transmitter* tx) {
    int fd = 0;
    dxwifi_tx_stats tx_stats = {
        .tx_state = DXWIFI_TX_NORMAL
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

    switch (args->tx_mode)
    {
    case TX_STREAM_MODE:
        setup_handlers_and_transmit(tx, STDIN_FILENO);
        break;

    case TX_FILE_MODE:
        transmit_files(tx, args->files, args->file_count, args->file_delay);
        break;

    case TX_DIRECTORY_MODE:
        transmit_directories(args, tx);
        break;
    
    default:
        break;
    }
}


