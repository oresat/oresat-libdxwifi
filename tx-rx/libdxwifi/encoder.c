/**
 *  encoder.h - See encoder.h for description
 *
 *  https://github.com/oresat/oresat-dxwifi-software
 * 
 */

#include <math.h>

#include <arpa/inet.h>

#include <rscode/ecc.h>
#include <of_openfec_api.h>

#include <libdxwifi/encoder.h>
#include <libdxwifi/details/utils.h>
#include <libdxwifi/details/crc32.h>
#include <libdxwifi/details/assert.h>
#include <libdxwifi/details/logging.h>



typedef struct {
    uint32_t k;
    uint32_t n;
    of_session_t* openfec_session;
} dxwifi_encoder;


static void log_encoder_config(dxwifi_encoder* encoder, of_ldpc_parameters_t* params) {
    log_info(
        "DxWiFi Encoder\n"
        "\tK:           %d\n"
        "\tN:           %d\n"
        "\tN1:          %d\n"
        "\tPRNG Seed:   %d\n",
        encoder->k,
        encoder->n,
        params->N1,
        params->prng_seed
    );
}


static void log_ldpc_data_frame(dxwifi_oti* oti) {
    log_debug("ESI: %u, CRC: 0x%x", ntohl(oti->esi), ntohl(oti->crc));
    log_hexdump((uint8_t*)oti, DXWIFI_LDPC_FRAME_SIZE);
}


// Caculate N,K values, initialize openfec session
static void init_encoder(dxwifi_encoder* encoder, size_t msglen, float coderate) {
    debug_assert(encoder);

    of_status_t status = OF_STATUS_OK;

    uint32_t k = ceil((float) msglen / DXWIFI_FEC_SYMBOL_SIZE);
    uint32_t n = k / coderate;  

    encoder->k = k;
    encoder->n = n;
    encoder->openfec_session = NULL;


    of_ldpc_parameters_t codec_params = {
        .nb_source_symbols      = k,
        .nb_repair_symbols      = n - k,
        .encoding_symbol_length = DXWIFI_FEC_SYMBOL_SIZE,
        .prng_seed              =  rand(), // TODO no seed for rand()
        .N1                     = (n-k) > DXWIFI_LDPC_N1_MAX ? DXWIFI_LDPC_N1_MAX : (n-k) 
    };
    assert_M(codec_params.N1 >= DXWIFI_LDPC_N1_MIN, "N - K must be >= %d", DXWIFI_LDPC_N1_MIN);

    log_encoder_config(encoder, &codec_params);

    status = of_create_codec_instance(&encoder->openfec_session, OF_CODEC_LDPC_STAIRCASE_STABLE, OF_ENCODER, 2); // TODO magic number
    assert_M(status == OF_STATUS_OK, "Failed to initialize OpenFEC session");

    status = of_set_fec_parameters(encoder->openfec_session, (of_parameters_t*) &codec_params);
    assert_M(status == OF_STATUS_OK, "Failed to set codec parameters");
}


// Teardown openfec instance
static void close_encoder(dxwifi_encoder* encoder) {
    if(encoder) {
        of_release_codec_instance(encoder->openfec_session);
    }
}


// FEC encode the message
size_t dxwifi_encode(void* message, size_t msglen, float coderate, void** out) {
    debug_assert(message && out);
    debug_assert(0.0 < coderate && coderate <= 1.0);

    of_status_t status = OF_STATUS_OK;
    size_t stride = DXWIFI_LDPC_FRAME_SIZE;

    dxwifi_encoder encoder;

    init_encoder(&encoder, msglen, coderate);

    void* encoded_message = calloc(encoder.n, stride);
    assert_M(encoded_message, "Failed to allocate memory for encoded message");

    // Setup symbol table and CRCs
    uint32_t crcs[encoder.n];
    void* symbol_table[encoder.n];

    // Load source symbols into symbol table, and calculate CRCs
    for(size_t esi = 0; esi < encoder.k; ++esi) { 
        void* symbol = offset(encoded_message, esi, stride) + sizeof(dxwifi_oti);

        // Copy symbol size bytes from message, or whatevers left over
        size_t nbytes = DXWIFI_FEC_SYMBOL_SIZE < msglen ? DXWIFI_FEC_SYMBOL_SIZE : msglen;
        memcpy(symbol, offset(message, esi, DXWIFI_FEC_SYMBOL_SIZE), nbytes);

        symbol_table[esi] = symbol;

        crcs[esi] = crc32(symbol, DXWIFI_FEC_SYMBOL_SIZE);

        msglen -= nbytes;
    }

    // Build repair symbols and calculate CRCs
    for(size_t esi = encoder.k; esi < encoder.n; ++esi) {
        symbol_table[esi] = offset(encoded_message, esi, stride) + sizeof(dxwifi_oti);
        status = of_build_repair_symbol(encoder.openfec_session, symbol_table, esi);
        assert_continue(status == OF_STATUS_OK, "Failed to build repair symbol. esi=%d", esi);

        crcs[esi] = crc32(symbol_table[esi], DXWIFI_FEC_SYMBOL_SIZE);
    }

    // Fill out OTI headers
    for(size_t esi = 0; esi < encoder.n; ++esi) {
        dxwifi_oti* oti  = offset(encoded_message, esi, stride);
        oti->esi         = htonl(esi);
        oti->n           = htonl(encoder.n);
        oti->k           = htonl(encoder.k);
        oti->crc         = htonl(crcs[esi]);

        log_ldpc_data_frame(oti);

        // TODO for each ldpc data frame, RS solomon encode

    }

    *out = encoded_message;

    close_encoder(&encoder);
    return encoder.n * stride;
}
