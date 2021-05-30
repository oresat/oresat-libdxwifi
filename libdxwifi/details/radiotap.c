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
#include <libdxwifi/details/assert.h>

//Radiotap Header's is_present field is aligned on a 4-byte boundary
//Radiotap Header itself is not aligned on 4 bytes.
//Compiler doesn't like this, and throws a warning.
//Pragma is used to override warning, as otherwise the iterator cannot initialize.
#pragma GCC diagnostic ignored "-Waddress-of-packed-member"

//Pragma used to fix paranoid compiler
//Error on: iterator->_arg += align - pad;
//However _arg defined previously as: iterator->_arg = (uint8_t *)radiotap_header + sizeof(*radiotap_header);
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"

//Note, Aligns are defined in radiotap.org/fields/defined AND radiotap.org/fields/suggested
//Align is given in each field's description
//Size is derived from the nubmer of bytes that the field's structure contains.
//These fields are the ones that the implemented pcap header contains.
static const struct radiotap_align_size rtap_namespace_sizes[] = {
	[IEEE80211_RADIOTAP_TSFT] = { .align = 8, .size = 8, },
	[IEEE80211_RADIOTAP_FLAGS] = { .align = 1, .size = 1, },
	[IEEE80211_RADIOTAP_RATE] = { .align = 1, .size = 1, },
	[IEEE80211_RADIOTAP_CHANNEL] = { .align = 2, .size = 4, },
	[IEEE80211_RADIOTAP_FHSS] = { .align = 2, .size = 2, },
	[IEEE80211_RADIOTAP_DBM_ANTSIGNAL] = { .align = 1, .size = 1, },
	[IEEE80211_RADIOTAP_DBM_ANTNOISE] = { .align = 1, .size = 1, },
	[IEEE80211_RADIOTAP_LOCK_QUALITY] = { .align = 2, .size = 2, },
	[IEEE80211_RADIOTAP_TX_ATTENUATION] = { .align = 2, .size = 2, },
	[IEEE80211_RADIOTAP_DB_TX_ATTENUATION] = { .align = 2, .size = 2, },
	[IEEE80211_RADIOTAP_DBM_TX_POWER] = { .align = 1, .size = 1, },
	[IEEE80211_RADIOTAP_ANTENNA] = { .align = 1, .size = 1, },
	[IEEE80211_RADIOTAP_DB_ANTSIGNAL] = { .align = 1, .size = 1, },
	[IEEE80211_RADIOTAP_DB_ANTNOISE] = { .align = 1, .size = 1, },
	[IEEE80211_RADIOTAP_RX_FLAGS] = { .align = 2, .size = 2, },
	[IEEE80211_RADIOTAP_TX_FLAGS] = { .align = 2, .size = 2, },
	[IEEE80211_RADIOTAP_RTS_RETRIES] = { .align = 1, .size = 1, },
	[IEEE80211_RADIOTAP_DATA_RETRIES] = { .align = 1, .size = 1, },
	[IEEE80211_RADIOTAP_MCS] = { .align = 1, .size = 3, },
	[IEEE80211_RADIOTAP_AMPDU_STATUS] = { .align = 4, .size = 8, },
	[IEEE80211_RADIOTAP_VHT] = { .align = 2, .size = 12, },
	/*
	 * add more here as they are defined in radiotap.h
	 */
};

//Rewritten to fix "function call not allowed in constant expression".
static const struct radiotap_namespace radiotap_ns = {
    .n_bits = sizeof(rtap_namespace_sizes)/sizeof((rtap_namespace_sizes)[0]), //Yanked from kernel.h
    .align_size = rtap_namespace_sizes,
};


uint16_t get_unaligned_le16 (const void *p){
    //From linux/unaligned/packed_struct.h (__get_unaligned_cpu_16)
    const struct __una_u16 * ptr = (const struct __una_u16 *)p;
    return ptr -> x;
}

uint32_t get_unaligned_le32(const void *p) {
    //From linux/unaligned/packed_struct.h (__get_unaligned_cpu_32)
    const struct __una_u32 * ptr = (const struct __una_u32 *)p;
    return ptr -> x;
}

