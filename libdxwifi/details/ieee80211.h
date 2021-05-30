/**
 *  ieee80211.h
 *  
 *  DESCRIPTION: The following code has been ported from https://github.com/torvalds/linux
 *  This file defines a ad-hoc subset of the 802.11 standard. Only components 
 *  that directly effect the dxwifi project are defined and may not be 
 *  completely faithful to the standard
 * 
 *  https://github.com/oresat/oresat-dxwifi-software
 * 
 */


#ifndef LIBDXWIFI_IEEE80211_H
#define LIBDXWIFI_IEEE80211_H

#include <stdint.h>
#include <stdbool.h>

// https://networkengineering.stackexchange.com/questions/32970/what-is-the-802-11-mtu
#define IEEE80211_MTU_MAX_LEN 2304

#define IEEE80211_MAX_FRAG_THRESHOLD 2352

#define IEEE80211_MAC_ADDR_LEN 6

#define IEEE80211_FCS_SIZE 4

#define IEEE80211_PROTOCOL_VERSION 0

// Defined in github.com/torvalds/linux/include/linux/ieee80211.h 
struct ieee80211_hdr_3addr { 
    uint16_t    frame_control;  /* __le16 */
    uint16_t    duration_id;    /* __le16 */

   /*
   *  The following addresses can have different intreptations depenging on the 
   *  state of the to_ds/from_ds flags in the frame control. By default we set
   *  to_ds to 0 and from_ds to 1.
   *  +-------+---------+-------------+-------------+-------------+-----------+
   *  | To DS | From DS | Address 1   | Address 2   | Address 3   | Address 4 |
   *  +-------+---------+-------------+-------------+-------------+-----------+
   *  |     0 |       0 | Destination | Source      | BSSID       | N/A       |
   *  |     0 |       1 | Destination | BSSID       | Source      | N/A       |
   *  |     1 |       0 | BSSID       | Source      | Destination | N/A       |
   *  |     1 |       1 | Receiver    | Transmitter | Destination | Source    |
   *  +-------+---------+-------------+-------------+-------------+-----------+
   */
    uint8_t     addr1[IEEE80211_MAC_ADDR_LEN];
    uint8_t     addr2[IEEE80211_MAC_ADDR_LEN];
    uint8_t     addr3[IEEE80211_MAC_ADDR_LEN];

    uint16_t    seq_ctrl;       /*__le16 */
} __attribute__ ((packed));
typedef struct ieee80211_hdr_3addr ieee80211_hdr;


enum ieee80211_fctl_masks {
    IEEE80211_FCTL_VERS         = 0x0003,
    IEEE80211_FCTL_FTYPE		= 0x000c,
    IEEE80211_FCTL_STYPE		= 0x00f0,
    IEEE80211_FCTL_TODS		    = 0x0100,
    IEEE80211_FCTL_FROMDS		= 0x0200,
    IEEE80211_FCTL_MOREFRAGS	= 0x0400,
    IEEE80211_FCTL_RETRY		= 0x0800,
    IEEE80211_FCTL_PM		    = 0x1000,
    IEEE80211_FCTL_MOREDATA		= 0x2000,
    IEEE80211_FCTL_PROTECTED	= 0x4000,
    IEEE80211_FCTL_ORDER		= 0x8000,
    IEEE80211_FCTL_CTL_EXT		= 0x0f00
};

enum ieee80211_fctl_type {
    IEEE80211_FTYPE_MGMT    = 0x00,
    IEEE80211_FTYPE_CTL     = 0x04,
    IEEE80211_FTYPE_DATA    = 0x08,
    IEEE80211_FTYPE_EXT     = 0x0c
};


