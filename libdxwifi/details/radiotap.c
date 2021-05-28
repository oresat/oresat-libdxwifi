/**
 *  radiotap.c
 * 
 *  DESCRIPTION: Port of github.com/torvalds/linux/blob/master/net/wireless/radiotap.c 
 *  Original authors Andy Green <andy@warmcat.com> & Johannes Berg <johannes@sipsolutions.net>
 *  Redistributed under GPL v2.
 * 
 *  TODO: ADD COMMENTS! (Pull from linux/blob/master/net/wireless/radiotap.c and modify as needed)
 *  NOTE: Some fields have been renamed for readability.
 *  https://github.com/oresat/oresat-dxwifi-software
 * 
 */

#include <libdxwifi/details/radiotap.h>
#include <libdxwifi/details/ieee80211.h>

//Necessary due to multibyte fields.
#pragma GCC diagnostic ignored "-Waddress-of-packed-member"

//Note, Aligns are defined in radiotap.org/fields/defined AND radiotap.org/fields/suggested
//Align is given in each field's description
//Size is derived from the nubmer of bytes that the field's structure contains.
static const struct radiotap_align_size radiotap_namespace_sizes[] ={
	[IEEE80211_RADIOTAP_TSFT]              = {.align = 8, .size = 8, },
	[IEEE80211_RADIOTAP_FLAGS]             = {.align = 1, .size = 1, },
	[IEEE80211_RADIOTAP_RATE]              = {.align = 1, .size = 1, },
	[IEEE80211_RADIOTAP_CHANNEL]           = {.align = 2, .size = 4, },
	[IEEE80211_RADIOTAP_FHSS]              = {.align = 2, .size = 2, },
	[IEEE80211_RADIOTAP_DBM_ANTSIGNAL]     = {.align = 1, .size = 1, },
	[IEEE80211_RADIOTAP_DBM_ANTNOISE]      = {.align = 1, .size = 1, },
	[IEEE80211_RADIOTAP_LOCK_QUALITY]      = {.align = 2, .size = 2, },
	[IEEE80211_RADIOTAP_TX_ATTENUATION]    = {.align = 2, .size = 2, },
	[IEEE80211_RADIOTAP_DB_TX_ATTENUATION] = {.align = 2, .size = 2, },
	[IEEE80211_RADIOTAP_DBM_TX_POWER]      = {.align = 1, .size = 1, },
	[IEEE80211_RADIOTAP_ANTENNA]           = {.align = 1, .size = 1, },
	[IEEE80211_RADIOTAP_DB_ANTSIGNAL]      = {.align = 1, .size = 1, },
	[IEEE80211_RADIOTAP_DB_ANTNOISE]       = {.align = 1, .size = 1, },
	[IEEE80211_RADIOTAP_RX_FLAGS]          = {.align = 2, .size = 2, },
	[IEEE80211_RADIOTAP_TX_FLAGS]          = {.align = 2, .size = 2, },
	[IEEE80211_RADIOTAP_RTS_RETRIES]       = {.align = 1, .size = 1, },
	[IEEE80211_RADIOTAP_DATA_RETRIES]      = {.align = 1, .size = 1, },
	[IEEE80211_RADIOTAP_MCS]               = {.align = 1, .size = 3, },
	[IEEE80211_RADIOTAP_AMPDU_STATUS]      = {.align = 4, .size = 8, },
	[IEEE80211_RADIOTAP_VHT]               = {.align = 2, .size = 12, },
    /* Additional Fields for ORESAT */
	[IEEE80211_RADIOTAP_TIMESTAMP]         = {.align = 8, .size =12, },
	[IEEE80211_RADIOTAP_HE]                = {.align = 2, .size =12, },
	[IEEE80211_RADIOTAP_HE_MU]             = {.align = 2, .size =12, },
    [IEEE80211_RADIOTAP_ZERO_LEN_PSDU]     = {.align = 1, .size =1, }, //May not be correct.  radiotap.org/fields/0-length-PSDU states no required align.
    [IEEE80211_RADIOTAP_LSIG]              = {.align = 2, .size =4, },
};

static const struct radiotap_namespace current_radiotap_namespace = {
    .n_bits = sizeof(radiotap_namespace_sizes)/sizeof((radiotap_namespace_sizes)[0]), //Yanked from kernel.h
    .align_size = radiotap_namespace_sizes,
};

