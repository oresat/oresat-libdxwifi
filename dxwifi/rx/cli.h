/**
 *  cli.h
 *  
 *  DESCRIPTION: Command line interface for rx.c
 * 
 *  https://github.com/oresat/oresat-dxwifi-software
 * 
 */


#include <libdxwifi/receiver.h>


typedef enum {
    RX_FILE_MODE,
    RX_STREAM_MODE,
    RX_DIRECTORY_MODE,
    RX_BIT_ERROR_MODE
} rx_mode_t;


typedef struct {
    rx_mode_t       rx_mode;
    int             verbosity;
    bool            quiet;
    bool            append;
    bool            use_syslog;
    const char*     compare_path;
    const char*     device;
    const char*     output_path;
    const char*     file_prefix;
    const char*     file_extension;
    dxwifi_receiver rx;
} cli_args;


#define DEFAULT_CLI_ARGS {\
        .rx_mode        = RX_STREAM_MODE,\
        .verbosity      = DXWIFI_LOG_INFO,\
        .quiet          = false,\
        .append         = false,\
        .use_syslog     = false,\
        .device         = "mon0",\
        .output_path    = ".",\
        .file_prefix    = "rx",\
        .file_extension = "cap",\
        .rx = DXWIFI_RECEIVER_DFLT_INITIALIZER,\
        .compare_path      = NULL \
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