/**
 * ieee80211_radiotap_iterator_init - radiotap parser iterator initialization
 * @iterator: radiotap_iterator to initialize
 * @radiotap_header: radiotap header to parse
 * @max_length: total length we can parse into (eg, whole packet length)
 * @vns: vendor namespaces to parse
 *
 * Returns: 0 or a negative error code if there is a problem.
 *
 * This function initializes an opaque iterator struct which can then
 * be passed to ieee80211_radiotap_iterator_next() to visit every radiotap
 * argument which is present in the header.  It knows about extended
 * present headers and handles them.
 *
 * How to use:
 * call __ieee80211_radiotap_iterator_init() to init a semi-opaque iterator
 * struct ieee80211_radiotap_iterator (no need to init the struct beforehand)
 * checking for a good 0 return code.  Then loop calling
 * __ieee80211_radiotap_iterator_next()... it returns either 0,
 * -ENOENT if there are no more args to parse, or -EINVAL if there is a problem.
 * The iterator's @this_arg member points to the start of the argument
 * associated with the current argument index that is present, which can be
 * found in the iterator's @this_arg_index member.  This arg index corresponds
 * to the IEEE80211_RADIOTAP_... defines.
 *
 * Radiotap header length:
 * You can find the CPU-endian total radiotap header length in
 * iterator->max_length after executing ieee80211_radiotap_iterator_init()
 * successfully.
 *
 * Alignment Gotcha:
 * You must take care when dereferencing iterator.this_arg
 * for multibyte types... the pointer is not aligned.  Use
 * get_unaligned((type *)iterator.this_arg) to dereference
 * iterator.this_arg for type "type" safely on all arches.
 *
 * Example code:
 * See Documentation/networking/radiotap-headers.rst
 */

int ieee80211_radiotap_iterator_init(
	struct ieee80211_radiotap_iterator *iterator,
	struct ieee80211_radiotap_header *radiotap_header,
	long unsigned int max_length, const struct radiotap_vendor_namespace *vns)
{
	/* check the radiotap header can actually be present */
	if (max_length < sizeof(struct ieee80211_radiotap_header))
		return -EINVAL;

	/* Linux only supports version 0 radiotap format */
	if (radiotap_header->it_version)
		return -EINVAL;

	/* sanity check for allowed length and radiotap length field */
	if (max_length < get_unaligned_le16(&radiotap_header->it_len))
		return -EINVAL;

	iterator->_rtheader = radiotap_header;
	iterator->_max_length = get_unaligned_le16(&radiotap_header->it_len);
	iterator->_arg_index = 0;
	iterator->_bitmap_shifter = get_unaligned_le32(&radiotap_header->it_present);
	iterator->_arg = (uint8_t *)radiotap_header + sizeof(*radiotap_header);
	iterator->_reset_on_ext = 0;
	iterator->_next_bitmap = (&radiotap_header->it_present);
	iterator->_next_bitmap++;
	iterator->_vns = vns;
	iterator->current_namespace = &radiotap_ns;
	iterator->is_radiotap_ns = 1;

	/* find payload start allowing for extended bitmap(s) */

	if (iterator->_bitmap_shifter & (1<<IEEE80211_RADIOTAP_EXT)) {
		if ((unsigned long)iterator->_arg -
		    (unsigned long)iterator->_rtheader + sizeof(uint32_t) >
		    (unsigned long)iterator->_max_length)
			return -EINVAL;
		while (get_unaligned_le32(iterator->_arg) &
					(1 << IEEE80211_RADIOTAP_EXT)) {
			iterator->_arg += sizeof(uint32_t);

			/*
			 * check for insanity where the present bitmaps
			 * keep claiming to extend up to or even beyond the
			 * stated radiotap header length
			 */

			if ((unsigned long)iterator->_arg -
			    (unsigned long)iterator->_rtheader +
			    sizeof(uint32_t) >
			    (unsigned long)iterator->_max_length)
				return -EINVAL;
		}

		iterator->_arg += sizeof(uint32_t);

		/*
		 * no need to check again for blowing past stated radiotap
		 * header length, because ieee80211_radiotap_iterator_next
		 * checks it before it is dereferenced
		 */
	}

	iterator->_this_arg = iterator->_arg;

	/* we are all initialized happily */

	return 0;
}

