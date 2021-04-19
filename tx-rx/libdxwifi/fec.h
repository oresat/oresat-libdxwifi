/**
 *  fec.h - API to FEC encode data 
 *
 *  https://github.com/oresat/oresat-dxwifi-software
 * 
 */

#ifndef LIBDXWIFI_FEC_H
#define LIBDXWIFI_FEC_H

/************************
 *  Includes
 ***********************/

#include <stdlib.h>
#include <stdint.h>
#include <rscode/ecc.h>

#include <libdxwifi/details/assert.h>

/************************
 *  Constants
 ***********************/

// Number of RS encoded chunks per LDPC frame
#define DXWIFI_RSCODE_BLOCKS_PER_FRAME 5

// Total size of the LDPC encoded symbol with OTI
#define DXWIFI_LDPC_FRAME_SIZE (RSCODE_MAX_MSG_LEN * DXWIFI_RSCODE_BLOCKS_PER_FRAME)

// Size in bytes of each symbol
#define DXWIFI_FEC_SYMBOL_SIZE (DXWIFI_LDPC_FRAME_SIZE - sizeof(dxwifi_oti))

// Total size of the LDPC encoded symbol with RS encoding and OTI
#define DXWIFI_RS_LDPC_FRAME_SIZE ((DXWIFI_RSCODE_BLOCKS_PER_FRAME * RSCODE_NPAR) + DXWIFI_LDPC_FRAME_SIZE)

// https://tools.ietf.org/html/rfc6816 - N1 definition
#define DXWIFI_LDPC_N1_MAX 10
#define DXWIFI_LDPC_N1_MIN 3


/************************
 *  Data structures
 ***********************/

// TODO add comment
typedef struct __attribute__((packed)) {
    uint32_t esi;
    uint32_t n;
    uint32_t k;
    uint32_t crc;
} dxwifi_oti; 
compiler_assert(sizeof(dxwifi_oti) == 16, "Mismatch in actual OTI size and calculated size");


// TODO add comment
typedef struct __attribute__((packed)) {
    dxwifi_oti oti;
    uint8_t symbol[DXWIFI_FEC_SYMBOL_SIZE];
} dxwifi_ldpc_frame;
compiler_assert(sizeof(dxwifi_ldpc_frame) == DXWIFI_LDPC_FRAME_SIZE, "Mismatch in actual LDPC Frame size and calculated size");


// TODO add comment
typedef struct __attribute__((packed)) {
    uint8_t ldpc_frag0[RSCODE_MAX_MSG_LEN];
    uint8_t rs_block0[RSCODE_NPAR];

    uint8_t ldpc_frag1[RSCODE_MAX_MSG_LEN];
    uint8_t rs_block1[RSCODE_NPAR];

    uint8_t ldpc_frag2[RSCODE_MAX_MSG_LEN];
    uint8_t rs_block2[RSCODE_NPAR];

    uint8_t ldpc_frag3[RSCODE_MAX_MSG_LEN];
    uint8_t rs_block3[RSCODE_NPAR];

    uint8_t ldpc_frag4[RSCODE_MAX_MSG_LEN];
    uint8_t rs_block4[RSCODE_NPAR];
} dxwifi_rs_ldpc_frame;
compiler_assert(sizeof(dxwifi_rs_ldpc_frame) == DXWIFI_RS_LDPC_FRAME_SIZE, "Mismatch in actual RS-LDPC Frame size and calculated size");


/************************
 *  Functions
 ***********************/


/**
 *  DESCRIPTION:        FEC Encodes a message and stores it in @out
 * 
 *  ARGUMENTS:
 *      
 *      message:        Message data to be encoded
 * 
 *      msglen:         Size of the message in bytes
 *
 *      coderate:       Rate at which to add repair symbols for each source symbol
 * 
 *      out:            Pointer to a void pointer which will contain the encoded
 *                      message on function return. 
 * 
 *  RETURNS:
 * 
 *      size_t:         Size of the encoded message in bytes
 * 
 * 
 *  NOTES:
 * 
 *      It is the users responsibility to free the encoded message pointed to by 
 *      the out parameter.
 * 
 */
size_t dxwifi_encode(void *message, size_t msglen, float coderate, void **out);


/**
 *  DESCRIPTION:  TODO
 * 
 */
size_t dxwifi_decode(void* encoded_message, size_t msglen, void** out);

#endif // LIBDXWIFI_FEC_H
