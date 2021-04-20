/**
 *  cli.c
 *  
 *  DESCRIPTION: Command line interface for tx.c
 * 
 *  https://github.com/oresat/oresat-dxwifi-software
 * 
 */


#include <argp.h>
#include <string.h>
#include <stdlib.h>

#include <dxwifi/tx/cli.h>

#include <libdxwifi/dxwifi.h>
#include <libdxwifi/details/utils.h>
#include <libdxwifi/details/ieee80211.h>


#define PRIMARY_GROUP           0
#define DIRECTORY_MODE_GROUP    1000
#define MAC_HEADER_GROUP        1500
#define RTAP_CONF_GROUP         2000
#define RTAP_FLAGS_GROUP        2500
#define RTAP_TX_FLAGS_GROUP     3000
#define HELP_GROUP              3500

#if defined(DXWIFI_TESTS)
#define TEST_GROUP (HELP_GROUP + 500)
#endif

#define GET_KEY(x, group) (x + group)


typedef enum {
    FILE_FILTER,
    INCLUDE_ALL_FLAG,
    NO_LISTEN_FLAG,
    WATCHDIR_TIMEOUT,
} directory_mode_settings_t;


const char* argp_program_version = DXWIFI_VERSION;

// Description of key arguments 
static char args_doc[] = "input-file(s)/directory(s)";

// Program description
static char doc[] = 
    "Read bytes from input file(s) and inject them over a monitor mode enabled WiFi interface";

// Available command line options 
static struct argp_option opts[] = {
    { "dev",            'd', "<network-device>",    0,  "Monitor mode enabled network interface",                                        PRIMARY_GROUP },
    { "blocksize",      'b', "<blocksize>",         0,  "Size in bytes of each block read from file",                                    PRIMARY_GROUP },
    { "timeout",        't', "<seconds>",           0,  "Number of seconds to wait for an available read",                               PRIMARY_GROUP },
    { "delay",          'u', "<mseconds>",          0,  "Length of time, in milliseconds, to delay between transmission blocks",         PRIMARY_GROUP },
    { "file-delay",     'f', "<mseconds>",          0,  "Length of time in milliseconds to delay between file transmissions",            PRIMARY_GROUP },
    { "redundancy",     'r', "<number>",            0,  "Number of extra control frames to send",                                        PRIMARY_GROUP },
    { "retransmit",     'R', "<number>",            0,  "Number of times to retransmit, -1 for infinity",                                PRIMARY_GROUP },
    { "test",           'T',  0,                    0,  "Transmit a test sequence of bytes, use -c to retransmit it multiple times",     PRIMARY_GROUP },
    { "daemon",         'D',  "<start|stop>",       0,  "Run the tx program as a forked daemon process (Sets logger to syslog as well)", PRIMARY_GROUP },
    { "pid-file",       'P',  "<file-path>",        0,  "Location of the Daemon's PID File",                                             PRIMARY_GROUP },
    { "packet-loss",    'p',  "<float>",            0,  "Numbers of packets dropped",                                                    PRIMARY_GROUP },
<<<<<<< HEAD
    { "error-rate" ,    'e',  "<float>",            0,  "Numbers bits flipped",                                                          PRIMARY_GROUP },
=======
    { "error-rate",     'e',  "<float>",            0,  "Numbers bits flipped",                                                          PRIMARY_GROUP },
>>>>>>> 63999d3e6284d1d4d1dee978f047faa35febfd4b
    { "enable-pa",      'E',  0,                    0,  "Enable Power Amplifer (Only works on OreSat DxWiFi board)",                     PRIMARY_GROUP },


    { 0, 0, 0, OPTION_DOC, "The following settings are only applicable when reading from a directory", DIRECTORY_MODE_GROUP },
    { "filter",         GET_KEY(FILE_FILTER,        DIRECTORY_MODE_GROUP),  "<glob>",       OPTION_NO_USAGE,  "Only transmit files whose filename matches the filter",      DIRECTORY_MODE_GROUP },
    { "include-all",    GET_KEY(INCLUDE_ALL_FLAG,   DIRECTORY_MODE_GROUP),  0,              OPTION_NO_USAGE,  "include files currently in the directory",                   DIRECTORY_MODE_GROUP },
    { "no-listen",      GET_KEY(NO_LISTEN_FLAG,     DIRECTORY_MODE_GROUP),  0,              OPTION_NO_USAGE,  "Don't listen for new files in the directory",                DIRECTORY_MODE_GROUP },
    { "watch-timeout",  GET_KEY(WATCHDIR_TIMEOUT,   DIRECTORY_MODE_GROUP),  "<seconds>",    OPTION_NO_USAGE,  "Number of seconds to listen for new files",                  DIRECTORY_MODE_GROUP },

    { 0, 0, 0, OPTION_DOC, "IEEE80211 MAC Header Configuration Options", MAC_HEADER_GROUP },
    { "address",        GET_KEY(1, MAC_HEADER_GROUP), "<macaddr>", OPTION_NO_USAGE, "MAC address of the transmitter", MAC_HEADER_GROUP },

