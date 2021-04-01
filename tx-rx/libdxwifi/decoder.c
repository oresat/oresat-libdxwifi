/**
 *  decoder.c - See decoder.h
 * 
 */

#include <arpa/inet.h>

#include <rscode/ecc.h>
#include <of_openfec_api.h>

#include <libdxwifi/decoder.h>
#include <libdxwifi/encoder.h>
#include <libdxwifi/details/crc32.h>
#include <libdxwifi/details/utils.h>
#include <libdxwifi/details/assert.h>
#include <libdxwifi/details/logging.h>


#define member_size(type, member) sizeof(((type *)0)->member)


size_t dxwifi_decode(void* encoded_msg, size_t msglen, void** out) {
    debug_assert(encoded_msg && out);

    dxwifi_oti* oti      = encoded_msg;
    uint32_t esi         = ntohl(oti->esi);
    uint32_t n           = ntohl(oti->n);
    uint32_t k           = ntohl(oti->k);
    //uint32_t crc         = ntohl(oti->crc);

    log_debug("esi=%d, n=%d, k=%d", esi, n, k);

    of_ldpc_parameters_t codec_params = {
        .nb_source_symbols      = k,
        .nb_repair_symbols      = n - k,
        .encoding_symbol_length = DXWIFI_FEC_SYMBOL_SIZE,
        .prng_seed              = rand(),
        .N1                     = (n-k) > DXWIFI_LDPC_N1_MAX ? DXWIFI_LDPC_N1_MAX : (n-k)
    };
    of_session_t* openfec_session = NULL;
    of_status_t status = OF_STATUS_OK;

    status = of_create_codec_instance(&openfec_session, OF_CODEC_LDPC_STAIRCASE_STABLE, OF_DECODER, 2);
    assert_M(status == OF_STATUS_OK, "Failed to initialize OpenFEC session");

    status = of_set_fec_parameters(openfec_session, (of_parameters_t*) &codec_params);
    assert_M(status == OF_STATUS_OK, "Failed to set codec parameters");

    void* symbol_table[n];

    for (size_t i = 0; i < msglen; i += DXWIFI_LDPC_FRAME_SIZE) 
    {
        oti = encoded_msg + i;
        of_decode_with_new_symbol(openfec_session, encoded_msg + i + sizeof(dxwifi_oti), ntohl(oti->esi));
    }
    if(!of_is_decoding_complete(openfec_session)) {
        status = of_finish_decoding(openfec_session);
        if(status != OF_STATUS_OK) {
            log_fatal("Couldn't finish decoding");
            // TODO Asssert here? write out what we have? Can we recover from this?
        }
    }

    status = of_get_source_symbols_tab(openfec_session, symbol_table);
    if(status != OF_STATUS_OK) {
        log_fatal("Failed to get src symbols");
        // TODO Asssert here? write out what we have? Can we recover from this?
    }

    void* decoded_msg = calloc(k, DXWIFI_FEC_SYMBOL_SIZE);

    // TODO if the original file size is not evenly divisible by DXWIFI_FEC_SYMBOL_SIZE then the Kth 
    // Symbol will have extra zeroes to pad it until it is of size DXWIFI_FEC_SYMBOL_SIZE. We will need 
    // to find a way to identify and remove the extra padding.

    for(size_t esi = 0; esi < k; ++esi) {
        void* symbol = offset(decoded_msg, esi, DXWIFI_FEC_SYMBOL_SIZE);
        memcpy(symbol, symbol_table[esi], DXWIFI_FEC_SYMBOL_SIZE);
    }
    *out = decoded_msg;

    return k * DXWIFI_FEC_SYMBOL_SIZE;
}
