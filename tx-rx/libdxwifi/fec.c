/**
 *  fec.c - See fec.h for description
 *
 *  https://github.com/oresat/oresat-dxwifi-software
 * 
 */

#include <math.h>

#include <arpa/inet.h>

#include <rscode/ecc.h>
#include <of_openfec_api.h>

#include <libdxwifi/fec.h>
#include <libdxwifi/details/utils.h>
#include <libdxwifi/details/crc32.h>
#include <libdxwifi/details/assert.h>
#include <libdxwifi/details/logging.h>


#define member_size(type, member) sizeof(((type *)0)->member)


typedef struct {
    uint32_t k;
    uint32_t n;
    of_session_t* openfec_session;
} dxwifi_encoder; // TODO it's probably not necessary to have this object abstraction


typedef struct {
    dxwifi_oti* oti;
    void*       symbol;
} dxwifi_ldpc_frame;


typedef struct {
    void* ldpc_fragment[DXWIFI_RSCODE_BLOCKS_PER_FRAME];
    void* rs_block[DXWIFI_RSCODE_BLOCKS_PER_FRAME];
} dxwifi_rs_ldpc_frame;


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


static void log_ldpc_data_frame(dxwifi_ldpc_frame* frame) {
    log_debug("ESI: %u, CRC: 0x%x", ntohl(frame->oti->esi), ntohl(frame->oti->crc));
    log_hexdump((uint8_t*)frame->oti, DXWIFI_LDPC_FRAME_SIZE);
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
    log_encoder_config(encoder, &codec_params);
    assert_M(codec_params.N1 >= DXWIFI_LDPC_N1_MIN, "N - K must be >= %d", DXWIFI_LDPC_N1_MIN);


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


static dxwifi_ldpc_frame make_ldpc_frame(void* buffer, size_t esi) {
    void* data = offset(buffer, esi, DXWIFI_LDPC_FRAME_SIZE);
    dxwifi_ldpc_frame frame = {
        .oti = data,
        .symbol = data + sizeof(dxwifi_oti)
    };
    return frame;
}


static dxwifi_rs_ldpc_frame make_rs_ldpc_frame(void* buffer, size_t esi) {
    void* data = offset(buffer, esi, DXWIFI_RS_LDPC_FRAME_SIZE);
    dxwifi_rs_ldpc_frame frame;
    for (size_t i = 0; i < DXWIFI_RSCODE_BLOCKS_PER_FRAME; ++i)
    {
        frame.ldpc_fragment[i] = offset(data, i, RSCODE_MAX_LEN);
        frame.rs_block[i] = &frame.ldpc_fragment[i] + RSCODE_MAX_MSG_LEN;
    }
    return frame;
}


// FEC encode the message
size_t dxwifi_encode(void* message, size_t msglen, float coderate, void** out) {
    debug_assert(message && out);
    debug_assert(0.0 < coderate && coderate <= 1.0);

    of_status_t status = OF_STATUS_OK;
    size_t stride = DXWIFI_LDPC_FRAME_SIZE;

    dxwifi_encoder encoder;

    init_encoder(&encoder, msglen, coderate);

    void* ldpc_encoded_msg = calloc(encoder.n, stride);
    assert_M(ldpc_encoded_msg, "Failed to allocate memory for encoded message");

    // Setup symbol table and CRCs
    uint32_t crcs[encoder.n];
    void* symbol_table[encoder.n];

    // Load source symbols into symbol table, and calculate CRCs
    for(size_t esi = 0; esi < encoder.k; ++esi) { 
        dxwifi_ldpc_frame frame = make_ldpc_frame(ldpc_encoded_msg, esi);

        // Copy symbol size bytes from message, or whatevers left over
        size_t nbytes = DXWIFI_FEC_SYMBOL_SIZE < msglen ? DXWIFI_FEC_SYMBOL_SIZE : msglen;
        memcpy(frame.symbol, offset(message, esi, DXWIFI_FEC_SYMBOL_SIZE), nbytes);

        symbol_table[esi] = frame.symbol;

        crcs[esi] = crc32(frame.symbol, DXWIFI_FEC_SYMBOL_SIZE);

        msglen -= nbytes;
    }

    // Build repair symbols and calculate CRCs
    for(size_t esi = encoder.k; esi < encoder.n; ++esi) {
        symbol_table[esi] = offset(ldpc_encoded_msg, esi, stride) + sizeof(dxwifi_oti);
        status = of_build_repair_symbol(encoder.openfec_session, symbol_table, esi);
        assert_continue(status == OF_STATUS_OK, "Failed to build repair symbol. esi=%d", esi);

        crcs[esi] = crc32(symbol_table[esi], DXWIFI_FEC_SYMBOL_SIZE);
    }


    void* rs_ldpc_encoded_msg = calloc(encoder.n, DXWIFI_RS_LDPC_FRAME_SIZE);
    log_fatal("%d", DXWIFI_RS_LDPC_FRAME_SIZE);

    initialize_ecc();

    // Fill out OTI headers
    for(size_t esi = 0; esi < encoder.n; ++esi) {
        dxwifi_ldpc_frame ldpc_frame = make_ldpc_frame(ldpc_encoded_msg, esi);
        ldpc_frame.oti->esi          = htonl(esi);
        ldpc_frame.oti->n            = htonl(encoder.n);
        ldpc_frame.oti->k            = htonl(encoder.k);
        ldpc_frame.oti->crc          = htonl(crcs[esi]);

        void* rs_ldpc_frame = offset(rs_ldpc_encoded_msg, esi, DXWIFI_RS_LDPC_FRAME_SIZE);
        for(size_t i = 0; i < DXWIFI_RSCODE_BLOCKS_PER_FRAME; ++i) {
            void* message  = offset(ldpc_frame.oti, i, RSCODE_MAX_MSG_LEN);
            void* codeword = offset(rs_ldpc_frame, i, RSCODE_MAX_LEN);

            encode_data(message, RSCODE_MAX_MSG_LEN, codeword);
        }
        log_ldpc_data_frame(&ldpc_frame);
        log_hexdump(rs_ldpc_frame, DXWIFI_RS_LDPC_FRAME_SIZE);
    }

    *out = rs_ldpc_encoded_msg;

    free(ldpc_encoded_msg);

    close_encoder(&encoder);
    return encoder.n * stride;
}


size_t dxwifi_decode(void* encoded_msg, size_t msglen, void** out) {
    debug_assert(encoded_msg && out);

    // TODO we should search for a valid OTI in the encoded message instead of assuming that the first oti is valid
    dxwifi_oti* oti      = encoded_msg;
    uint32_t esi         = ntohl(oti->esi);
    uint32_t n           = ntohl(oti->n);
    uint32_t k           = ntohl(oti->k);
    //uint32_t crc         = ntohl(oti->crc);

    log_fatal("esi=%d, n=%d, k=%d", esi, n, k);

    void* ldpc_encoded_msg = calloc(n, DXWIFI_LDPC_FRAME_SIZE);

    if( msglen % DXWIFI_RS_LDPC_FRAME_SIZE != 0) {
        log_warning("Message length is not divisible by the size of the RS-LDPC frame");
    }

    size_t nframes = msglen / DXWIFI_RS_LDPC_FRAME_SIZE;
    for(size_t esi = 0; esi < nframes; ++esi) {
        void* rs_ldpc_frame = offset(encoded_msg, esi, DXWIFI_RS_LDPC_FRAME_SIZE);
        dxwifi_ldpc_frame ldpc_frame = make_ldpc_frame(ldpc_encoded_msg, esi);

        for(size_t i = 0; i < DXWIFI_RSCODE_BLOCKS_PER_FRAME; ++i) {
            void* message  = offset(ldpc_frame.oti, i, RSCODE_MAX_MSG_LEN);
            void* codeword = offset(rs_ldpc_frame, i, RSCODE_MAX_LEN);

            decode_data(codeword, RSCODE_MAX_LEN);
            if(check_syndrome() != 0) {
                correct_errors_erasures(codeword, RSCODE_MAX_LEN, 0, NULL);
            }
            memcpy(message, codeword, RSCODE_MAX_MSG_LEN);
        }
        log_ldpc_data_frame(&ldpc_frame);
        log_hexdump(rs_ldpc_frame, DXWIFI_RS_LDPC_FRAME_SIZE);
    }

    of_ldpc_parameters_t codec_params = {
        .nb_source_symbols      = k,
        .nb_repair_symbols      = n - k,
        .encoding_symbol_length = DXWIFI_FEC_SYMBOL_SIZE,
        .prng_seed              = rand(),
        .N1                     = (n-k) > DXWIFI_LDPC_N1_MAX ? DXWIFI_LDPC_N1_MAX : (n-k)
    };
    of_session_t* openfec_session = NULL;
    of_status_t status = OF_STATUS_OK;

    log_info("N1=%d, PRNG Seed=%d", codec_params.N1, codec_params.prng_seed);

    status = of_create_codec_instance(&openfec_session, OF_CODEC_LDPC_STAIRCASE_STABLE, OF_DECODER, 2);
    assert_M(status == OF_STATUS_OK, "Failed to initialize OpenFEC session");

    status = of_set_fec_parameters(openfec_session, (of_parameters_t*) &codec_params);
    assert_M(status == OF_STATUS_OK, "Failed to set codec parameters");

    void* symbol_table[n];

    for (size_t i = 0; i < nframes; ++i) 
    {
        dxwifi_ldpc_frame frame = make_ldpc_frame(ldpc_encoded_msg, i);
        of_decode_with_new_symbol(openfec_session, frame.symbol, ntohl(frame.oti->esi));
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
