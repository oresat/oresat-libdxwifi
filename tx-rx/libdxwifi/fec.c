/**
 *  fec.c - See fec.h for description
 *
 *  https://github.com/oresat/oresat-dxwifi-software
 *  TODO: Hook Conditional Compilation (DEBUG_LOGGING) into cmake
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


static void log_codec_params(const of_ldpc_parameters_t* params) {
    log_info(
        "DxWiFi Encoder\n"
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
    assert_M(codec_params.N1 >= DXWIFI_LDPC_N1_MIN, "N - K must be >= %d", DXWIFI_LDPC_N1_MIN);

    status = of_create_codec_instance(&openfec_session, OF_CODEC_LDPC_STAIRCASE_STABLE, OF_ENCODER, 2); // TODO magic number
    assert_M(status == OF_STATUS_OK, "Failed to initialize OpenFEC session");

    status = of_set_fec_parameters(openfec_session, (of_parameters_t*) &codec_params);
    assert_M(status == OF_STATUS_OK, "Failed to set codec parameters");

    return openfec_session;
}


// FEC encode the message
size_t dxwifi_encode(void* message, size_t msglen, float coderate, void** out) {
    debug_assert(message && out);
    debug_assert(0.0 < coderate && coderate <= 1.0);

    uint32_t k = ceil((float) msglen / DXWIFI_FEC_SYMBOL_SIZE);
    uint32_t n = k / coderate;  

    of_session_t* openfec_session = init_openfec(n, k);

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
        #ifdef DEBUG_LOGGING
        log_ldpc_data_frame(ldpc_frame);
        log_rs_ldpc_data_frame(rs_ldpc_frame);
        #endif
    }

    *out = rs_ldpc_frames;

    free(ldpc_frames);

    of_release_codec_instance(openfec_session);
    return n * DXWIFI_RS_LDPC_FRAME_SIZE;
}


size_t dxwifi_decode(void* encoded_msg, size_t msglen, void** out) {
    debug_assert(encoded_msg && out);
    //foundvalue used for crc checking, defaults to -1
    int foundvalue = -1;

    size_t nframes = msglen / DXWIFI_RS_LDPC_FRAME_SIZE;

    dxwifi_ldpc_frame* ldpc_frames = calloc(nframes, sizeof(dxwifi_ldpc_frame));

    if( msglen % DXWIFI_RS_LDPC_FRAME_SIZE != 0) {
        log_warning("Shouldnt msglen be divisible by the size of a RS-LDPC Data frame?");
    }

    // Cast encoded message as an array of RS-LDPC Frames
    dxwifi_rs_ldpc_frame* rs_ldpc_frames = encoded_msg;

    // Decode outer RS shell and populate the LDPC Frames
    for(size_t frame_no= 0; frame_no < nframes; ++frame_no) {
        dxwifi_rs_ldpc_frame* rs_ldpc_frame = &rs_ldpc_frames[frame_no];
        dxwifi_ldpc_frame* ldpc_frame = &ldpc_frames[frame_no];

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

    //search for first valid OTI header 
    // Start at Zero, End at total symbols (nframes), increment by 1.
    for(size_t oti_check = 0; oti_check < nframes; oti_check++) {
    	dxwifi_ldpc_frame* test_frame = &ldpc_frames[oti_check];
    	uint32_t crc = crc32(test_frame->symbol, DXWIFI_FEC_SYMBOL_SIZE); 
    	#ifdef DEBUG_LOGGING
    	log_debug("Test Frame CRC: 0x%x ", ntohl(test_frame->oti.crc));
    	log_debug("Calculated CRC: 0x%x \n", crc);
    	#endif
    	//Recompute the CRC value, and compare vs the one pulled from oti.crc
    	//if it's equal, log info that a valid one was found and return to standard execution.
    	if(crc == ntohl(test_frame->oti.crc)){
    		#ifdef DEBUG_LOGGING
    		log_debug("Valid OTI header found, returning to standard execution");
    		#endif
    		foundvalue = (int)oti_check;
    		break;
    	}
    	//Otherwise, log a warning for a CRC mismatch and continue forwards.
    	else { log_warning("CRC didn't match: Expected: 0x%x, Got: 0x%x", crc, ntohl(test_frame->oti.crc)); }
	} 
	//If no valid OTI headers were found (foundvalue still == -1), then exit the program with an error
	if(foundvalue == -1){
		//Log Fault and exit with error flag.
		log_warning("No valid OTI headers found.  Aborting program.");
		exit(1);
	}
	//Now actually decode the LDPC blocks
	dxwifi_oti* oti      = encoded_msg;
    uint32_t esi         = ntohl(oti->esi);
    uint32_t n           = ntohl(oti->n);
    uint32_t k           = ntohl(oti->k);
    //uint32_t crc         = ntohl(oti->crc);
    log_info("esi=%d, n=%d, k=%d", esi, n, k);


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

    // Decode LDPC Frames
    for (size_t i = 0; i < nframes; ++i) {
        dxwifi_ldpc_frame* frame = &ldpc_frames[i];
        of_decode_with_new_symbol(openfec_session, frame->symbol, ntohl(frame->oti.esi));
    }
    if(!of_is_decoding_complete(openfec_session)) {
        status = of_finish_decoding(openfec_session);
        if(status != OF_STATUS_OK) {
            log_fatal("Couldn't finish decoding");
            // TODO Asssert here? write out what we have? Can we recover from this?
        }
    }

    void* symbol_table[n];
    status = of_get_source_symbols_tab(openfec_session, symbol_table);
    if(status != OF_STATUS_OK) {
        log_fatal("Failed to get src symbols");
        // TODO Asssert here? write out what we have? Can we recover from this?
    }

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
