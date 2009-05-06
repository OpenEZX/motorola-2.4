/* 
 * 		File: wlan_passthrough.h
 */

#define	PT_MODE_SUCCESS		0
#define	PT_MODE_FAILURE		1
#define	PT_MODE_TIMEOUT		2

#define MAX_PT_DATA_PKT_SIZE	2346

typedef struct _HostCmd_DS_802_11_PASSTHROUGH {
	u16	Action __attribute__ ((packed));	
	u8	Enable __attribute__ ((packed));
	u8	Channel __attribute__ ((packed));
	u32	Filter1 __attribute__ ((packed));
	u32	Filter2 __attribute__ ((packed));
	u8	MatchBssid[6] __attribute__ ((packed));
	u8	MatchSsid[32] __attribute__ ((packed));
} __attribute__ ((packed)) HostCmd_DS_802_11_PASSTHROUGH,
	*PHostCmd_DS_802_11_PASSTHROUGH;

typedef struct _wlan_passthrough_config {
	u16	Action __attribute__ ((packed));	
	u8	Enable __attribute__ ((packed));
	u8	Channel __attribute__ ((packed));
	u32	Filter1 __attribute__ ((packed));
	u32	Filter2 __attribute__ ((packed));
	u8	MatchBssid[6] __attribute__ ((packed));
	u8	MatchSsid[32] __attribute__ ((packed));
} __attribute__ ((packed)) wlan_passthrough_config;

typedef struct _PTDataPacket {
	u16 	PTPktType;
	u16	SizeOfPkt;
	u8	RSSI;
	u8	packet[MAX_PT_DATA_PKT_SIZE];
} __attribute__ ((packed)) PTDataPacket;

typedef struct _PTDataPacketsList {
	struct list_head	list;
	PTDataPacket		*pt_pkt;
} PTDataPacketList;

