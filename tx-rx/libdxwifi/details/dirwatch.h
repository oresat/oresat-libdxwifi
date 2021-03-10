/**
 *  dirwatch.h
 *  
 *  DESCRIPTION: Simplified API for monitoring a directory for events
 * 
 *  https://github.com/oresat/oresat-dxwifi-software
 * 
 *  NOTES: Currently only supports tracking a directory for new files but can
 *  easily be modified to listen for a number of other events
 * 
 */


#ifndef LIBDXWIFI_DIRWATCH_H
#define LIBDXWIFI_DIRWATCH_H

#include <sys/inotify.h>


// Maximum number of directories to watch concurrently
#define DIRWATCH_MAX 256 


typedef enum {
    // File was created and then closed
    DW_CREATE_AND_CLOSE = 0x00000001,

    // File was subscribed to, but no events occured. This can occur if the 
    // File was created and closed before we could finish subscribing to its 
    // events.
    DW_LEFTOVER_FILE    = 0x00000002
} dirwatch_events_t;


/**
 *  Events structure contains a bitmask describing the event as well as the 
 *  name of the directory where the event occured and the name of the file. 
 *  These strings are owned by dirwatch and have limited scope. They must be
 *  copied if you need to access them later. 
 */
typedef struct {
    dirwatch_events_t   event;
    const char*         dirname;
    const char*         filename;
} dirwatch_event;

/**
 *  Event handler is called everytime there is an event that was subscribed to
 *  occurs. The event memory is owned by dirwatch and must be copied by the user
 *  if it needs to be referenced later. 
 */
typedef void (*dirwatch_event_handler)(const dirwatch_event* event, void* user);


// Implementation in dirwatch.c
typedef struct __dirwatch dirwatch;


/**
 *  DESCRIPTION:    Initializes a dirwatch object
 * 
 *  RETURNS:
 * 
 *      dirwatch*:  Allocated dirwatch handle. Must not be freed by the user. 
 *                  User dirwatch_close() to teardown dirwatch resources
 * 
 */
dirwatch* dirwatch_init();


/**
 *  DESCRIPTION:    Tearsdown any resources associated with the dirwatch handle
 * 
 *  ARGUMENTS:
 * 
 *      dw:         Allocated dirwatch handle, see dirwatch_init()
 * 
 */
void dirwatch_close(dirwatch* dw);


/**
 *  DESCRIPTION:    Adds a directory to the watchlist
 * 
 *  ARGUMENTS:
 * 
 *      dw:             Allocated dirwatch handle, see dirwatch_init()
 * 
 *      dirname:        Path to the directory to watch
 * 
 *      file_filter:    Glob pattern to filter filenames against. Only events 
 *                      for files that match the glob will be processed. Use 
 *                      "*" to process all files.
 * 
 *      events:         Bitmask of events to subscribe to
 * 
 *      clobber:        Clobber events and file_filter if the dirname is already
 *                      being watched?
 * 
 *  RETURNS:
 *      
 *      int:            Index reference to watched directory
 * 
 */
int dirwatch_add(dirwatch* dw, const char* dirname, const char* file_filter, dirwatch_events_t events, bool clobber);


/**
 *  DESCRIPTION:    Removes a directory from the watchlist
 * 
 *  ARGUMENTS:
 * 
 *      dw:         Allocated dirwatch handle, see dirwatch_init()
 * 
 *      index:      Index reference to watch directory, see dirwatch_add()
 * 
 *  RETURNS:
 *      
 *      bool:       True if directory was successfully removed from the watchlist
 * 
 */
bool dirwatch_remove(dirwatch* dw, unsigned index);


/**
 *  DESCRIPTION:    Initiates the listener loop and processes dirwatch events
 * 
 *  ARGUMENTS:
 * 
 *      dw:         Allocated dirwatch handle, see dirwatch_init()
 * 
 *      timeout_ms: Number of milliseconds to wait for an event to occur. A
 *                  negative value denotes no timeout.
 * 
 *      handler:    Callback to process each event
 * 
 *      user:       User arguments to forward to the handler
 * 
 *  NOTES: Unless a positive timeout value is specified, this function is 
 *  divergent and will block until an event occurs. Install a signal handler to
 *  call dirwatch_stop() if this behavior is not desired.
 *  
 */
void dirwatch_listen(dirwatch* dw, int timeout_ms, dirwatch_event_handler handler, void* user);


/**
 *  DESCRIPTION:    Signals to dirwatch to stop listening for events
 * 
 *  ARGUMENTS:
 *      dw:         Allocated dirwatch handle, see dirwatch_init()
 * 
 *  NOTES: There are no guarantees that no more events will be process. At most, 
 *  one more buffer of events may be processed.
 * 
 */
void dirwatch_stop(dirwatch* dw);

#endif // LIBDXWIFI_DIRWATCH_H
