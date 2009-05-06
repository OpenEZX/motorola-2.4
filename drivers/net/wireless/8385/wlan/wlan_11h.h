/*
File: wlan_11h.h
*/
#ifndef _WLAN_11H_TPC_
#define _WLAN_11H_TPC_

typedef struct IEEEtypes_PowerConstraint_t {
	u8	ElementId;
	u8	Len;
	u8	LocalPowerConstraint;
} __ATTRIB_PACK__ IEEEtypes_PowerConstraint_t;

typedef struct IEEEtypes_PowerCapability_t {
	u8	ElementId;
	u8	Len;
	s8	MinTxPowerCapability;
	s8	MaxTxPowerCapability;
} __ATTRIB_PACK__ IEEEtypes_PowerCapability_t;

typedef struct IEEEtypes_TPCRequest_t {
	u8	ElementId;
	u8	Len;
} __ATTRIB_PACK__ IEEEtypes_TPCRequest_t;

typedef struct IEEEtypes_TPCReport_t {
	u8	ElementId;
	u8	Len;
	s8	TxPower;
	s8	Linkmargin;
} __ATTRIB_PACK__ IEEEtypes_TPCReport_t;


/* Local Power Constraint, CMD_802_11H_TPC_INFO */
typedef struct _MrvlIEtypes_LocalPowerConstraint_t {
	MrvlIEtypesHeader_t	Header;
	u8	Chan;
	u8	PowerConstraint;
} __ATTRIB_PACK__ MrvlIEtypes_LocalPowerConstraint_t;
	
/* data structure for CMD_802_11H_TPC_INFO */
typedef struct _HostCmd_DS_802_11H_TPC_INFO {
	MrvlIEtypes_LocalPowerConstraint_t	LocalPowerConstraint;
	MrvlIEtypes_PowerCapability_t		PowerCapability;
} __ATTRIB_PACK__ HostCmd_DS_802_11H_TPC_INFO;

typedef struct _HostCmd_DS_802_11H_TPC_INFO_RSP {
	MrvlIEtypes_LocalPowerConstraint_t	LocalPowerConstraint;
	MrvlIEtypes_PowerCapability_t		PowerCapability;
} __ATTRIB_PACK__ HostCmd_DS_802_11H_TPC_INFO_RSP;
	
/* data structure for CMD_802_11H_TPC_REQUEST */
typedef struct _HostCmd_DS_802_11H_TPC_REQUEST {
	u8		BSSID[ETH_ALEN];
	u16		timeout;
	u8		RateIndex;
} __ATTRIB_PACK__ HostCmd_DS_802_11H_TPC_REQUEST;

typedef struct _HostCmd_DS_802_11H_TPC_REQUEST_RSP {
	u8		TPCRetCode;
	s8		TxPower;
	s8		LinkMargin;
	s8		RSSI;
} __ATTRIB_PACK__ HostCmd_DS_802_11H_TPC_REQUEST_RSP;


typedef struct _wlan_802_11h_tpc_request_param_t {
/*request Parameter*/
	u8	mac[ETH_ALEN];
	u16	timeout;
	u8	index;
/*result*/
	u8	RetCode;
	s8	TxPower;
	s8	LinkMargin;
	s8	RSSI;
} wlan_802_11h_tpc_request_param_t;

typedef struct _wlan_802_11h_tpc_info_param_t {
	u8		Chan;
	u8		PowerConstraint;
} wlan_802_11h_tpc_info_param_t;

typedef struct _wlan_802_11h_tpc_t { 
	BOOLEAN		Enable11HTPC;	/* True for Enable 11H */
	s8		MinTxPowerCapability;
	s8		MaxTxPowerCapability;
	s8		UsrDefPowerConstraint;
	wlan_802_11h_tpc_info_param_t		InfoTpc;
	wlan_802_11h_tpc_request_param_t	RequestTpc;
} wlan_802_11h_tpc_t;


int wlan_cmd_802_11h_tpc_info(wlan_private * priv, 
			HostCmd_DS_COMMAND * cmd, u16 cmdno);

int wlan_cmd_802_11h_tpc_request(wlan_private * priv, 
			HostCmd_DS_COMMAND * cmd, u16 cmdno);


int wlan_cmd_enable_11h_tpc( wlan_private *priv, struct iwreq *wrq );

int wlan_cmd_get_local_power_11h_tpc( wlan_private *priv, struct iwreq *wrq );

int wlan_do_request_tpc_ioctl(wlan_private *priv, struct iwreq *wrq);

int wlan_do_power_cap_ioctl(wlan_private *priv, struct iwreq *wrq);


int  wlan_ret_802_11h_tpc_request( wlan_private *priv, HostCmd_DS_COMMAND *resp);

int  wlan_ret_802_11h_tpc_info( wlan_private *priv, HostCmd_DS_COMMAND *resp);


int  wlan_802_11h_init( wlan_private *priv );


#endif /*_WLAN_11H_TPC_*/