enum ieee80211_fctl_management_stype {
    IEEE80211_STYPE_ASSOC_REQ       = 0x0000,
    IEEE80211_STYPE_ASSOC_RESP      = 0x0010,
    IEEE80211_STYPE_REASSOC_REQ	    = 0x0020,
    IEEE80211_STYPE_REASSOC_RESP    = 0x0030,
    IEEE80211_STYPE_PROBE_REQ       = 0x0040,
    IEEE80211_STYPE_PROBE_RESP      = 0x0050,
    IEEE80211_STYPE_BEACON          = 0x0080,
    IEEE80211_STYPE_ATIM            = 0x0090,
    IEEE80211_STYPE_DISASSOC        = 0x00A0,
    IEEE80211_STYPE_AUTH            = 0x00B0,
    IEEE80211_STYPE_DEAUTH          = 0x00C0,
    IEEE80211_STYPE_ACTION          = 0x00D0
};


enum ieee80211_fctl_control_stype {
    IEEE80211_STYPE_CTL_EXT     = 0x0060,
    IEEE80211_STYPE_BACK_REQ    = 0x0080,
    IEEE80211_STYPE_BACK        = 0x0090,
    IEEE80211_STYPE_PSPOLL      = 0x00A0,
    IEEE80211_STYPE_RTS         = 0x00B0,
    IEEE80211_STYPE_CTS         = 0x00C0,
    IEEE80211_STYPE_ACK         = 0x00D0,
    IEEE80211_STYPE_CFEND       = 0x00E0,
    IEEE80211_STYPE_CFENDACK    = 0x00F0
};


enum ieee80211_fctl_data_stype {
    IEEE80211_STYPE_DATA                = 0x0000,
    IEEE80211_STYPE_DATA_CFACK          = 0x0010,
    IEEE80211_STYPE_DATA_CFPOLL         = 0x0020,
    IEEE80211_STYPE_DATA_CFACKPOLL      = 0x0030,
    IEEE80211_STYPE_NULLFUNC            = 0x0040,
    IEEE80211_STYPE_CFACK               = 0x0050,
    IEEE80211_STYPE_CFPOLL              = 0x0060,
    IEEE80211_STYPE_CFACKPOLL           = 0x0070,
    IEEE80211_STYPE_QOS_DATA            = 0x0080,
    IEEE80211_STYPE_QOS_DATA_CFACK      = 0x0090,
    IEEE80211_STYPE_QOS_DATA_CFPOLL     = 0x00A0,
    IEEE80211_STYPE_QOS_DATA_CFACKPOLL  = 0x00B0,
    IEEE80211_STYPE_QOS_NULLFUNC        = 0x00C0,
    IEEE80211_STYPE_QOS_CFACK           = 0x00D0,
    IEEE80211_STYPE_QOS_CFPOLL          = 0x00E0,
    IEEE80211_STYPE_QOS_CFACKPOLL       = 0x00F0
};

/**
 * The frame control field in the MAC header defines what type of frame is being
 * sent (i.e. control/management/data) and provides control information about 
 * that field. The ieee80211_frame_control struct is an abstraction to allow 
 * a user to control each field individually without worrying about endianess
 * or bit fields. 
 */
typedef struct {

    uint8_t protocol_version;           /* Protocol version is always zero  */

    enum ieee80211_fctl_type type;      /* Frame data type                  */

    union {
        enum ieee80211_fctl_management_stype management;
        enum ieee80211_fctl_control_stype    control;
        enum ieee80211_fctl_data_stype       data;
    } stype;    /* subtypes are dependent on the frame type */

    bool to_ds;                         /* To distribution system?          */
    bool from_ds;                       /* From a distribution system?      */
    bool more_frag;                     /* More fragment data follows?      */
    bool retry;                         /* Is this fram a retransmission?   */
    bool power_mgmt;                    /* Go into power saving after Rx?   */
    bool more_data;                     /* More data follows?               */
    bool wep;                           /* Is the data encrypted?           */
    bool order;                         /* Process frames in strict order?  */

} ieee80211_frame_control;


#endif // LIBDXWIFI_IEEE80211_H
