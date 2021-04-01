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
 *  Data structures
 ***********************/

typedef struct __dxwifi_decoder dxwifi_decoder;

/************************
 *  Functions
 ***********************/

/**
 *  DESCRIPTION:        Initializes the FEC decoder
 * 
 *  ARGUMENTS:
 * 
 *      encoded:        Encoded message data 
 * 
 *      msglen:         Length, in bytes, of the raw message to encode
 * 
 *  RETURNS:
 *      
 *      dxwifi_decoder*: Allocated and initialized decoder object. Do not attempt
 *      to free this decoder. Please use the close_decoder() method instead. 
 * 
 */
dxwifi_decoder* init_decoder(void* encoded, size_t msglen);


/**
 *  DESCRIPTION:        Tearsdown any resources associated with the FEC decoder
 * 
 *  ARGUMENTS:
 *      
 *      decoder:        Pointer to an initialized dxwifi decoder
 * 
 */
void close_decoder(dxwifi_decoder* decoder);


/**
 *  DESCRIPTION: 
 * 
 */
size_t dxwifi_decode(dxwifi_decoder* decoder, void* encoded_message, size_t msglen, void** out);

#endif // LIBDXWIFI_DECODER_H