/**
 *  DESCRIPTION:  Initialize radiotap iterator   
 * 
 *  ARGUMENTS:
 *   iterator of type radiotap_iterator
 *   struct of type radiotap_header (to parse)
 *   int max_length (total packet length)
 *   struct of type radiotap_vendor_namespace
 * 
 *  RETURNS:
 *  0 if init success, -EINVAL if something broke.
 *       
 * 
 */
int radiotap_iterator_initialize(struct radiotap_iterator *iterator, struct ieee80211_radiotap_header *current_header, uint64_t max_length, const struct radiotap_vendor_namespace *vendor){
	//Failure case handling
	if(max_length < sizeof(struct ieee80211_radiotap_header)) { return -EINVAL; }
	if(current_header -> it_version != 0) { return -EINVAL; }
	if(max_length > get_unaligned_le16(&current_header -> it_len)) { return -EINVAL; }

	iterator -> header = current_header;
	iterator -> max_length = get_unaligned_le16(&current_header -> it_len);
	iterator -> arg_index = 0;
	iterator -> bitmap_shifter = get_unaligned_le32(&current_header -> it_present);
	iterator -> arg = (uint8_t *)current_header + sizeof(current_header);
	iterator -> reset_on_ext = 0;
	iterator -> next_bitmap = &current_header -> it_present;
	iterator -> next_bitmap++;
	iterator -> vendor_namespace = vendor;
	iterator -> current_namespace = &current_radiotap_namespace;
	iterator -> is_radiotap_namespace = 1;

	if(iterator -> bitmap_shifter & (1 << IEEE80211_RADIOTAP_EXT)){
		if((unsigned long)iterator -> arg - (unsigned long)iterator -> header + sizeof(uint32_t) > (unsigned long)iterator -> max_length) { return -EINVAL; }
		while(get_unaligned_le32(iterator -> arg) & 1 << IEEE80211_RADIOTAP_EXT) {
			iterator -> arg += sizeof(uint32_t);
            //Failure case when Bitmaps > Header Len
			if((unsigned long)iterator -> arg - (unsigned long)iterator -> header + sizeof(uint32_t) > (unsigned long)iterator -> max_length) { return -EINVAL; }
		}
		iterator -> arg += sizeof(uint32_t);
	}
	iterator -> this_arg = iterator -> arg;
    //Initialized successfully.
	return 0;
}

/**
 *  DESCRIPTION:    
 *                  
 * 
 *  ARGUMENTS:
 *   iterator of type radiotap_iterator 
 *   oui of type uint32
 *   subns of type uint8
 * 
 *  RETURNS:
 *  Nothing.
 */
static void find_ns(struct radiotap_iterator *iterator, uint32_t oui, uint8_t subns){
	int index;
	iterator -> current_namespace = NULL;
	if(!iterator -> vendor_namespace) { return; }
	for(index = 0; index < iterator -> vendor_namespace -> n_ns; index++){
		if(iterator -> vendor_namespace -> ns[index].oui != oui) { continue; }
		if(iterator -> vendor_namespace -> ns[index].subns != subns) {continue; }
		iterator -> current_namespace = &iterator -> vendor_namespace -> ns[index];
		break;
	}
}

/**
 *  DESCRIPTION:  Returns next radiotap parser iterator argument
 *  
 *  ARGUMENTS: 
 *  iterator of type radiotap_iterator
 * 
 *  RETURNS:
 *  0 if no arguments to handle
 *  -ENOINT if no more args
 *  -EINVAL if something went wrong
 * 
 *  WARNING: 
 *  When dereferencing the this_arg field for multibyte types, the pointer will NOT be aligned.
 *  Use get_unaligned((type*)iterator.this_arg) to dereference safely.
 */