    { 0, 0, 0, OPTION_DOC, "Radiotap Header Configuration Options (WARN: settings are driver dependent and/or may not be supported by DxWiFi)", RTAP_CONF_GROUP  },
    { "rate",           GET_KEY(IEEE80211_RADIOTAP_RATE,            RTAP_CONF_GROUP),       "<Mbps>",   OPTION_NO_USAGE,  "Tx data rate (Mbps)",                    RTAP_CONF_GROUP  },
    { "cfp",            GET_KEY(IEEE80211_RADIOTAP_F_CFP,           RTAP_FLAGS_GROUP),      0,          OPTION_NO_USAGE,  "Sent during CFP",                        RTAP_FLAGS_GROUP },
    { "short-preamble", GET_KEY(IEEE80211_RADIOTAP_F_SHORTPRE,      RTAP_FLAGS_GROUP),      0,          OPTION_NO_USAGE,  "Sent with short preamble",               RTAP_FLAGS_GROUP },
    { "wep",            GET_KEY(IEEE80211_RADIOTAP_F_WEP,           RTAP_FLAGS_GROUP),      0,          OPTION_NO_USAGE,  "Sent with WEP encryption",               RTAP_FLAGS_GROUP },
    { "frag",           GET_KEY(IEEE80211_RADIOTAP_F_FRAG,          RTAP_FLAGS_GROUP),      0,          OPTION_NO_USAGE,  "Sent with fragmentation",                RTAP_FLAGS_GROUP },
    { "nofcs",          GET_KEY(IEEE80211_RADIOTAP_F_FCS,           RTAP_FLAGS_GROUP),      0,          OPTION_NO_USAGE,  "Frame does not includes FCS",            RTAP_FLAGS_GROUP },
    { "ack",            GET_KEY(IEEE80211_RADIOTAP_F_TX_NOACK,      RTAP_TX_FLAGS_GROUP),   0,          OPTION_NO_USAGE,  "Tx expects an ACK frame",                RTAP_TX_FLAGS_GROUP },
    { "sequence",       GET_KEY(IEEE80211_RADIOTAP_F_TX_NOSEQNO,    RTAP_TX_FLAGS_GROUP),   0,          OPTION_NO_USAGE,  "Tx includes preconfigured sequence id",  RTAP_TX_FLAGS_GROUP },
    { "ordered",        GET_KEY(IEEE80211_RADIOTAP_F_TX_ORDER,      RTAP_TX_FLAGS_GROUP),   0,          OPTION_NO_USAGE,  "Tx should not be reordered",             RTAP_TX_FLAGS_GROUP },

    { 0, 0, 0, OPTION_DOC, "Help Options", HELP_GROUP },
    { "verbose",    'v', 0, 0, "Verbosity level",           HELP_GROUP },
    { "syslog",     's', 0, 0, "Use SysLog for messages",   HELP_GROUP }, 
    { "quiet",      'q', 0, 0, "Silence any output",        HELP_GROUP },

#if defined(DXWIFI_TESTS)
    { 0, 0, 0, OPTION_DOC, "WARNING! You are running a development test build!", TEST_GROUP },
    { "savefile", GET_KEY(1, TEST_GROUP), "<filename>", 0, "Dump packetized data into this file", TEST_GROUP },
#endif

    { 0 } // Final zero field is required by argp
};


