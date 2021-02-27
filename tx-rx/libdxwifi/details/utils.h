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


#include <stdint.h>

#include <sys/stat.h>


static inline void set_bit32(uint32_t *word, uint32_t bit) {
    *word |= (1 << bit);
}


static inline void clr_bit32(uint32_t *word, uint32_t bit) {
    *word &= ~(1 << bit);
}


static inline void flip_bit32(uint32_t *word, uint32_t bit) {
    *word ^= (1 << bit);
}


static inline void set_bits32(uint32_t* word, uint32_t mask, uint32_t value) {
    *word = (*word & ~mask) | (value & mask);
}


static inline uint32_t check_bit32(uint32_t *word, uint32_t bit) {
    return *word & (1 << bit);
}


static inline void set_bit16(uint16_t *word, uint16_t bit) {
    *word |= (1 << bit);
}


static inline void clr_bit16(uint16_t *word, uint16_t bit) {
    *word &= ~(1 << bit);
}


static inline void flip_bit16(uint16_t *word, uint16_t bit) {
    *word ^= (1 << bit);
}


static inline void set_bits16(uint16_t* word, uint16_t mask, uint16_t value) {
    *word = (*word & ~mask) | (value & mask);
}


static inline uint32_t check_bit16(uint16_t *word, uint16_t bit) {
    return *word & (1 << bit);
}


static bool is_regular_file(const char* path) {
    struct stat path_stat;
    return (lstat(path, &path_stat) == 0 && S_ISREG(path_stat.st_mode));
}


static bool is_directory(const char* path) {
    struct stat path_stat;
    return (lstat(path, &path_stat) == 0 && S_ISDIR(path_stat.st_mode));
}


// Gets rid of `unused-parameter` warnings in release builds. Should only be 
// used in situations where the parameter being ignored is used in concert with 
// a conditionally compiled function.
static inline void __unused(const int dummy, ...) { (void)dummy; }
#define __DXWIFI_UTILS_UNUSED(...)\
  do { if(0) __unused(0, ##__VA_ARGS__); } while(0)


#endif // LIBDXWIFI_UTILITY_H