int radiotap_iterator_next(struct radiotap_iterator *iterator){
	while(1) {
		int hit = 0;
		int pad, align, size;
		uint8_t subns;
		uint32_t oui;
		
		if((iterator -> arg_index % 32) == IEEE80211_RADIOTAP_EXT && !(iterator -> bitmap_shifter & 1)){ return -ENOENT; }       
		if(!(iterator -> bitmap_shifter & 1)) {
            //Replacement for GOTO    
            iterator -> bitmap_shifter >>= 1;
			iterator -> arg_index++;
            //END
        }

		switch(iterator -> arg_index % 32)
		{
			case IEEE80211_RADIOTAP_RADIOTAP_NAMESPACE:
			case IEEE80211_RADIOTAP_EXT:
			align = 1;
			size = 0;
			break;
			case IEEE80211_RADIOTAP_VENDOR_NAMESPACE:
			align = 2;
			size = 6;
			break;
			default:
			if(!iterator -> current_namespace || iterator -> arg_index >= iterator -> current_namespace -> n_bits) {
				if(iterator -> current_namespace == &current_radiotap_namespace) { return -ENOENT; }
				align = 0;
			}
			else {
				align = iterator -> current_namespace -> align_size[iterator -> arg_index].align;
				size = iterator -> current_namespace -> align_size[iterator -> arg_index].size;
			}
			if(!align){
				iterator -> arg = iterator -> next_ns_data;
				iterator -> current_namespace = NULL;
				//Replacement for GOTO
                iterator -> bitmap_shifter >>= 1;
			    iterator -> arg_index++;
                //END
			}
			break;
		}
		pad = ((unsigned long)iterator -> arg - (unsigned long)iterator -> header) & (align - 1);
		if(pad){
			iterator->arg += align - pad;
		}
		if(iterator->arg_index % 32 == IEEE80211_RADIOTAP_VENDOR_NAMESPACE){
			int length_vendor_namespace;
			if((unsigned long)iterator -> arg + size - (unsigned long)iterator -> header > (unsigned long)iterator -> max_length) { return -EINVAL; }

			oui = (*iterator -> arg << 16) | (*(iterator -> arg + 1) << 8) | *(iterator -> arg + 2);
			subns = *(iterator -> arg +3);
			find_ns(iterator, oui, subns);
			length_vendor_namespace = get_unaligned_le16(iterator -> arg + 4);
			if(!iterator -> current_namespace){
				size += length_vendor_namespace;
			}
		}
		iterator -> this_arg_index = iterator -> arg_index;
		iterator -> this_arg = iterator -> arg;
		iterator -> this_arg_size = size;

		iterator -> arg += size;

		if((unsigned long)iterator -> arg - (unsigned long) iterator -> header > (unsigned long)iterator -> max_length) { return -EINVAL; }
		switch (iterator -> arg_index % 32)
		{
			case IEEE80211_RADIOTAP_VENDOR_NAMESPACE:
			    iterator -> reset_on_ext = 1;
			    iterator -> is_radiotap_namespace = 0;
			    iterator -> this_arg_index = IEEE80211_RADIOTAP_VENDOR_NAMESPACE;
			    if(!iterator -> current_namespace){
				    hit = 1;
			    }
			    //Replacement for GOTO
                iterator -> bitmap_shifter >>= 1;
			    iterator -> arg_index++;
                break;
                //END
			case IEEE80211_RADIOTAP_RADIOTAP_NAMESPACE:
			    iterator -> reset_on_ext = 1;
			    iterator -> current_namespace = &current_radiotap_namespace;
			    iterator -> is_radiotap_namespace = 1;
                //Replacement for GOTO
                iterator -> bitmap_shifter >>= 1;
			    iterator -> arg_index++;
                break;
                //END
			case IEEE80211_RADIOTAP_EXT:
			    iterator -> bitmap_shifter = get_unaligned_le32(iterator -> next_bitmap);
			    iterator -> next_bitmap++;
			    if(iterator -> reset_on_ext){
				    iterator -> arg_index = 0;
			    }
			    else{
				    iterator -> arg_index++;
			    }
			    iterator -> reset_on_ext = 0;
                break;
			default:
			    hit = 1;
                //Original location of GOTO endpoint.
			    iterator -> bitmap_shifter >>= 1;
			    iterator -> arg_index++;
                break;
		}
            //return valid arg if found
		if(hit) return 0;
	}       
}

/**
 *  DESCRIPTION:  Run Parser.
 *  
 *  ARGUMENTS: 
 *  struct of type radiotap_header_data
 * 
 *  RETURNS:
 *  0 if no arguments to handle
 *  -ENOINT if no more args
 *  -EINVAL if something went wrong
 * 
 */
