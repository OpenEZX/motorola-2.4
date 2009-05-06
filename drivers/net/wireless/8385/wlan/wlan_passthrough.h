/* 
 * 		File: wlan_passthrough.h
 */

#ifndef	_WLAN_PASSTHROUGH_
#define	_WLAN_PASSTHROUGH_

#define	PT_MODE_SUCCESS		0
#define	PT_MODE_FAILURE		1
#define	PT_MODE_TIMEOUT		2

#define MAX_PT_DATA_PKT_SIZE	2346

typedef struct _HostCmd_DS_802_11_PASSTHROUGH {
	u16	Action;	
	u8	Enable;
	u8	Channel;
	u32	Filter1;
	u32	Filter2;
	u8	MatchBssid[6];
	u8	MatchSsid[32];
} __ATTRIB_PACK__ HostCmd_DS_802_11_PASSTHROUGH,
	*PHostCmd_DS_802_11_PASSTHROUGH;

typedef struct _wlan_passthrough_config {
	u16	Action;	
	u8	Enable;
	u8	Channel;
	u32	Filter1;
	u32	Filter2;
	u8	MatchBssid[6];
	u8	MatchSsid[32];
} __ATTRIB_PACK__ wlan_passthrough_config;

typedef struct _PTDataPacket {
	u16 	PTPktType;
	u16	SizeOfPkt;
	u8	RSSI;
	u8	packet[MAX_PT_DATA_PKT_SIZE];
} __ATTRIB_PACK__ PTDataPacket;

typedef struct _PTDataPacketsList {
	struct list_head	list;
	PTDataPacket		*pt_pkt;
} PTDataPacketList;

#endif	/* _WLAN_PASSTHROUGH_ */
