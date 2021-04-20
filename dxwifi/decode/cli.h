/**
 *  cli.h - Command line interface for `decode`
 */

#include <stdbool.h>


typedef struct {
    const char* file_in;
    const char* file_out;
    int         verbosity;
    bool        quiet;
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
