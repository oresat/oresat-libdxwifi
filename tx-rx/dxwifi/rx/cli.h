/**
 *  cli.h
 *  
 *  DESCRIPTION: Command line interface for tx.c
 * 
 *  https://github.com/oresat/oresat-dxwifi-software
 * 
 */


#include <libdxwifi/receiver.h>


typedef enum {
    RX_FILE_MODE,
    RX_STREAM_MODE,
    RX_DIRECTORY_MODE,
} rx_mode_t;


typedef struct {
    rx_mode_t rx_mode;
    int verbosity;
    bool append;
    const char* device;
    const char* output_path;
    const char* file_prefix;
    const char* file_extension;
    dxwifi_receiver rx;
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
