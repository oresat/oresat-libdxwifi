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
#include <libdxwifi/details/ieee80211.h>


#define PRIMARY_GROUP       0
#define MAC_HEADER_GROUP    500
#define RTAP_CONF_GROUP     1000
#define RTAP_FLAGS_GROUP    1500
#define RTAP_TX_FLAGS_GROUP 2000
#define HELP_GROUP          2500

#define GET_KEY(x, group) (x + group)


// Description of key arguments 
static char args_doc[] = "input-file(s)/directory";

// Program description
static char doc[] = 
    "Read bytes from input file(s) and inject them over a monitor mode enabled WiFi interface";

// Available command line options 
static struct argp_option opts[] = {
    { "dev",            'd', "<network-device>",    0, "Monitor mode enabled network interface",                                PRIMARY_GROUP },
    { "blocksize",      'b', "<blocksize>",         0, "Size in bytes of each block read from file",                            PRIMARY_GROUP },
    { "timeout",        't', "<seconds>",           0, "Number of seconds to wait for an available read",                       PRIMARY_GROUP },
    { "delay",          'u', "<useconds>",          0, "Length of time, in microseconds, to delay between transmission blocks", PRIMARY_GROUP },

    { 0, 0, 0, 0, "IEEE80211 MAC Header Configuration Options", MAC_HEADER_GROUP },
    { "address",        GET_KEY(1, MAC_HEADER_GROUP), "<macaddr>", OPTION_NO_USAGE, "MAC address of the transmitter",           MAC_HEADER_GROUP },

    { 0, 0, 0, 0, "Radiotap Header Configuration Options (WARN: settings are driver dependent and/or may not be supported by DxWiFi)",                       RTAP_CONF_GROUP  },
    { "rate",           GET_KEY(IEEE80211_RADIOTAP_RATE,            RTAP_CONF_GROUP),       0,  OPTION_NO_USAGE,  "Tx data rate (Mbps)",                    RTAP_CONF_GROUP  },
    { "cfp",            GET_KEY(IEEE80211_RADIOTAP_F_CFP,           RTAP_FLAGS_GROUP),      0,  OPTION_NO_USAGE,  "Sent during CFP",                        RTAP_FLAGS_GROUP },
    { "short-preamble", GET_KEY(IEEE80211_RADIOTAP_F_SHORTPRE,      RTAP_FLAGS_GROUP),      0,  OPTION_NO_USAGE,  "Sent with short preamble",               RTAP_FLAGS_GROUP },
    { "wep",            GET_KEY(IEEE80211_RADIOTAP_F_WEP,           RTAP_FLAGS_GROUP),      0,  OPTION_NO_USAGE,  "Sent with WEP encryption",               RTAP_FLAGS_GROUP },
    { "frag",           GET_KEY(IEEE80211_RADIOTAP_F_FRAG,          RTAP_FLAGS_GROUP),      0,  OPTION_NO_USAGE,  "Sent with fragmentation",                RTAP_FLAGS_GROUP },
    { "nofcs",          GET_KEY(IEEE80211_RADIOTAP_F_FCS,           RTAP_FLAGS_GROUP),      0,  OPTION_NO_USAGE,  "Frame does not includes FCS",            RTAP_FLAGS_GROUP },
    { "ack",            GET_KEY(IEEE80211_RADIOTAP_F_TX_NOACK,      RTAP_TX_FLAGS_GROUP),   0,  OPTION_NO_USAGE,  "Tx expects an ACK frame",                RTAP_TX_FLAGS_GROUP },
    { "sequence",       GET_KEY(IEEE80211_RADIOTAP_F_TX_NOSEQNO,    RTAP_TX_FLAGS_GROUP),   0,  OPTION_NO_USAGE,  "Tx includes preconfigured sequence id",  RTAP_TX_FLAGS_GROUP },
    { "ordered",        GET_KEY(IEEE80211_RADIOTAP_F_TX_ORDER,      RTAP_TX_FLAGS_GROUP),   0,  OPTION_NO_USAGE,  "Tx should not be reordered",             RTAP_TX_FLAGS_GROUP },

    { 0, 0, 0, 0, "Help Options", HELP_GROUP },
    { "verbose", 'v', 0, 0, "Verbosity level", HELP_GROUP },

    { 0 } // Final zero field is required by argp
};


static bool parse_mac_address(const char* arg, uint8_t* mac) {
    return sscanf(arg, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", mac, mac + 1, mac + 2, mac + 3, mac + 4, mac + 5) == 6;
}


static error_t parse_opt(int key, char* arg, struct argp_state *state) {

    error_t status = 0;
    cli_args* args = (cli_args*) state->input;

    switch (key)
    {
    case ARGP_KEY_END:
        if(state->arg_num > 0) {
            args->tx_mode = TX_FILE_MODE;
        } 
        else {
            args->tx_mode = TX_STREAM_MODE;
        }
        break;

    case ARGP_KEY_INIT:
        memset(args->files, 0x00, sizeof(char*) * TX_CLI_FILE_MAX);
        break;

    case ARGP_KEY_ARG:
        if(state->arg_num >= TX_CLI_FILE_MAX) {
            argp_error(state, "Reached maximum number of files to transmit");
            argp_usage(state);
        }
        args->files[state->arg_num] = arg;
        break;

    case 'd':
        args->device = arg;
        break;

    case 'b':
        args->tx.blocksize = atoi(arg);
        if(args->tx.blocksize < DXWIFI_BLOCK_SIZE_MIN || args->tx.blocksize > DXWIFI_BLOCK_SIZE_MAX) {
            argp_error("Blocksize must be in the range(%d, %d)", DXWIFI_BLOCK_SIZE_MIN, DXWIFI_BLOCK_SIZE_MAX);
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
        args->verbosity++;
        break;

    case 'u': 
        args->tx_delay = atoi(arg);
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
