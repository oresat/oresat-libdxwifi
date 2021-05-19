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
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include <sys/stat.h>

#include <libdxwifi/dxwifi.h>


/**
 *  DESCRIPTION:    Sets the masked bits in word to that of value
 * 
 *  ARGUMENTS: 
 *      
 *      word:       Word to be modified
 * 
 *      mask:       Mask of desired bits to change
 * 
 *      value:      Desired value of bits
 * 
 */
void set_bits32(uint32_t* word, uint32_t mask, uint32_t value);


/**
 *  DESCRIPTION:    Sets the masked bits in word to that of value
 * 
 *  ARGUMENTS: 
 *      
 *      word:       Word to be modified
 * 
 *      mask:       Mask of desired bits to change
 * 
 *      value:      Desired value of bits
 * 
 */
void set_bits16(uint16_t* word, uint16_t mask, uint16_t value);


/**
 *  DESCRIPTION:    Determines if the given path is a regular file
 * 
 *  ARGUMENTS: 
 *      
 *      path:       Path to file to check
 * 
 *  RETURNS:    
 *      
 *      bool:       True if the path leads to a regular file
 */
bool is_regular_file(const char* path);


/**
 *  DESCRIPTION:    Determines if the given path is a directory
 * 
 *  ARGUMENTS: 
 *      
 *      path:       Path to directory
 * 
 *  RETURNS:    
 *      
 *      bool:       True if the path leads to a directory
 */
bool is_directory(const char* path);


/**
 *  DESCRIPTION:    Determines if a process is still alive
 * 
 *  ARGUMENTS: 
 *      
 *      pid:        PID in question
 * 
 *  RETURNS:    
 *      
 *      bool:       True if the pid exists in /proc/
 */
bool is_alive(int pid);


/**
 *  DESCRIPTION:    Get the size of the file in bytes
 * 
 *  ARGUMENTS: 
 *      
 *      path:       Path to the file
 * 
 *  RETURNS:    
 *      
 *      off_t:     Size, in bytes, of the file  or -1
 * 
 *  NOTES: For 32 bit systems off_t is limited to about ~2GB. For Large File 
 *  Support see https://users.suse.com/~aj/linux_lfs.html
 */
off_t get_file_size(const char* path);


/**
 *  DESCRIPTION:    Converts the control frame type to a string
 * 
 *  ARGUMENTS: 
 *      
 *      type:       Control frame type
 * 
 *  RETURNS:    
 *      
 *      const char*:    C string representing the type
 */
const char* control_frame_type_to_str(dxwifi_control_frame_t type);


/**
 *  DESCRIPTION:    Millisecond sleep
 * 
 *  ARGUMENTS: 
 *      
 *      msec:       Number of milliseconds to sleep 
 * 
 *      require_elapsed:  If this flag is set the subroutine will continue to
 *                        sleep until msec has elapsed despite interrupts
 * 
 *  RETURNS:    
 *      
 *      int:        0 if msec elapsed, or -1 if the sleep was interrupted
 */
int msleep(unsigned msec, bool require_elapsed);


/**
 *  DESCRIPTION:    Appends the filename to the path, adds in '/' if necessary
 * 
 *  ARGUMENTS: 
 *      
 *      buffer:     Buffer to hold path string. 
 * 
 *      n:          size of the buffer
 * 
 *      path:       Path string
 * 
 *      filename:   Filename to append to path
 * 
 */
void combine_path(char* buffer, size_t n, const char* path, const char* filename);


/**
 *  DESCRIPTION:    Calculates the offset from a pointer
 * 
 *  ARGUMENTS: 
 *      
 *      base:       base pointer to calculate offset from
 * 
 *      count:      Number of units offset from base
 * 
 *      sz:         Step size of each unit
 * 
 *  NOTES: Undefined behavior if base is null, count is greater than the number
 *  of items in the array, the computed offset overflows sizeof(void*)
 * 
 */
static inline void* offset(void* base, size_t count, size_t sz)   { return ((uint8_t*) base) + (count * sz); }


/**
 *  DESCRIPTION:    Calculates the hamming distance between a and b 
 * 
 *  ARGUMENTS: 
 *      
 *      a:          binary string
 * 
 *      b:          binary string
 * 
 *  RETURNS: 
 *    
 *      uint32_t:   The number of bit positions at which a and b are different
 */
static inline uint32_t hamming_dist32(uint32_t a, uint32_t b) {
  return __builtin_popcount(a ^ b);
}


/**
 *  DESCRIPTION:    Calculates the hamming distance between a and b 
 * 
 *  ARGUMENTS: 
 *      
 *      a:          binary string
 * 
 *      b:          binary string
 * 
 *  RETURNS: 
 *    
 *      uint64_t:   The number of bit positions at which a and b are different
 */
static inline uint64_t hamming_dist64(uint64_t a, uint64_t b) {
  return __builtin_popcountll(a ^ b);
}


/**
 *  DESCRIPTION:  Parses a MAC address string and stores the value into an array
 * 
 *  ARGUMENTS: 
 *      
 *      arg:      MAC address string
 * 
 *      mac:      Mutable reference to user owned memory of size IEEE80211_MAC_ADDR_LEN
 * 
 *  RETURNS: 
 *    
 *      bool:     True if the operation was successful.
 * 
 *  NOTES: Even if this operation fails the provided array could still be 
 *    mutated with a partial result. 
 * 
 */
static inline bool parse_mac_address(const char* arg, uint8_t* mac) {
    return sscanf(arg, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", mac, mac + 1, mac + 2, mac + 3, mac + 4, mac + 5) == 6;
}


// Gets rid of `unused-parameter` warnings in release builds. Should only be 
// used in situations where the parameter being ignored is used in concert with 
// a conditionally compiled function.
static inline void __dxwifi_unused(const int dummy, ...) { (void)dummy; }
#define __DXWIFI_UTILS_UNUSED(...)\
  do { if(0) __dxwifi_unused(0, ##__VA_ARGS__); } while(0)


// Only use for array types NOT pointers
#define NELEMS(x) (sizeof(x) / sizeof((x)[0]))


#endif // LIBDXWIFI_UTILITY_H
