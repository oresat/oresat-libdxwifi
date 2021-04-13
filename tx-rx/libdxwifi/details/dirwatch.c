/**
 *  dirwatch.c
 *  
 *  DESCRIPTION: See dirwatch.h for description
 * 
 *  https://github.com/oresat/oresat-dxwifi-software
 * 
 */


#include <poll.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fnmatch.h>


#include <libdxwifi/details/assert.h>
#include <libdxwifi/details/logging.h>
#include <libdxwifi/details/dirwatch.h>


// TODO all the find operations just do a linear search, which is obviously suboptimal

// Buffer can hold at a minimum, 32 events
#define EVENT_BUFFSIZE ((sizeof(struct inotify_event) + NAME_MAX + 1) * 32)

static const int EMPTY_NODE = 0;


typedef struct {
    int wd;                     /* Watch Descriptor handle                  */

    char* dirname;              /* Name of directory where event occured    */

    char* file_filter;          /* Glob pattern to filter file events       */

    char* watchfiles[DIRWATCH_MAX];
                                /* Name of each file to watch within the dir*/
} watchdir;


struct __dirwatch {
    struct pollfd handle;       /* Pollable inotify handle                  */

    watchdir watchdirs[DIRWATCH_MAX];
                                /* Directory watchlist                      */

    volatile bool listen;       /* Loop variable flag                       */
};


// Filtering expression for searching watch lists
typedef bool (*filter)(const watchdir* node, const void* val);


/**
 *  DESCRIPTION:    Finds the first watchdir that matches the filter
 * 
 *  ARGUMENTS:
 *      dw:             Allocated dirwatch object
 * 
 *      target:         Value to compare against
 * 
 *      match:          Filter expression
 * 
 *  RETURNS:
 * 
 *      watch_node*:    Pointer to the matched watch node or NULL
 * 
 */
static watchdir* find_watchdir(dirwatch* dw, const void* target, filter match) {
    debug_assert(dw && target);

    for(size_t i = 0; i < DIRWATCH_MAX; ++i) {
        if(match(&dw->watchdirs[i], target)) {
            return &dw->watchdirs[i];
        }
    }
    return NULL;
}


/**
 *  DESCRIPTION:    Finds the integer index to the first watchdir that matches the filter
 * 
 *  ARGUMENTS:
 *      dw:             Allocated dirwatch handle
 * 
 *      target:         Value to compare against
 * 
 *      match:          Filter expression
 * 
 *  RETURNS:
 * 
 *      int:            Index to the matched watch node or -1
 * 
 */
static int get_watchdir_idx(dirwatch* dw, const void* target, filter match) {
    debug_assert(dw && target);

    for(size_t i = 0; i < DIRWATCH_MAX; ++i) {
        if(match(&dw->watchdirs[i], target)) {
            return i;
        }
    }
    return -1;
}

/**
 *  DESCRIPTION:        Filters by watch descriptor
 * 
 *  ARGUMENTS:
 * 
 *      watch:          Watchdir to be tested
 * 
 *      val:            Target watch descriptor value
 * 
 *  RETURNS:
 * 
 *      bool:           True if the watch descriptors match
 * 
 */
static bool find_by_wd(const watchdir* watch, const void* val) {
    debug_assert(watch && val);

    int wd = *(const int*)val;
    return watch->wd == wd;
}


/**
 *  DESCRIPTION:        Filters by directory name
 * 
 *  ARGUMENTS:
 * 
 *      watch:          Watchdir to be tested
 * 
 *      val:            Target directory name
 * 
 *  RETURNS:
 *      bool:           True if the directory names match
 * 
 */
static bool find_by_dirname(const watchdir* watch, const void* val) {
    debug_assert(watch && val);

    if(watch->dirname) {
        return strcmp(watch->dirname, (const char*)val) == 0;
    }
    return false;
}


/**
 *  DESCRIPTION:    Converts a dirwatch event bitmask to the correct inotify bitmask
 * 
 *  ARGUMENTS:
 * 
 *      events:     Dirwatch event bitmask
 * 
 *  RETURNS:
 * 
 *      int:        Inotify event bitmask
 * 
 */
static int get_inotify_mask(dirwatch_events_t events) {
    int mask = 0x00;
    if(events & DW_CREATE_AND_CLOSE) {
        mask |= IN_CREATE | IN_CLOSE_WRITE;
    }
    return mask;
}

//
// See dirwatch.h for non-static function descriptions
//

dirwatch* dirwatch_init() {
    dirwatch* dw = calloc(1, sizeof(dirwatch));
    assert_M(dw, "Calloc failed: %s", strerror(errno));

    dw->handle.fd = inotify_init1(IN_NONBLOCK);
    assert_M(dw->handle.fd > 0, "Failed to initialize inotify: %s", strerror(errno));

    dw->handle.events = POLLIN;

    return dw;
}


