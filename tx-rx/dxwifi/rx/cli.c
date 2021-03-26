/**
 *  cli.c
 *  
 *  DESCRIPTION: Command line interface for rx.c
 * 
 *  https://github.com/oresat/oresat-dxwifi-software
 * 
 */

#include <argp.h>
#include <stdlib.h>

#include <dxwifi/rx/cli.h>

#include <libdxwifi/details/utils.h>

#ifndef DXWIFI_RX_VERSION
#define DXWIFI_RX_VERSION "0.1.0-dev"
#endif // DXWIFI_RX_VERSION

#define PRIMARY_GROUP           0
#define DIRECTORY_MODE_GROUP    500
#define PCAP_SETTINGS_GROUP     1000
#define HELP_GROUP              1500

#if defined(DXWIFI_TESTS)
#define TEST_GROUP (HELP_GROUP + 500)
#endif

#define GET_KEY(x, group) (x + group)

typedef enum {
    SNAPLEN,
    BUFFER_TIMEOUT,
    FILTER,
    NO_OPTIMIZE,
} pcap_settings_t;


const char* argp_program_version = DXWIFI_RX_VERSION;

// Description of key arguments 
static char args_doc[] = "output-file/directory";

// Program description
static char doc[] = 
    "Capture packets matching a BPF program filter and output payload data to stdout/file(s)";

// Available command line options 
static struct argp_option opts[] = { 
    { "dev",            'd', "<network-device>",    0, "Monitor mode enabled network interface",                                PRIMARY_GROUP },
    { "timeout",        't', "<seconds>",           0, "Number of seconds to wait for a packet (default: infinity)",            PRIMARY_GROUP },
    { "dispatch-count", 'c', "<number>",            0, "Number of packets to process at a time",                                PRIMARY_GROUP },
    { "buffsize",       'b', "<nbytes>",            0, "Size of intermediate packet buffer in bytes",                           PRIMARY_GROUP },
    { "append",         'a', 0,                     0, "Open files in append mode",                                             PRIMARY_GROUP },
    { "ordered",        'o', 0,                     0, "Expect packets to have sequence informations",                          PRIMARY_GROUP },
    { "add-noise",      'n', 0,                     0, "Add noise for missing packets",                                         PRIMARY_GROUP },

    { 0, 0, 0, 0, "The following settings are only applicable when outputting to a directory",      DIRECTORY_MODE_GROUP },
    { "prefix",         'p', "<file-prefix>",       0, "What to name each created file",            DIRECTORY_MODE_GROUP },
    { "extension",      'e', "<file-extension>",    0, "Extension for each created file",           DIRECTORY_MODE_GROUP },

    { 0, 0, 0, 0, "Packet Capture Settings (https://www.tcpdump.org/manpages/pcap.3pcap.html)", PCAP_SETTINGS_GROUP },
    { "snaplen",        GET_KEY(SNAPLEN,        PCAP_SETTINGS_GROUP),    "<bytes>",      OPTION_NO_USAGE,    "Snapshot length in bytes",             PCAP_SETTINGS_GROUP },
    { "buffer-timeout", GET_KEY(BUFFER_TIMEOUT, PCAP_SETTINGS_GROUP),    "<ms>",         OPTION_NO_USAGE,    "Packet buffer timeout",                PCAP_SETTINGS_GROUP },
    { "filter",         GET_KEY(FILTER,         PCAP_SETTINGS_GROUP),    "<string>",     OPTION_NO_USAGE,    "Berkely Packet Filter expression",     PCAP_SETTINGS_GROUP },
    { "no-optimize",    GET_KEY(NO_OPTIMIZE,    PCAP_SETTINGS_GROUP),    0,              OPTION_NO_USAGE,    "Do not optimize the BPF expression",   PCAP_SETTINGS_GROUP },

