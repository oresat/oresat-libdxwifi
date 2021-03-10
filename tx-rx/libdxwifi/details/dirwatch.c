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

#include <linux/limits.h>

#include <libdxwifi/details/assert.h>
#include <libdxwifi/details/logging.h>
#include <libdxwifi/details/dirwatch.h>

// Buffer can hold at a minimum, 32 events
#define EVENT_BUFFSIZE ((sizeof(struct inotify_event) + NAME_MAX + 1) * 32)

static const int EMPTY_NODE = 0;


typedef struct {
    int wd;                     /* Watch Descriptor handle                  */

    char* dirname;              /* Name of directory where event occured    */

    union {
        char* file_filter;      /* Filter is used for watch directories    */
        char* filename;         /* Name of file for which the event occured*/
    } tag;

} watch_node;


struct __dirwatch {
    struct pollfd handle;       /* Pollable inotify handle                  */

    watch_node watchdirs[DIRWATCH_MAX];
                                /* Directory watchlist                      */
    watch_node watchfiles[DIRWATCH_MAX];
                                /* File watchlist                           */

    volatile bool listen;
};


// Filtering expression for searching watch lists
typedef bool (*filter)(const watch_node* node, const void* val);


/**
 *  DESCRIPTION:    Finds the first watchnode that matches the filter
 * 
 *  ARGUMENTS:
 *      watch_node:     The watchlist
 * 
 *      n:              Number of elements in the watchlist
 * 
 *      target:         Value to compare against
 * 
 *      match:          Filter expression
 * 
 *  RETURNS:
 *      watch_node*:    Pointer to the matched watch node or NULL
 * 
 */
static watch_node* find_node(watch_node* nodes, size_t n, const void* target, filter match) {
    debug_assert(nodes && target);

    for(size_t i = 0; i < n; ++i) {
        if(match(&nodes[i], target)) {
            return &nodes[i];
        }
    }
    return NULL;
}


/**
 *  DESCRIPTION:    Finds the integer index to the first watchnode that matches the filter
 * 
 *  ARGUMENTS:
 *      watch_node:     The watchlist
 * 
 *      n:              Number of elements in the watchlist
 * 
 *      target:         Value to compare against
 * 
 *      match:          Filter expression
 * 
 *  RETURNS:
 *      int:            Index to the matched watch node or -1
 * 
 */
