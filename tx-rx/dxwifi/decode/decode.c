/**
 *  encode.c - FEC Encoding program
 * 
 */


#include <math.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

#include <dxwifi/decode/cli.h>

#include <libdxwifi/decoder.h>
#include <libdxwifi/details/utils.h>
#include <libdxwifi/details/assert.h>
#include <libdxwifi/details/logging.h>

void decode_file(cli_args* args);
void decode_stream(cli_args* args);

int main(int argc, char** argv) {
    cli_args args = {
        .file_in    = NULL,
        .file_out   = NULL,
        .verbosity  = DXWIFI_LOG_INFO,
        .quiet      = false
    };

    parse_args(argc, argv, &args);

    set_log_level(DXWIFI_LOG_ALL_MODULES, args.verbosity);

    if(args.file_in) {
        decode_file(&args);
    }
    else {
        decode_stream(&args);
    }

    exit(0);
}


void decode_file(cli_args* args) {

    // Setup File In / File Out
    int fd_in = open(args->file_in, O_RDWR);
    assert_M(fd_in > 0, "Failed to open file: %s - %s", args->file_in, strerror(errno));

    int open_flags  = O_WRONLY | O_CREAT | O_TRUNC;
    mode_t mode     = S_IRUSR  | S_IWUSR | S_IROTH | S_IWOTH; 

    int fd_out      = args->file_out ? open(args->file_out, open_flags, mode) : STDOUT_FILENO;
    assert_M(fd_out > 0, "Failed to open file: %s - %s", args->file_out, strerror(errno));

    off_t file_size = get_file_size(args->file_in);

    void* file_data = mmap(NULL, file_size, PROT_READ, MAP_SHARED, fd_in, 0);
    assert_M(file_data != MAP_FAILED, "Failed to map file to memory - %s", strerror(errno));

    // Decode file
    dxwifi_decoder* decoder = init_decoder(file_data, file_size);

    void* decoded_msg = NULL;
    size_t msglen = dxwifi_decode(decoder, file_data, file_size, &decoded_msg);

    if(decoded_msg) {
        int nbytes = write(fd_out, decoded_msg, msglen);
        assert_M(nbytes == msglen, "Partial write occured: %d/%d - %s", nbytes, msglen, strerror(errno));
        free(decoded_msg);
    }

    // Teardown resources
    close(fd_in);
    if(args->file_out) {
        close(fd_out);
    }
    close_decoder(decoder);
    munmap(file_data, file_size);
}


void decode_stream(cli_args* args) {
    assert_always("Unimplemented");
}