/**
 *  decoder.h - API to decode FEC data
 * 
 *  https://github.com/oresat/oresat-dxwifi-software
 *
 */

#ifndef LIBDXWIFI_DECODER_H
#define LIBDXWIFI_DECODER_H

/************************
 *  Includes
 ***********************/

#include <stdlib.h>
#include <stdint.h>


/************************
 *  Functions
 ***********************/


/**
 *  DESCRIPTION: 
 * 
 */
size_t dxwifi_decode(void* encoded_message, size_t msglen, void** out);

#endif // LIBDXWIFI_DECODER_H