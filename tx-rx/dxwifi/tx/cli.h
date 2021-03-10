/**
 *  cli.h
 *  
 *  DESCRIPTION: Command line interface for tx.c
 * 
 *  https://github.com/oresat/oresat-dxwifi-software
 * 
 */


#include <libdxwifi/transmitter.h>


typedef enum {
    TX_FILE_MODE,
    TX_STREAM_MODE,
    TX_DIRECTORY_MODE,
} tx_mode_t;

// TODO this is defined arbitrarily, is there an upper limit to the number of 
// Files to transmit at a time? 
#define TX_CLI_FILE_MAX 1024

typedef struct {
    tx_mode_t           tx_mode;
    char*               files[TX_CLI_FILE_MAX];
    int                 file_count;
    const char*         file_filter;
    bool                transmit_current_files;
    bool                listen_for_new_files;
    int                 dirwatch_timeout; 
    int                 verbosity;
    bool                quiet;
    unsigned            tx_delay;
    unsigned            file_delay;
    const char*         device;
    dxwifi_transmitter  tx;
} cli_args;


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
