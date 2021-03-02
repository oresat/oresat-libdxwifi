/**
 *  utils.h
 * 
 *  DESCRIPTION: DxWiFi collection of utility macros/functions
 * 
 *  https://github.com/oresat/oresat-dxwifi-software
 * 
 */

#ifndef LIBDXWIFI_UTILITY_H
#define LIBDXWIFI_UTILITY_H


#include <time.h>
#include <errno.h>
#include <stdint.h>

#include <sys/stat.h>

#include <libdxwifi/dxwifi.h>


static inline void set_bits32(uint32_t* word, uint32_t mask, uint32_t value) {
    *word = (*word & ~mask) | (value & mask);
}


static inline void set_bits16(uint16_t* word, uint16_t mask, uint16_t value) {
    *word = (*word & ~mask) | (value & mask);
}


static bool is_regular_file(const char* path) {
    struct stat path_stat;
    return (lstat(path, &path_stat) == 0 && S_ISREG(path_stat.st_mode));
}


static bool is_directory(const char* path) {
    struct stat path_stat;
    return (lstat(path, &path_stat) == 0 && S_ISDIR(path_stat.st_mode));
}


static const char* control_frame_type_to_str(dxwifi_control_frame_t type) {
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


static int msleep(unsigned msec, bool require_elapsed) {
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


// Gets rid of `unused-parameter` warnings in release builds. Should only be 
// used in situations where the parameter being ignored is used in concert with 
// a conditionally compiled function.
static inline void __unused(const int dummy, ...) { (void)dummy; }
#define __DXWIFI_UTILS_UNUSED(...)\
  do { if(0) __unused(0, ##__VA_ARGS__); } while(0)


#endif // LIBDXWIFI_UTILITY_H
