/**
 *  cli.h
 *  
 *  DESCRIPTION: Command line interface for tx.c
 * 
 *  https://github.com/oresat/oresat-dxwifi-software
 * 
 */


#include <libdxwifi/transmitter.h>
#include <libdxwifi/details/daemon.h>


#define TX_DEFAULT_PID_FILE "/run/oresat-live-txd.pid"

// TODO this is defined arbitrarily, is there an upper limit to the number of 
// Files to transmit at a time? 
#define TX_CLI_FILE_MAX 1024


typedef enum {
    TX_TEST_MODE,       // Sanity check, transmit a test sequence of bytes
    TX_FILE_MODE,       // Transmit a file or list of files
    TX_STREAM_MODE,     // Transmit all data from stdin
    TX_DIRECTORY_MODE,  // Transmit contents of a directory
} tx_mode_t;


typedef struct {
    tx_mode_t           tx_mode;
    bool                daemon;
    dxwifi_daemon_cmd_t daemon_cmd;
    const char*         pid_file;
    char*               files[TX_CLI_FILE_MAX];
    int                 file_count;
    const char*         file_filter;
    int                 retransmit_count;
    bool                transmit_current_files;
    bool                listen_for_new_files;
    int                 dirwatch_timeout; 
    int                 verbosity;
    bool                quiet;
    bool                use_syslog;
    unsigned            tx_delay;
    unsigned            file_delay;
    const char*         device;
    dxwifi_transmitter  tx;
} cli_args;


#define DEFAULT_CLI_ARGS  {\
        .tx_mode                    = TX_STREAM_MODE,\
        .daemon                     = false,\
        .daemon_cmd                 = DAEMON_START,\
        .pid_file                   = TX_DEFAULT_PID_FILE,\
        .verbosity                  = DXWIFI_LOG_INFO,\
        .quiet                      = false,\
        .use_syslog                 = false,\
        .file_count                 = 0,\
        .file_filter                = "*",\
        .retransmit_count           = 0,\
        .transmit_current_files     = false,\
        .listen_for_new_files       = true,\
        .dirwatch_timeout           = -1,\
        .tx_delay                   = 0,\
        .file_delay                 = 0,\
        .device                     = "mon0",\
        .tx = DXWIFI_TRANSMITTER_DFLT_INITIALIZER\
    }\


/**
 *  DESCRIPTION:    Parse command line arguments into the cli_args struct
 * 
 *  ARGUMENTS:
 * 
 *      argc:       Number of command line arguments
 * 
 *      argv:       list of arguments
 * 
 *      out:        Pointer to allocated cli_args structure.
 * 
 *  RETURNS:
 *      
 *      int:        0 if arguments were parsed successfully
 *     
 *  
 */
int parse_args(int argc, char** argv, cli_args* out);
