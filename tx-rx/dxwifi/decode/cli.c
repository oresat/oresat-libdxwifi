/**
 *  cli.c - See cli.h for description
 *  
 */


#include <argp.h>
#include <stdlib.h>

#include <dxwifi/decode/cli.h>
#include <libdxwifi/details/utils.h>

#define PRIMARY_GROUP   0
#define HELP_GROUP      500

#define GET_KEY(x, group) (x + group)

const char* argp_program_version = DXWIFI_VERSION;

// Description of key arguments 
static char args_doc[] = "input-file";

// Program description
static char doc[] = 
    "FEC Decode input-file and output to file or stdout";

// Available command line options 
static struct argp_option opts[] = {
    { "output",         'o', "<path>",              0, "Output file path",                                   PRIMARY_GROUP },

    { 0, 0, 0, 0, "Help Options", HELP_GROUP },
    { "verbose",    'v', 0, 0, "Verbosity level",           HELP_GROUP },
    { "quiet",      'q', 0, 0, "Silence any output",        HELP_GROUP },


    { 0 } // Final zero field is required by argp
};


static error_t parse_opt(int key, char* arg, struct argp_state *state) {

    error_t status = 0;
    cli_args* args = (cli_args*) state->input;

    switch (key)
    {
    case ARGP_KEY_END:
        if(args->quiet) {
            args->verbosity = 0;
        }
        break;

    case ARGP_KEY_ARG:
        if(state->arg_num > 0) {
            argp_usage(state);
        }
        else if (!is_regular_file(arg)){
            argp_error(state, "Error: Input file must be a regular file");
        }
        else {
            args->file_in = arg;
        }
        break;

    case 'o':
        args->file_out = arg;
        break;

    case 'v':
        ++args->verbosity;
        break;

    case 'q':
        args->quiet = true;
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