static bool parse_mac_address(const char* arg, uint8_t* mac) {
    return sscanf(arg, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", mac, mac + 1, mac + 2, mac + 3, mac + 4, mac + 5) == 6;
}


// TODO all these atois() need error handling
static error_t parse_opt(int key, char* arg, struct argp_state *state) {

    error_t status = 0;
    cli_args* args = (cli_args*) state->input;

    switch (key)
    {
    case ARGP_KEY_END:
        // Determine TX Mode
        if(!(args->tx_mode == TX_TEST_MODE)) {
            if(args->file_count > 0) {
                // TODO Dirwatch now supports multiple directories we don't need to limit to 1 anymore
                if(args->file_count == 1 && is_directory(args->files[0])) {
                    args->tx_mode = TX_DIRECTORY_MODE;
                }
                else { // TODO verify every file in the list is actually a file
                    args->tx_mode = TX_FILE_MODE;
                }
            }
            else {
                args->tx_mode = TX_STREAM_MODE;
            }
        }

        // Determine Verbosity
        if(args->quiet) {
            args->verbosity = 0;
        }
        break; 

    case ARGP_KEY_INIT:
        memset(args->files, 0x00, sizeof(char*) * TX_CLI_FILE_MAX);
#if defined(DXWIFI_TESTS)
        args->tx.savefile = NULL;
#endif
        break;

    case ARGP_KEY_ARG:
        if(args->file_count >= TX_CLI_FILE_MAX) {
            argp_error(state, "Reached maximum number of files to transmit");
            argp_usage(state);
        }
        args->files[args->file_count++] = arg;
        break;

    case 'd':
        args->device = arg;
        break;

    case 'b':
        args->tx.blocksize = atoi(arg);
        if(args->tx.blocksize < DXWIFI_BLOCK_SIZE_MIN || args->tx.blocksize > DXWIFI_BLOCK_SIZE_MAX) {
            argp_error(state, "Blocksize must be in the range(%d, %d)", DXWIFI_BLOCK_SIZE_MIN, DXWIFI_BLOCK_SIZE_MAX);
            argp_usage(state);
        }
        break;

    case 't':
        args->tx.transmit_timeout = atoi(arg);
        if(args->tx.transmit_timeout == 0) {
            argp_usage(state);
        }
        break;

    case 'v':
        ++args->verbosity;
        break;

    case 'q':
        args->quiet = true;
        break;

    case 'u': 
        args->tx_delay = atoi(arg);
        break;

    case 'r':
        args->tx.redundant_ctrl_frames = atoi(arg);
        break;

    case 'f':
        args->file_delay = atoi(arg);
        break;

    case 'R':
        args->retransmit_count = atoi(arg);
        break;

    case 's':
        args->use_syslog = true;
        break;

    case 'T':
        args->tx_mode = TX_TEST_MODE;
        break;

    case 'D':
        args->daemon = str_to_daemon_cmd(arg);
        break;

    case 'P':
        args->pid_file = arg;
        break;
    
    case 'p':
        args->packet_loss = atof(arg);
        //TODO: bounds check
        break;
    
    case 'e':
        args->error_rate = atof(arg);
        //TODO: bounds check
        break; 

    case 'E':
        args->tx.enable_pa = true;
        break;

    case GET_KEY(FILE_FILTER, DIRECTORY_MODE_GROUP):
        args->file_filter = arg;
        break;

    case GET_KEY(INCLUDE_ALL_FLAG, DIRECTORY_MODE_GROUP):
        args->transmit_current_files = true;
        break;

    case GET_KEY(NO_LISTEN_FLAG, DIRECTORY_MODE_GROUP):
        args->listen_for_new_files = false;
        break;

    case GET_KEY(WATCHDIR_TIMEOUT, DIRECTORY_MODE_GROUP):
        args->dirwatch_timeout = atoi(arg);
        break;

    case GET_KEY(1, MAC_HEADER_GROUP):
        if( !parse_mac_address(arg, args->tx.address) )
        {
            argp_error(state, "Mac address must be 6 octets in hexadecimal format delimited by a ':'");
            argp_usage(state);
        }
        break;

    case GET_KEY(IEEE80211_RADIOTAP_F_CFP, RTAP_FLAGS_GROUP):
        args->tx.rtap_flags |= IEEE80211_RADIOTAP_F_CFP;
        break;

    case GET_KEY(IEEE80211_RADIOTAP_F_SHORTPRE, RTAP_FLAGS_GROUP):
        args->tx.rtap_flags |= IEEE80211_RADIOTAP_F_SHORTPRE;
        break;

    case GET_KEY(IEEE80211_RADIOTAP_F_WEP, RTAP_FLAGS_GROUP):
        args->tx.rtap_flags |= IEEE80211_RADIOTAP_F_WEP;
        break;

    case GET_KEY(IEEE80211_RADIOTAP_F_FRAG, RTAP_FLAGS_GROUP):
        args->tx.rtap_flags |= IEEE80211_RADIOTAP_F_FRAG;
        break;

    // Clear bit since default is on
    case GET_KEY(IEEE80211_RADIOTAP_F_FCS, RTAP_FLAGS_GROUP):
        args->tx.rtap_flags &= ~(IEEE80211_RADIOTAP_F_FCS);
        break;

    case GET_KEY(IEEE80211_RADIOTAP_RATE, RTAP_CONF_GROUP):
        args->tx.rtap_rate_mbps = atoi(arg);
        break;

    // Clear bit since default is on
    case GET_KEY(IEEE80211_RADIOTAP_F_TX_NOACK, RTAP_TX_FLAGS_GROUP):
        args->tx.rtap_tx_flags &= ~(IEEE80211_RADIOTAP_F_TX_NOACK);
        break;

    case GET_KEY(IEEE80211_RADIOTAP_F_TX_NOSEQNO, RTAP_TX_FLAGS_GROUP):
        args->tx.rtap_tx_flags |= IEEE80211_RADIOTAP_F_TX_NOSEQNO;
        break;

    case GET_KEY(IEEE80211_RADIOTAP_F_TX_ORDER, RTAP_TX_FLAGS_GROUP):
        args->tx.rtap_tx_flags |= IEEE80211_RADIOTAP_F_TX_ORDER;
        break;

#if defined(DXWIFI_TESTS)
    case GET_KEY(1, TEST_GROUP):
        args->tx.savefile = arg;
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
