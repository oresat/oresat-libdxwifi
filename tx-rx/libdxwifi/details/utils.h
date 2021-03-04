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
 *  DESCRIPTION:    Searches for the index matching the target value in an array
 * 
 *  ARGUMENTS: 
 *      
 *      array:      Allocated array
 * 
 *      n:          Number of elements in the array
 * 
 *  RETURNS:    
 *      
 *      int:        index to the matching value or -1
 * 
 */
int get_index(int* array, size_t n, int target);


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


// Gets rid of `unused-parameter` warnings in release builds. Should only be 
// used in situations where the parameter being ignored is used in concert with 
// a conditionally compiled function.
static inline void __unused(const int dummy, ...) { (void)dummy; }
#define __DXWIFI_UTILS_UNUSED(...)\
  do { if(0) __unused(0, ##__VA_ARGS__); } while(0)


#endif // LIBDXWIFI_UTILITY_H
