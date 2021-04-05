/**
 *  utils.c
 * 
 *  DESCRIPTION: See utils.h for details
 * 
 *  https://github.com/oresat/oresat-dxwifi-software
 * 
 */

#include <time.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include <string.h>
#include <sys/stat.h>

#include <libdxwifi/dxwifi.h>
#include <libdxwifi/details/utils.h>


inline void set_bits32(uint32_t* word, uint32_t mask, uint32_t value) {
    *word = (*word & ~mask) | (value & mask);
}


inline void set_bits16(uint16_t* word, uint16_t mask, uint16_t value) {
    *word = (*word & ~mask) | (value & mask);
}


bool is_regular_file(const char* path) {
    struct stat path_stat;
    return (lstat(path, &path_stat) == 0 && S_ISREG(path_stat.st_mode));
}


bool is_directory(const char* path) {
    struct stat path_stat;
    return (lstat(path, &path_stat) == 0 && S_ISDIR(path_stat.st_mode));
}


off_t get_file_size(const char* path) {
    struct stat path_stat;

    if(stat(path, &path_stat) == 0 && S_ISREG(path_stat.st_mode)) {
        return path_stat.st_size;
    }

    return -1;
}


const char* control_frame_type_to_str(dxwifi_control_frame_t type) {
    switch (type)
    {
    case DXWIFI_CONTROL_FRAME_PREAMBLE:
        return "Preamble";
    
    case DXWIFI_CONTROL_FRAME_EOT:
        return "EOT";

    case DXWIFI_CONTROL_FRAME_NONE:
        return "None";

    default:
        return "Unknown";
    }
}


int msleep(unsigned msec, bool require_elapsed) {
    int status;
    struct timespec ts = {
        .tv_sec  = msec / 1000,
        .tv_nsec = (msec % 1000) * 1000000
    };

    do {
        status = nanosleep(&ts, &ts);
    } while(status && errno == EINTR && require_elapsed);

    return status;
}


void combine_path(char* buffer, size_t n, const char* path, const char* filename) {
    if(rindex(path, '/')) {
        snprintf(buffer, n, "%s%s", path, filename);
    }
    else {
        snprintf(buffer, n, "%s/%s", path, filename);
    }
}