    { 0, 0, 0, 0, "Help options", HELP_GROUP },
    { "verbose", 'v', 0, 0, "Verbosity level",              HELP_GROUP },
    { "syslog",  's', 0, 0, "Use SysLog for messages",      HELP_GROUP }, 
    { "quiet",   'q', 0, 0, "Silence any output",           HELP_GROUP },

#if defined(DXWIFI_TESTS)
    { 0, 0, 0, 0, "WARNING! You are running a test build!", TEST_GROUP },
    { "savefile", GET_KEY(1, TEST_GROUP), "<filename>", 0, "Dump packetized data into this file", TEST_GROUP },
#endif

    { 0 } // Final zero field is required by arg
};


static error_t parse_opt(int key, char* arg, struct argp_state *state) {

    error_t status = 0;
    cli_args* args = (cli_args*) state->input;

    // TODO error handling with atoi
    switch (key)
    {
    case ARGP_KEY_ARG:
        if(state->arg_num >= 1) {
            argp_usage(state);
        }
        else {
            args->output_path = arg;
        }
        break;

    case ARGP_KEY_END:
        if(state->arg_num > 0) {
            if (is_directory(args->output_path) ) {
                args->rx_mode = RX_DIRECTORY_MODE;
            }
            else {
                args->rx_mode = RX_FILE_MODE;
            }
        }
        else {
            args->rx_mode = RX_STREAM_MODE;
        }
        if(args->quiet) {
            args->verbosity = 0;
        }
        break;

    case 'd':
        args->device = arg;
        break;

    case 'a':
        args->append = true;
        break;

    case 't':
        args->rx.capture_timeout = atoi(arg); 
        break;

    case 'c':
        args->rx.dispatch_count = atoi(arg);
        break;

    case 'b':
        args->rx.packet_buffer_size = atoi(arg);
        if(  args->rx.packet_buffer_size < DXWIFI_RX_PACKET_BUFFER_SIZE_MIN
          || args->rx.packet_buffer_size > DXWIFI_RX_PACKET_BUFFER_SIZE_MAX ) {
              argp_error(
                  state,
                  "Packet buffer size of `%s` is not in range (%d,%d)\n",
                  arg,
                  DXWIFI_RX_PACKET_BUFFER_SIZE_MIN,
                  DXWIFI_RX_PACKET_BUFFER_SIZE_MAX
              );
              argp_usage(state);
           }
        break;

    case 'v':
        ++args->verbosity;
        break;

    case 'q':
        args->quiet = true;
        break;

    case 'p':
        args->file_prefix = arg;
        break;

    case 'e': 
        args->file_extension = arg;
        break;

    case 'o':
        args->rx.ordered = true;
        break;

    case 'n':
        args->rx.add_noise = true;
        break;

    case 's':
        args->use_syslog = true;
        break;

    case GET_KEY(SNAPLEN, PCAP_SETTINGS_GROUP):
        args->rx.snaplen = atoi(arg);
        break;
    
    case GET_KEY(BUFFER_TIMEOUT, PCAP_SETTINGS_GROUP):
        args->rx.pb_timeout = atoi(arg);
        break;

    case GET_KEY(FILTER, PCAP_SETTINGS_GROUP):
        args->rx.filter = arg;
        break;

    case GET_KEY(NO_OPTIMIZE, PCAP_SETTINGS_GROUP):
        args->rx.optimize = false;
        break;

#if defined(DXWIFI_TESTS)
    case ARGP_KEY_INIT:
        args->rx.savefile = NULL;
        break;

    case GET_KEY(1, TEST_GROUP):
        args->rx.savefile = arg;
        break;
#endif 

    default:
        status = ARGP_ERR_UNKNOWN;
        break;
    }
    return status;
}


static struct argp argparser = { 
    .options        = opts, 
    .parser         = parse_opt, 
    .args_doc       = args_doc, 
    .doc            = doc, 
    .children       = 0, 
    .help_filter    = 0,
    .argp_domain    = 0
};


int parse_args(int argc, char** argv, cli_args* out) {

    return argp_parse(&argparser, argc, argv, 0, 0, out);

}
