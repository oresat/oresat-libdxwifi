/**
 *  radiotap.h
 * 
 *  DESCRIPTION: Headers for Radiotap.c
 * 
 *  https://github.com/oresat/oresat-dxwifi-software
 * 
 */

#ifndef LIBDXWIFI_RADIOTAP_H
#define LIBDXWIFI_RADIOTAP_H

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

#include <asm/types.h>

#include <linux/types.h>
#include <linux/errno.h>

#include <libdxwifi/details/ieee80211.h>

typedef __u64 __bitwise _le64;
typedef __u32 __bitwise _le32;
typedef __u16 __bitwise _le16;

struct __una_u16 { uint16_t x __attribute__((packed));};
struct __una_u32 { uint32_t x __attribute__((packed));};
struct __una_u64 { uint64_t x __attribute__((packed));};

struct radiotap_override {
    uint8_t field;
    uint8_t align:4, size:4;
};

struct radiotap_align_size {
    uint8_t align:4, size:4;
};

struct radiotap_namespace {
    const struct radiotap_align_size * align_size;
    int n_bits;
    uint32_t oui;
    uint8_t subns;
};

struct radiotap_vendor_namespace{
    const struct radiotap_namespace * ns;
    int n_ns;
};

//See linux/blob/master/include/net/cfg80211.h for desc.
//Ref: ieee80211_radiotap_iterator
struct radiotap_iterator{
    struct ieee80211_radiotap_header * header;
    const struct radiotap_vendor_namespace * vendor_namespace;
    const struct radiotap_namespace * current_namespace;
    u_char *arg;  //Pointer to next argument
    u_char *next_ns_data; //beginning of next namespace's data.
    __le32 *next_bitmap; //Internal pointer to next present u32
    u_char *this_arg; // Current Arg.
    int this_arg_index; // Index of current arg
    int this_arg_size; // length of current arg.
    int is_radiotap_namespace; //Is the current namespace the default radiotap namespace?
    uint64_t max_length; //Length of radiotap header in CPU Byte Order
    int arg_index; //Next argument index
    uint32_t bitmap_shifter; //Internal shifter for current u32 butmap, bit 0 set, arg present.
    int reset_on_ext;  //Reset arg index to 0 when advancing to next bitmap word

};

/*
 *
 * Radiotap header contains captured flags from the radiotap header
 * 
 * 
 */
 typedef struct radiotap_header_data {
    //Capture Flags, Derived from IE80211.h, Transmitter.h, & Radiotap.org
   
    //TSFT:  Bit Number 0.
    uint32_t TSFT[2]; //Unit: Microseconds
    //Flags: Bit Number 1.  
    uint8_t  Flags; //Unit: Bitmap. See: /radiotap/fields/flags
    //Channel:  Bit Number 3.  
    uint16_t ChannelFreq; //Unit: MHz. (TX/RX Frequency)
    uint16_t ChannelFlags; //Unit: Bitmap.  See: /radiotap/fields/Channel
    //Antenna Signal: Bit Number 5.
    int8_t   dBm_AntSignal;
    //Antenna Noise:  Bit Number 6.
    //RX Flags:  Bit Number 14.
    uint16_t Rx_Flags; //Bitmap
    //MCS: Required Alignment 1.
    uint8_t  MCS_Known;
    uint8_t  MCS_Flags;
    uint8_t  MCS_MCS; //Rate Index (IEEE 802.11N-2009)
} radiotap_header_data;

int radiotap_iteratator_initialize(struct radiotap_iterator *iterator, struct ieee80211_radiotap_header *current_header, uint64_t max_length, const struct radiotap_vendor_namespace *vendor_namespace);

int radiotap_iterator_next(struct radiotap_iterator *iterator);

int run_parser(struct radiotap_header_data *output_data);

static uint16_t get_unaligned_le16 (const void *p){
    //return get_unaligned_le16(p);
    const struct __una_u16 * ptr = (const struct __una_u16 *)p;
    return ptr -> x;
}

static uint32_t get_unaligned_le32(const void *p) {
    //return get_unaligned_le32(p);
    const struct __una_u32 * ptr = (const struct __una_u32 *)p;
    return ptr -> x;
}


static uint64_t get_unaligned_le64(const void *p){
    //return get_unaligned_le64(p);
    const struct __una_u64 * ptr = (const struct __una_u64 *)p;
    return ptr -> x;

}




#endif //LIBDXWIFI_RADIOTAP_H