static void find_ns(struct ieee80211_radiotap_iterator *iterator,
		    uint32_t oui, uint8_t subns)
{
	int i;

	iterator->current_namespace = NULL;

	if (!iterator->_vns)
		return;

	for (i = 0; i < iterator->_vns->n_ns; i++) {
		if (iterator->_vns->ns[i].oui != oui)
			continue;
		if (iterator->_vns->ns[i].subns != subns)
			continue;

		iterator->current_namespace = &iterator->_vns->ns[i];
		break;
	}
}



/**
 * ieee80211_radiotap_iterator_next - return next radiotap parser iterator arg
 * @iterator: radiotap_iterator to move to next arg (if any)
 *
 * Returns: 0 if there is an argument to handle,
 * -ENOENT if there are no more args or -EINVAL
 * if there is something else wrong.
 *
 * This function provides the next radiotap arg index (IEEE80211_RADIOTAP_*)
 * in @this_arg_index and sets @this_arg to point to the
 * payload for the field.  It takes care of alignment handling and extended
 * present fields.  @this_arg can be changed by the caller (eg,
 * incremented to move inside a compound argument like
 * IEEE80211_RADIOTAP_CHANNEL).  The args pointed to are in
 * little-endian format whatever the endianess of your CPU.
 *
 * Alignment Gotcha:
 * You must take care when dereferencing iterator.this_arg
 * for multibyte types... the pointer is not aligned.  Use
 * get_unaligned((type *)iterator.this_arg) to dereference
 * iterator.this_arg for type "type" safely on all arches.
 */

