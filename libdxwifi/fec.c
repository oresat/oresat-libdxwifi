/**
 *  fec.c - See fec.h for description
 *
 *  https://github.com/oresat/oresat-dxwifi-software
 */

#include <math.h>

#include <arpa/inet.h>

#include <rscode/ecc.h>
#include <of_openfec_api.h>
#include <ldpc_staircase/of_codec_profile.h>

#include <libdxwifi/fec.h>
#include <libdxwifi/details/utils.h>
#include <libdxwifi/details/crc32.h>
#include <libdxwifi/details/assert.h>
#include <libdxwifi/details/logging.h>

// Max number of symbols that OpenFEC can handle, 50000
#define OFEC_MAX_SYMBOLS OF_LDPC_STAIRCASE_MAX_NB_ENCODING_SYMBOLS_DEFAULT

// TODO Add function comments

static void log_codec_params(const of_ldpc_parameters_t* params) {
    log_info(
        "DxWiFi Codec\n"
        "\tK:           %d\n"
        "\tN-K:         %d\n"
        "\tN1:          %d\n"
        "\tPRNG Seed:   %d\n",
        params->nb_source_symbols,
        params->nb_repair_symbols,
        params->N1,
        params->prng_seed
    );
}


static void log_ldpc_data_frame(dxwifi_ldpc_frame* frame) {
    log_debug("(LDPC Frame) ESI: %u, CRC: 0x%x", ntohl(frame->oti.esi), ntohl(frame->oti.crc));
    log_hexdump((uint8_t*)frame, sizeof(dxwifi_ldpc_frame));
}


static void log_rs_ldpc_data_frame(dxwifi_rs_ldpc_frame* frame) {
    log_debug("(RS-LDPC Frame)");
    log_hexdump((uint8_t*)frame, sizeof(dxwifi_rs_ldpc_frame));
}


// Caculate N,K values, initialize openfec session
static of_session_t* init_openfec(uint32_t n, uint32_t k) {
    of_status_t status = OF_STATUS_OK;

    of_session_t* openfec_session = NULL;

    of_ldpc_parameters_t codec_params = {
        .nb_source_symbols      = k,
        .nb_repair_symbols      = n - k,
        .encoding_symbol_length = DXWIFI_FEC_SYMBOL_SIZE,
        .prng_seed              = rand(), // TODO no seed for rand(), does this need to match the decoder?
        .N1                     = (n-k) > DXWIFI_LDPC_N1_MAX ? DXWIFI_LDPC_N1_MAX : (n-k) 
    };
    log_codec_params(&codec_params);

    if(codec_params.N1 >= DXWIFI_LDPC_N1_MIN) {
        status = of_create_codec_instance(&openfec_session, OF_CODEC_LDPC_STAIRCASE_STABLE, OF_ENCODER, 2); // TODO magic number
        assert_M(status == OF_STATUS_OK, "Failed to initialize OpenFEC session");

        status = of_set_fec_parameters(openfec_session, (of_parameters_t*) &codec_params);
        assert_M(status == OF_STATUS_OK, "Failed to set codec parameters");
    }
    return openfec_session;
}


//
// See fec.h for non-static function descriptions
//

const char* dxwifi_fec_error_to_str(dxwifi_fec_error_t err) {
    switch (err)
    {
    case FEC_ERROR_EXCEEDED_MAX_SYMBOLS:
        return "N exceeds maximum number of symbols. Possible solution, decrease the coderate";

    case FEC_ERROR_BELOW_N1_MIN:
        return "N - K is below the N1 minimum. Possible solution, increase the coderate";

    case FEC_ERROR_NO_OTI_FOUND:
        return "No OTI Header found in the encoded message.";

    case FEC_ERROR_DECODE_NOT_POSSIBLE:
        return "Decode failed, not enough repair symbols";
    
    default:
        return "Unknown error";
    }
}

