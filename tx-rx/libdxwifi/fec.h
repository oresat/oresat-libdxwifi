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

/** 
 *  The OTI header (Object Transmission Info) stores important parameters 
 *  regarding how the message was encoded. Since these parameters are critical
 *  for decoding an OTI header is attached to the front of every single LDPC
 *  encoded frame
 */
typedef struct __attribute__((packed)) {
    uint32_t esi;   /* Encoding Symbol ID           */
    uint32_t n;     /* Total number of symbols      */
    uint32_t k;     /* Number of source symbols     */
    uint32_t crc;   /* Computed CRC of the symbol   */
} dxwifi_oti; 
compiler_assert(sizeof(dxwifi_oti) == 16, "Mismatch in actual OTI size and calculated size");


/**
 *  The LDPC (Low Density Parity Check) frame is an LDPC encoded block of size 
 *  `DXWIFI_FEC_SYMBOL_SIZE` of the original raw message. LDPC Frame can be 
 *  either a source symbol or a repair symbol. If `oti.esi` is greater than 
 *  `oti.k` then it is a repair symbol. 
 */
typedef struct __attribute__((packed)) {
    dxwifi_oti oti;     /* Object Transmission Info */
    uint8_t symbol[DXWIFI_FEC_SYMBOL_SIZE]; 
                        /* Actual symbol data       */
} dxwifi_ldpc_frame;
compiler_assert(sizeof(dxwifi_ldpc_frame) == DXWIFI_LDPC_FRAME_SIZE, "Mismatch in actual LDPC Frame size and calculated size");


/**
 *  The RS_LDPC is the LDPC encoded symbol that has been fragmented into 5 
 *  chunks of size `RSCODE_MAX_MSG_LEN` and then Reed Solomon encoded with 
 *  `RSCODE_NPAR` parity bits.
 */
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