int ieee80211_radiotap_iterator_next(
	struct ieee80211_radiotap_iterator *iterator)
{
	while (1) {
		int hit = 0;
		int pad, align, size, subns;
		uint32_t oui;

		/* if no more EXT bits, that's it */
		if ((iterator->_arg_index % 32) == IEEE80211_RADIOTAP_EXT &&
		    !(iterator->_bitmap_shifter & 1))
			return -ENOENT;

		if (!(iterator->_bitmap_shifter & 1))
			goto next_entry; /* arg not present */

		/* get alignment/size of data */
		switch (iterator->_arg_index % 32) {
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
			if (!iterator->current_namespace ||
			    iterator->_arg_index >= iterator->current_namespace->n_bits) {
				if (iterator->current_namespace == &radiotap_ns)
					return -ENOENT;
				align = 0;
			} else {
				align = iterator->current_namespace->align_size[iterator->_arg_index].align;
				size = iterator->current_namespace->align_size[iterator->_arg_index].size;
			}
			if (!align) {
				/* skip all subsequent data */
				iterator->_arg = iterator->_next_ns_data;
				/* give up on this namespace */
				iterator->current_namespace = NULL;
				goto next_entry;
			}
			break;
		}

		/*
		 * arg is present, account for alignment padding
		 *
		 * Note that these alignments are relative to the start
		 * of the radiotap header.  There is no guarantee
		 * that the radiotap header itself is aligned on any
		 * kind of boundary.
		 *
		 * The above is why get_unaligned() is used to dereference
		 * multibyte elements from the radiotap area.
		 */

		pad = ((unsigned long)iterator->_arg -
		       (unsigned long)iterator->_rtheader) & (align - 1);

		if (pad)
			iterator->_arg += align - pad;

		if (iterator->_arg_index % 32 == IEEE80211_RADIOTAP_VENDOR_NAMESPACE) {
			int vnslen;

			if ((unsigned long)iterator->_arg + size -
			    (unsigned long)iterator->_rtheader >
			    (unsigned long)iterator->_max_length)
				return -EINVAL;

			oui = (*iterator->_arg << 16) |
				(*(iterator->_arg + 1) << 8) |
				*(iterator->_arg + 2);
			subns = *(iterator->_arg + 3);

			find_ns(iterator, oui, subns);

			vnslen = get_unaligned_le16(iterator->_arg + 4);
			iterator->_next_ns_data = iterator->_arg + size + vnslen;
			if (!iterator->current_namespace)
				size += vnslen;
		}

		/*
		 * this is what we will return to user, but we need to
		 * move on first so next call has something fresh to test
		 */
		iterator->_this_arg_index = iterator->_arg_index;
		iterator->_this_arg = iterator->_arg;
		iterator->_this_arg_size = size;

		/* internally move on the size of this arg */
		iterator->_arg += size;

		/*
		 * check for insanity where we are given a bitmap that
		 * claims to have more arg content than the length of the
		 * radiotap section.  We will normally end up equalling this
		 * max_length on the last arg, never exceeding it.
		 */

		if ((unsigned long)iterator->_arg -
		    (unsigned long)iterator->_rtheader >
		    (unsigned long)iterator->_max_length)
			return -EINVAL;

		/* these special ones are valid in each bitmap word */
		switch (iterator->_arg_index % 32) {
		case IEEE80211_RADIOTAP_VENDOR_NAMESPACE:
			iterator->_reset_on_ext = 1;

			iterator->is_radiotap_ns = 0;
			/*
			 * If parser didn't register this vendor
			 * namespace with us, allow it to show it
			 * as 'raw. Do do that, set argument index
			 * to vendor namespace.
			 */
			iterator->_this_arg_index =
				IEEE80211_RADIOTAP_VENDOR_NAMESPACE;
			if (!iterator->current_namespace)
				hit = 1;
			goto next_entry;
		case IEEE80211_RADIOTAP_RADIOTAP_NAMESPACE:
			iterator->_reset_on_ext = 1;
			iterator->current_namespace = &radiotap_ns;
			iterator->is_radiotap_ns = 1;
			goto next_entry;
		case IEEE80211_RADIOTAP_EXT:
			/*
			 * bit 31 was set, there is more
			 * -- move to next u32 bitmap
			 */
			iterator->_bitmap_shifter =
				get_unaligned_le32(iterator->_next_bitmap);
			iterator->_next_bitmap++;
			if (iterator->_reset_on_ext)
				iterator->_arg_index = 0;
			else
				iterator->_arg_index++;
			iterator->_reset_on_ext = 0;
			break;
		default:
			/* we've got a hit! */
			hit = 1;
 next_entry:
			iterator->_bitmap_shifter >>= 1;
			iterator->_arg_index++;
		}

		/* if we found a valid arg earlier, return it now */
		if (hit)
			return 0;
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
 *  -ENOENT if no more args
 *  -EINVAL if something went wrong
 * 
 */
int run_parser(struct radiotap_header_data *data_out, const struct ieee80211_radiotap_header *actual_data, int length_data){
    debug_assert(data_out && actual_data);

    struct ieee80211_radiotap_iterator iterator;
    struct ieee80211_radiotap_header header;
    header.it_version = actual_data->it_version;
    header.it_len = actual_data->it_len;
    header.it_pad = actual_data->it_pad;
    header.it_present = actual_data->it_present;



    long unsigned int max_len = (long unsigned int)length_data;
    int buffer_length = length_data;

    int return_value = ieee80211_radiotap_iterator_init(&iterator, &header, max_len, NULL);

    //Sanity check for init function failure.
    if(return_value == -EINVAL) {
        return -EINVAL;
    }

    while(buffer_length > 0){
        while(!return_value){
            return_value = ieee80211_radiotap_iterator_next(&iterator);
            if(return_value){
                continue;
            }
            //NOTE, inside these case statements, get_unaligned(type *)iterator->this_arg MUST be used for multibyte data fields.
            switch(iterator._this_arg_index){
                case IEEE80211_RADIOTAP_TSFT:
                    data_out->TSFT[0] = get_unaligned_le32((uint32_t *)iterator._this_arg);
                    data_out->TSFT[1] = get_unaligned_le32((uint32_t *)iterator._this_arg);
                break;
                case IEEE80211_RADIOTAP_FLAGS:
                    data_out->Flags = *iterator._this_arg;
                break;
                case IEEE80211_RADIOTAP_CHANNEL:
                    data_out->ChannelFreq  = get_unaligned_le16((uint16_t *)iterator._this_arg);
                    data_out->ChannelFlags = get_unaligned_le16((uint16_t *)iterator._this_arg);
                break;
                case IEEE80211_RADIOTAP_DBM_ANTSIGNAL:
                    data_out->dBm_AntSignal = *iterator._this_arg;
                break;
                case IEEE80211_RADIOTAP_MCS:
                    data_out->MCS_Known = *iterator._this_arg;
                    data_out->MCS_Flags = *iterator._this_arg;
                    data_out->MCS_MCS = *iterator._this_arg;
                default:
                break;
            }
        }
        if(return_value != -ENOENT){
            log_error("Malformed Radiotap Data!\n");
            return return_value;
        }
        //discard current header part and continue
        actual_data += iterator._max_length;
        buffer_length -= iterator._max_length;
    }
    return 0;
}