int run_parser(struct radiotap_header_data *data_out){
	struct radiotap_iterator * iterator;
	struct ieee80211_radiotap_header * header;
	uint64_t max_len = sizeof(header);
	int return_value = radiotap_iterator_initialize(iterator, header, max_len, iterator->vendor_namespace);
	uint8_t buffer = 0;
    int buffer_length = sizeof(iterator);
    
    while(buffer_length > 0){
		while(!return_value){
			return_value = radiotap_iterator_next(iterator);
			if(return_value){
				continue;
			}
        //NOTE, inside these case statements, get_unaligned(type *)iterator->this_arg MUST be used for multibyte data fields.
			switch(iterator->this_arg_index){
				case IEEE80211_RADIOTAP_TSFT:
				data_out->TSFT = get_unaligned_le64((void *)iterator -> this_arg);
				break;
				case IEEE80211_RADIOTAP_FLAGS:
				data_out->Flags = *iterator -> this_arg;
				break;
				case IEEE80211_RADIOTAP_RATE:
				data_out->Rate = (*iterator -> this_arg) * 5;
				break;
				case IEEE80211_RADIOTAP_CHANNEL:
                //Note, this may not work correctly.
                //data_out->ChannelFreq  = get_unaligned_le16((void *)iterator -> this_arg);
                //data_out->ChannelFlags = get_unaligned_le16((void*)iterator -> this_arg);
				break;
				case IEEE80211_RADIOTAP_FHSS:
               // data_out->FHSS_hop_set = ???; //TODO FIGURE THIS OUT
               // data_out->FHSS_hop_patt = ???; //TODO FIGURE THIS OUT
				break;
				case IEEE80211_RADIOTAP_DBM_ANTSIGNAL:
				data_out->dBm_AntSignal = *iterator -> this_arg;
				break;
				case IEEE80211_RADIOTAP_DBM_ANTNOISE:
				data_out->dBm_AntNoise = *iterator -> this_arg;
				break;
				case IEEE80211_RADIOTAP_LOCK_QUALITY:
				data_out->LockQuality = get_unaligned_le16((void*)iterator -> this_arg);
				break;
				case IEEE80211_RADIOTAP_TX_ATTENUATION:
				data_out->TX_Attenuation = get_unaligned_le16((void*)iterator -> this_arg);
				break;
				case IEEE80211_RADIOTAP_DB_TX_ATTENUATION:
				data_out->dB_TX_Attenuation = get_unaligned_le16((void *)iterator -> this_arg);
				break;
				case IEEE80211_RADIOTAP_DBM_TX_POWER:
				data_out->dBm_Tx_Power = *iterator -> this_arg;
				break;
				case IEEE80211_RADIOTAP_ANTENNA:
				data_out->Antenna = *iterator -> this_arg;
				break;
				case IEEE80211_RADIOTAP_DB_ANTSIGNAL:
				data_out->db_AntSignal = *iterator -> this_arg;
				break;
				case IEEE80211_RADIOTAP_DB_ANTNOISE:
				data_out->db_AntNoise = *iterator -> this_arg;
				break;
				case IEEE80211_RADIOTAP_RX_FLAGS:
				data_out->Rx_Flags = get_unaligned_le16((void *)iterator -> this_arg);
				break;
				case IEEE80211_RADIOTAP_TX_FLAGS:
				data_out->Tx_Flags = get_unaligned_le16((void *)iterator -> this_arg);
				break;
				case IEEE80211_RADIOTAP_RTS_RETRIES:
				data_out->RTS_Retries = *iterator -> this_arg;
				break;
				case IEEE80211_RADIOTAP_DATA_RETRIES:
				data_out->Data_Retries = *iterator -> this_arg;
				break;
				case IEEE80211_RADIOTAP_MCS:
            //To Do
				break;
				case IEEE80211_RADIOTAP_AMPDU_STATUS:
            //To Do
				break;
				case IEEE80211_RADIOTAP_VHT:
            //To Do
				break;
				case IEEE80211_RADIOTAP_TIMESTAMP:
            //To Do
				break;
				case IEEE80211_RADIOTAP_HE:
            //To Do
				break;
				case IEEE80211_RADIOTAP_HE_MU:
            //To Do
				break;
				case IEEE80211_RADIOTAP_ZERO_LEN_PSDU:
				data_out->ZERO_LEN_PSDU = *iterator -> this_arg;
				break;
				case IEEE80211_RADIOTAP_LSIG:
            //To Do
				break;
				default:
				break;
			}
		}
        //While there's more headers
		if(return_value != -ENOENT){
            free(iterator);
            free(header);
            return 0; //insert value here to return.  Note to self, ask Alex.
        }
        //discard current header part and continue
        buffer += iterator->max_length;
        buffer_length -= iterator->max_length;
    }
    return 0;
}