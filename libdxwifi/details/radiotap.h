/**
 *  radiotap.h
 * 
 *  DESCRIPTION: Headers for Radiotap.c
 * 
 *  https://github.com/oresat/oresat-dxwifi-software
 * 
 */

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

#include <asm/types.h>

#include <linux/types.h>
#include <linux/errno.h>

#include <libdxwifi/details/ieee80211.h>

typedef __u32 __bitwise __le32;
typedef __u16 __bitwise __le16;

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
 struct radiotap_header_data {
    uint8_t  it_version; /* Version */
    uint8_t  it_pad; /* padding */
    uint16_t it_len; /* */
    uint32_t it_present; /* */
    //Capture Flags, Derived from IE80211.h, Transmitter.h, & Radiotap.org
   
    //TSFT:  Bit Number 0.
    uint64_t TSFT; //Unit: Microseconds
    //Flags: Bit Number 1.  
    uint8_t  Flags; //Unit: Bitmap. See: /radiotap/fields/flags
    //Rate: Bit Number 2.
    uint8_t  Rate; //Unit 500Kbps (Tx/Rx Data Rate)
    //Channel:  Bit Number 3.  
    uint16_t ChannelFreq; //Unit: MHz. (TX/RX Frequency)
    uint16_t ChannelFlags; //Unit: Bitmap.  See: /radiotap/fields/Channel
    //FHSS: Bit Number 4.  
    uint8_t  FHSS_hop_set; //Unit: ??
    uint8_t  FHSS_hop_patt; //Unit: ??
    //Antenna Signal: Bit Number 5.
    int8_t   dBm_AntSignal; //Unit: dBm
    //Antenna Noise:  Bit Number 6.
    int8_t   dBm_AntNoise; //Unit: dBm
    //Lock Quality:  Bit Number 7. (Quality of Barker code Lock).
    uint16_t LockQuality; //Unitless
    //TX Attenuation:  Bit Number 8.
    uint16_t TX_Attenuation;
    //dB Tx Attenuation:  Bit Number 9.
    uint16_t dB_TX_Attenuation;
    //dBm Tx Power:  Bit Number 10.
    int8_t   dBm_Tx_Power;
    //Antenna: Bit Number 11.
    uint8_t  Antenna;
    //dB Antenna Signal: Bit Number 12.
    uint8_t  db_AntSignal; //Unit: dB
    //dB Antenna Noise: Bit Number 13.
    uint8_t  db_AntNoise; //Unit: dB
    //RX Flags:  Bit Number 14.
    uint16_t Rx_Flags; //Bitmap
    //TX Flags:  Bit Number 15.
    uint16_t Tx_Flags; //Bitmap
    //RTS Retries: Bit Number 16.
    uint8_t  RTS_Retries;  //Number of RTS retries a transmission frame used.
    //Data Retries: Bit Number 17.
    uint8_t  Data_Retries; //Number of data retries a transmission frame used.
    //Bit Number 18. - XChannel - Undefined
    //MCS: Required Alignment 1.  Bit Number 19.
    uint8_t  MCS_Known;
    uint8_t  MCS_Flags;
    uint8_t  MCS_MCS; //Rate Index (IEEE 802.11N-2009)
    //A-MPDU Status:  Bit Number 20.
    uint32_t AMPDU_RefNumber;
    uint16_t AMPDU_Flags;
    uint8_t  AMPDU_Delimiter_CRC_Value;
    //Last Byte Reserved.
    //VHT:  Bit Number 21.
    uint8_t  VHT_Known;
    uint8_t  VHT_Flags;
    uint8_t  VHT_Bandwidth;
    uint8_t  VHT_MCS_NSS;
    uint8_t  VHT_Coding;
    uint8_t  VHT_GroupID;
    uint16_t VHT_PartialAid;
    //Timestamp:  Bit Number 22.
    uint64_t Timestamp_timestamp;
    uint16_t Timestamp_accuracy;
    uint8_t  Timestamp_UnitPos;
    uint8_t  Timestamp_Flags;
    //HE: Bit Number 23.  Frame Received/Transmitted using HE PHY.
    uint16_t HE_Data1;
    uint16_t HE_Data2;
    uint16_t HE_Data3;
    uint16_t HE_Data4;
    uint16_t HE_Data5;
    uint16_t HE_Data6;
    //HE-MU: Bit Number 24.
    uint16_t HE_MU_Flags1;
    uint16_t HE_MU_Flags2; //Only used for 40MHz or Higher
    uint8_t  HE_MU_RU_Channel1;
    uint8_t  HE_MU_RU_Channel2;  //Only used for 40 MHz or Higher
    //Bit Number 25. - Undefined
    //0-Length PSDU: No PSDU in / captured for PPDU.  Header not followed by 802.11 Header. Bit Number 26.
    uint8_t  ZERO_LEN_PSDU;
    //L-SIG: Bit Number 27.
    uint16_t LSIG_Data1; //Unitless
    uint16_t LSIG_Data2;
};

int radiotap_iteratator_initialize(struct radiotap_iterator *iterator, struct ieee80211_radiotap_header *current_header, uint64_t max_length, const struct radiotap_vendor_namespace *vendor_namespace);

int radiotap_iterator_next(struct radiotap_iterator *iterator);

int run_parser(struct radiotap_header_data *output_data);

uint64_t get_unaligned_le64(const void *p){
    return get_unaligned_le64(p);
}

uint32_t get_unaligned_le32(const void *p) {
    return get_unaligned_le32(p);
}

uint16_t get_unaligned_le16 (const void *p){
    return get_unaligned_le16(p);
}