// TODO refactor the individual algorithms of the encode routine into seperate 
// functions
ssize_t dxwifi_encode(void* message, size_t msglen, float coderate, void** out) {
    debug_assert(message && out);
    debug_assert(0.0 < coderate && coderate <= 1.0);

    uint32_t k = ceil((float) msglen / DXWIFI_FEC_SYMBOL_SIZE);
    uint32_t n = k / coderate;  

    //check if N > Max Symbols
    if(n > OFEC_MAX_SYMBOLS) {
        return FEC_ERROR_EXCEEDED_MAX_SYMBOLS;
    }
    
    of_session_t* openfec_session = init_openfec(n, k);

    if(!openfec_session) {
        return FEC_ERROR_BELOW_N1_MIN;
    }

    dxwifi_ldpc_frame* ldpc_frames = calloc(n, sizeof(dxwifi_ldpc_frame));
    assert_M(ldpc_frames, "Failed to allocate memory for LDPC Frames");

    // Setup symbol table and CRCs
    uint32_t crcs[n];
    void* symbol_table[n];

    // Load source symbols into symbol table, and calculate CRCs
    for(size_t esi = 0; esi < k; ++esi) { 
        dxwifi_ldpc_frame* frame = &ldpc_frames[esi];

        // Copy symbol size bytes from message, or whatevers left over
        size_t nbytes = DXWIFI_FEC_SYMBOL_SIZE < msglen ? DXWIFI_FEC_SYMBOL_SIZE : msglen;
        memcpy(frame->symbol, offset(message, esi, DXWIFI_FEC_SYMBOL_SIZE), nbytes);

        symbol_table[esi] = frame->symbol;

        crcs[esi] = crc32(frame->symbol, DXWIFI_FEC_SYMBOL_SIZE);

        msglen -= nbytes;
    }

    // Build repair symbols and calculate CRCs
    of_status_t status = OF_STATUS_OK;
    for(size_t esi = k; esi < n; ++esi) {
        dxwifi_ldpc_frame* frame = &ldpc_frames[esi];
        symbol_table[esi] = frame->symbol;

        status = of_build_repair_symbol(openfec_session, symbol_table, esi);
        assert_continue(status == OF_STATUS_OK, "Failed to build repair symbol. esi=%d", esi);

        crcs[esi] = crc32(symbol_table[esi], DXWIFI_FEC_SYMBOL_SIZE);
    }

    initialize_ecc();

    dxwifi_rs_ldpc_frame* rs_ldpc_frames = calloc(n, sizeof(dxwifi_rs_ldpc_frame));

    // Fill out OTI headers and RS Encode each LDPC Frame
    for(size_t esi = 0; esi < n; ++esi) {
        dxwifi_ldpc_frame* ldpc_frame = &ldpc_frames[esi];
        ldpc_frame->oti.esi           = htonl(esi);
        ldpc_frame->oti.n             = htonl(n);
        ldpc_frame->oti.k             = htonl(k);
        ldpc_frame->oti.crc           = htonl(crcs[esi]);

        dxwifi_rs_ldpc_frame* rs_ldpc_frame = &rs_ldpc_frames[esi];
        for(size_t i = 0; i < DXWIFI_RSCODE_BLOCKS_PER_FRAME; ++i) {
            void* message  = offset(ldpc_frame, i, RSCODE_MAX_MSG_LEN);
            void* codeword = offset(rs_ldpc_frame, i, RSCODE_MAX_LEN);

            encode_data(message, RSCODE_MAX_MSG_LEN, codeword);
        }
        log_ldpc_data_frame(ldpc_frame);
        log_rs_ldpc_data_frame(rs_ldpc_frame);
    }

    *out = rs_ldpc_frames;

    free(ldpc_frames);

    of_release_codec_instance(openfec_session);
    return n * DXWIFI_RS_LDPC_FRAME_SIZE;
}