static int get_watchnode_idx(watch_node* nodes, size_t n, const void* target, filter match) {
    debug_assert(nodes && target);

    for(size_t i = 0; i < n; ++i) {
        if(match(&nodes[i], target)) {
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
 *      watch_node:     Node in question
 * 
 *      val:            Target watch descriptor value
 * 
 *  RETURNS:
 *      bool:           True if the watch descriptors match
 * 
 */
static bool find_by_wd(const watch_node* node, const void* val) {
    debug_assert(node && val);

    int wd = *(const int*)val;
    return node->wd == wd;
}


/**
 *  DESCRIPTION:        Filters by directory name
 * 
 *  ARGUMENTS:
 * 
 *      watch_node:     Node in question
 * 
 *      val:            Target directory name
 * 
 *  RETURNS:
 *      bool:           True if the directory names match
 * 
 */
static bool find_by_dirname(const watch_node* node, const void* val) {
    debug_assert(node && val);

    if(node->dirname) {
        return strcmp(node->dirname, (const char*)val) == 0;
    }
    return false;
}


/**
 *  DESCRIPTION:        Removes a node from a watchlist
 * 
 *  ARGUMENTS:
 * 
 *      inotify_handle: File descriptor to inotify instance
 * 
 *      nodes:          The watchlist
 * 
 *      n:              Number of elements in the watchlist
 * 
 *      index:          Index to the node to remove
 * 
 *  RETURNS:
 *      bool:           True if the node was successfully removed
 * 
 */
static bool remove_node(int inotify_handle, watch_node* nodes, size_t n, unsigned index) {
    debug_assert(nodes);

    if(index < n) {
        if(nodes[index].wd > 0) {
            inotify_rm_watch(inotify_handle, nodes[index].wd);
            free(nodes[index].tag.filename);
            free(nodes[index].dirname);
            nodes[index].wd = 0;
            return true;
        }
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
        mask |= IN_CREATE;
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
            remove_node(dw->handle.fd, dw->watchdirs, DIRWATCH_MAX, i);
            remove_node(dw->handle.fd, dw->watchfiles, DIRWATCH_MAX, i);
        }

        close(dw->handle.fd);

        free(dw);
    }
}


int dirwatch_add(dirwatch* dw, const char* dirname, const char* file_filter, dirwatch_events_t events, bool clobber) {
    debug_assert(dw && dw->handle.fd);

    int mask = get_inotify_mask(events) | (clobber ? 0 : IN_MASK_ADD);

    // Check if node already exists, update it if it does
    int idx = get_watchnode_idx(dw->watchdirs, DIRWATCH_MAX, dirname, find_by_dirname);
    if(idx >= 0) { 
        dw->watchdirs[idx].wd = inotify_add_watch(dw->handle.fd, dirname, mask);

        if(clobber) {
            free(dw->watchdirs[idx].tag.file_filter); // Free old filter
            dw->watchdirs[idx].tag.file_filter = strdup(file_filter);
        }
    }
    else 
    {
        // Add watch to empty node
        idx = get_watchnode_idx(dw->watchdirs, DIRWATCH_MAX, &EMPTY_NODE, find_by_wd);
        if(idx >= 0) {
            dw->watchdirs[idx].wd = inotify_add_watch(dw->handle.fd, dirname, mask);
            dw->watchdirs[idx].dirname = strdup(dirname);
            dw->watchdirs[idx].tag.file_filter = strdup(file_filter);
        }
    }

    return idx;
}



bool dirwatch_remove(dirwatch* dw, unsigned index) {
    debug_assert(dw);

    if(dw) {
        return remove_node(dw->handle.fd, dw->watchdirs, DIRWATCH_MAX, index);
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

    log_info("DirWatch activated");
    dw->listen = true;
    while(dw->listen) {
        int status = poll(&dw->handle, 1, timeout_ms);
        if(status == 0) {
            log_info("DirWatch timeout occured");
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
                    watch_node* watchdir = find_node(dw->watchdirs, DIRWATCH_MAX, &event->wd, find_by_wd);
                    if(watchdir && fnmatch(watchdir->tag.file_filter, event->name, 0) == 0) {
                        watch_node* watchfile = find_node(dw->watchfiles, DIRWATCH_MAX, &EMPTY_NODE, find_by_wd);
                        if(watchfile) { 
                            combine_path(path_buffer, PATH_MAX, watchdir->dirname, event->name);
                            watchfile->wd = inotify_add_watch(dw->handle.fd, path_buffer, IN_CLOSE_WRITE);
                            watchfile->dirname = strdup(watchdir->dirname);
                            watchfile->tag.filename = strdup(event->name);
                        }
                    }
                }
                // File was closed, check if we were watching it
                else if (event->mask & IN_CLOSE_WRITE) {
                    int idx = get_watchnode_idx(dw->watchfiles, DIRWATCH_MAX, &event->wd, find_by_wd);
                    if(idx >= 0) {
                        dw_event.event      = DW_CREATE_AND_CLOSE;
                        dw_event.dirname    = dw->watchfiles[idx].dirname;
                        dw_event.filename   = dw->watchfiles[idx].tag.filename;

                        handler(&dw_event, user);

                        remove_node(dw->handle.fd, dw->watchfiles, DIRWATCH_MAX, idx);
                    }
                }
                next += sizeof(struct inotify_event) + event->len;
            }
        }
    }

    // Check for any leftover files
    for(size_t i = 0; i < DIRWATCH_MAX; ++i) {
        if(dw->watchfiles[i].wd != 0) {
            dw_event.event    = DW_LEFTOVER_FILE;
            dw_event.dirname  = dw->watchfiles[i].dirname;
            dw_event.filename = dw->watchfiles[i].tag.filename;

            handler(&dw_event, user);

            remove_node(dw->handle.fd, dw->watchfiles, DIRWATCH_MAX, i);
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