void dirwatch_close(dirwatch* dw) {
    debug_assert(dw);
    if(dw) {
        for(int i = 0; i < DIRWATCH_MAX; ++i) {
            dirwatch_remove(dw, i);
        }

        close(dw->handle.fd);

        free(dw);
    }
}


int dirwatch_add(dirwatch* dw, const char* dirname, const char* file_filter, dirwatch_events_t events, bool clobber) {
    debug_assert(dw && dw->handle.fd);

    int mask = get_inotify_mask(events) | (clobber ? 0 : IN_MASK_ADD);

    // Check if node already exists, update it if it does
    int idx = get_watchdir_idx(dw, dirname, find_by_dirname);
    if(idx >= 0) { 
        dw->watchdirs[idx].wd = inotify_add_watch(dw->handle.fd, dirname, mask);

        if(clobber) {
            free(dw->watchdirs[idx].file_filter); // Free old filter
            dw->watchdirs[idx].file_filter = strdup(file_filter);
        }
    }
    else 
    {
        // Add watch to empty node
        idx = get_watchdir_idx(dw, &EMPTY_NODE, find_by_wd);
        if(idx >= 0) {
            dw->watchdirs[idx].wd = inotify_add_watch(dw->handle.fd, dirname, mask);
            dw->watchdirs[idx].dirname = strdup(dirname);
            dw->watchdirs[idx].file_filter = strdup(file_filter);
        }
    }

    return idx;
}



bool dirwatch_remove(dirwatch* dw, unsigned index) {
    debug_assert(dw);

    if(dw && index < DIRWATCH_MAX) {
        if(dw->watchdirs[index].wd > 0) {
            inotify_rm_watch(dw->handle.fd, dw->watchdirs[index].wd);
            free(dw->watchdirs[index].dirname);
            free(dw->watchdirs[index].file_filter);

            // Free any associated watch files
            for(size_t i = 0; i < DIRWATCH_MAX; ++i) {
                if(dw->watchdirs[index].watchfiles[i]) {
                    free(dw->watchdirs[index].watchfiles[i]);
                }
            }

            dw->watchdirs[index].wd = 0;
            return true;
        }
    }
    return false;
}


void dirwatch_listen(dirwatch* dw, int timeout_ms, dirwatch_event_handler handler, void* user) {
    debug_assert(dw && handler);

    dirwatch_event dw_event;
    char* path_buffer = calloc(PATH_MAX, sizeof(char));

    ssize_t next = 0;
    ssize_t nbytes = 0;

    //https://man7.org/linux/man-pages/man7/inotify.7.html - See example section
    uint8_t event_buffer[EVENT_BUFFSIZE] 
        __attribute__((aligned(__alignof__(struct inotify_event))));

    log_info("Dirwatch activated");
    dw->listen = true;
    while(dw->listen) {
        int status = poll(&dw->handle, 1, timeout_ms);
        if(status == 0) {
            log_info("Dirwatch timeout occured");
            dw->listen = false;
        }
        else if(status < 0) {
            if(dw->listen) {
                log_error("Error occured: %s", strerror(errno));
            }
        }
        else {// Events have occured, read them into the buffer
            nbytes = read(dw->handle.fd, event_buffer, EVENT_BUFFSIZE);

            next = 0;
            while(next < nbytes) { // Process all events
                struct inotify_event* event = (struct inotify_event*) &event_buffer[next];

                // New file was created, watch for file close
                if((event->mask & IN_CREATE) && !(event->mask & IN_ISDIR)) {

                    watchdir* dir = find_watchdir(dw, &event->wd, find_by_wd);

                    // Cache the filename if it matches the filter
                    if(dir && fnmatch(dir->file_filter, event->name, 0) == 0) {
                        for(int i = 0; i < DIRWATCH_MAX; ++i) { // Find an empty watchfile
                            if(dir->watchfiles[i] == NULL){
                                dir->watchfiles[i] = strdup(event->name);
                            }
                        }
                    }
                }
                // File was closed, check if we were watching it
                if (event->mask & IN_CLOSE_WRITE) {

                    watchdir* dir = find_watchdir(dw, &event->wd, find_by_wd);

                    if(dir) {
                        bool found = false;
                        for(int i = 0; i < DIRWATCH_MAX && !found; ++i) {
                            // TODO: strcmp in linear search is sub-optimal at best
                            if(strcmp(event->name, dir->watchfiles[i]) == 0) {
                                dw_event.event    = DW_CREATE_AND_CLOSE;
                                dw_event.dirname  = dir->dirname;
                                dw_event.filename = dir->watchfiles[i];

                                handler(&dw_event, user);

                                free(dir->watchfiles[i]);
                                dir->watchfiles[i] = NULL;
                                found = true;
                            }
                        }
                    }
                }
                next += sizeof(struct inotify_event) + event->len;
            }
        }
    }

    free(path_buffer);

    log_info("DirWatch deactivated");
}


void dirwatch_stop(dirwatch* dw) {
    if(dw) {
        dw->listen = false;
    }
}