// TODO Refactor the individual algorithms of the decode routine into seperate
// functions. 
ssize_t dxwifi_decode(void* encoded_msg, size_t msglen, void** out) {
    debug_assert(encoded_msg && out);

    size_t nframes = msglen / DXWIFI_RS_LDPC_FRAME_SIZE;

    dxwifi_ldpc_frame* ldpc_frames = calloc(nframes, sizeof(dxwifi_ldpc_frame));

    if( msglen % DXWIFI_RS_LDPC_FRAME_SIZE != 0) {
        log_warning("Misaligned, msglen (%u) is not divisible by RS-LDPC frame size");
    }

    dxwifi_rs_ldpc_frame* rs_ldpc_frames = encoded_msg;

    // Decode outer RS shell and populate the LDPC Frames
    for(size_t i = 0; i < nframes; ++i) {

        dxwifi_rs_ldpc_frame* rs_ldpc_frame = &rs_ldpc_frames[i];
        dxwifi_ldpc_frame* ldpc_frame = &ldpc_frames[i];

        for(size_t i = 0; i < DXWIFI_RSCODE_BLOCKS_PER_FRAME; ++i) {
            void* message  = offset(ldpc_frame, i, RSCODE_MAX_MSG_LEN);
            void* codeword = offset(rs_ldpc_frame, i, RSCODE_MAX_LEN);

            decode_data(codeword, RSCODE_MAX_LEN);
            if(check_syndrome() != 0) {
                correct_errors_erasures(codeword, RSCODE_MAX_LEN, 0, NULL);
            }
            memcpy(message, codeword, RSCODE_MAX_MSG_LEN);
        }
        log_ldpc_data_frame(ldpc_frame);
        log_rs_ldpc_data_frame(rs_ldpc_frame);
    }

    // Search for first valid OTI header 
    size_t idx = 0;
    for(; idx < nframes; ++idx) {
    	dxwifi_ldpc_frame* frame = &ldpc_frames[idx];

    	uint32_t crc = crc32(frame->symbol, DXWIFI_FEC_SYMBOL_SIZE); 

    	if(crc == ntohl(frame->oti.crc)){
            break;
    	}
    	else { 
            log_warning("Frame %d CRC mistmatch, actual: 0x%x expected: 0x%x", idx, crc, ntohl(frame->oti.crc)); 
        }
	} 
    if(idx >= nframes){
        free(ldpc_frames);
        return FEC_ERROR_NO_OTI_FOUND;
	}

	dxwifi_oti* oti = &ldpc_frames[idx].oti;
    uint32_t esi    = ntohl(oti->esi);
    uint32_t n      = ntohl(oti->n);
    uint32_t k      = ntohl(oti->k);
    log_info("OTI Found: esi=%d, n=%d, k=%d", esi, n, k);

    of_ldpc_parameters_t codec_params = {
        .nb_source_symbols      = k,
        .nb_repair_symbols      = n - k,
        .encoding_symbol_length = DXWIFI_FEC_SYMBOL_SIZE,
        .prng_seed              = rand(),
        .N1                     = (n-k) > DXWIFI_LDPC_N1_MAX ? DXWIFI_LDPC_N1_MAX : (n-k)
    };
    of_session_t* openfec_session = NULL;
    of_status_t status = OF_STATUS_OK;
    log_codec_params(&codec_params);

    status = of_create_codec_instance(&openfec_session, OF_CODEC_LDPC_STAIRCASE_STABLE, OF_DECODER, 2);
    assert_M(status == OF_STATUS_OK, "Failed to initialize OpenFEC session");

    status = of_set_fec_parameters(openfec_session, (of_parameters_t*) &codec_params);
    assert_M(status == OF_STATUS_OK, "Failed to set codec parameters");

    // Decode LDPC Frames
    for (size_t i = 0; i < nframes; ++i) {
        dxwifi_ldpc_frame* frame = &ldpc_frames[i];

        uint32_t esi = ntohl(frame->oti.esi);
        if(esi > n) {
            log_debug("Invalid ESI: %u, N: %u", esi, n);
        } else {
            of_decode_with_new_symbol(openfec_session, frame->symbol, ntohl(frame->oti.esi));
        }
    }
    if(!of_is_decoding_complete(openfec_session)) {
        status = of_finish_decoding(openfec_session);
        if(status != OF_STATUS_OK) {
            free(ldpc_frames);
            return FEC_ERROR_DECODE_NOT_POSSIBLE;
        }
    }

    void* symbol_table[n];
    of_get_source_symbols_tab(openfec_session, symbol_table);

    // TODO if the original file size is not evenly divisible by DXWIFI_FEC_SYMBOL_SIZE then the Kth 
    // Symbol will have extra zeroes to pad it until it is of size DXWIFI_FEC_SYMBOL_SIZE. We will need 
    // to find a way to identify and remove the extra padding.

    // Copy out the decoded message
    void* decoded_msg = calloc(k, DXWIFI_FEC_SYMBOL_SIZE);
    for(size_t esi = 0; esi < k; ++esi) {
        void* symbol = offset(decoded_msg, esi, DXWIFI_FEC_SYMBOL_SIZE);
        memcpy(symbol, symbol_table[esi], DXWIFI_FEC_SYMBOL_SIZE);
    }
    *out = decoded_msg;

    free(ldpc_frames);

    return k * DXWIFI_FEC_SYMBOL_SIZE;
}
