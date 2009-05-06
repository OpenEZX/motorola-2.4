/*
File: wlan_11h.c
*/
#include "include.h"


int wlan_802_11h_tpc_enabled( wlan_private * priv )
/*check if 11H enabled*/
{
	wlan_adapter			*Adapter = priv->adapter;
	wlan_802_11h_tpc_t		*state = &Adapter->State11HTPC;
	return ( (state->Enable11HTPC == TRUE)? 1:0 );
}

int wlan_802_11h_tpc_enable( wlan_private * priv, BOOLEAN flag )
/* enable/disable 11H tpc*/
{
/* flag== TRUE, 11H_TPC is enable, otherwise, disable */

	wlan_adapter			*Adapter = priv->adapter;
	wlan_802_11h_tpc_t		*state = &Adapter->State11HTPC;
	int ret;
	int enable = (flag == TRUE )? 1:0;
	
	ENTER();

	/* send cmd to FW to enable/disable 11H TPC fucntion in FW */
	ret = PrepareAndSendCommand(priv, HostCmd_CMD_802_11_SNMP_MIB,
		0, HostCmd_OPTION_USE_INT, OID_802_11H_TPC_ENABLE, 
		HostCmd_PENDING_ON_SET_OID, &enable);

	if (ret) {
		LEAVE();
		return ret;
	}

	state->Enable11HTPC = flag;
	LEAVE();
	return WLAN_STATUS_SUCCESS;
}

int  wlan_802_11h_init( wlan_private *priv )
/*Init the 802.11h tpc related parameter*/
{
	wlan_adapter			*Adapter = priv->adapter;
	wlan_802_11h_tpc_t		*state = &Adapter->State11HTPC;
	int ret;

	ENTER();

	state -> UsrDefPowerConstraint= MRV_11H_TPC_POWERCONSTAINT;
	state -> MinTxPowerCapability = MRV_11H_TPC_POWERCAPABILITY_MIN;
	state -> MaxTxPowerCapability = MRV_11H_TPC_POWERCAPABILITY_MAX;

	ret = wlan_802_11h_tpc_enable( priv, FALSE);  /* disable 802.11H TPC */
	if (ret) {
		LEAVE();
		return ret;
	}

	LEAVE();
	return WLAN_STATUS_SUCCESS;
}


/*Build command for CMD_802_11H_TPC_REQUEST */
int wlan_cmd_802_11h_tpc_request(wlan_private * priv, 
			HostCmd_DS_COMMAND * cmd, u16 cmdno)
{
	HostCmd_DS_802_11H_TPC_REQUEST *ptpcReq = &cmd->params.tpcrequest;
	wlan_adapter *Adapter = priv->adapter;
	wlan_802_11h_tpc_request_param_t *preqParam = 
					&(Adapter->State11HTPC.RequestTpc); 

	ENTER();

	cmd->Command = wlan_cpu_to_le16(cmdno);
	ptpcReq->timeout = wlan_cpu_to_le16(preqParam->timeout);
	ptpcReq->RateIndex = preqParam->index;
	memcpy( ptpcReq->BSSID, preqParam->mac, ETH_ALEN );

	cmd->Size = wlan_cpu_to_le16(sizeof(HostCmd_DS_802_11H_TPC_REQUEST) + S_DS_GEN);
	
	HEXDUMP("11HTPC: 802_11H_TPC_REQUEST:", (u8 *)cmd, (int)(cmd->Size) );
 	LEAVE();

	return 0;
}

/* Build comamnd body for CMD_802_11H_TPC_INFO */
int wlan_cmd_802_11h_tpc_info(wlan_private * priv, 
			HostCmd_DS_COMMAND * cmd, u16 cmdno)
{
	HostCmd_DS_802_11H_TPC_INFO *ptpcInfo = &cmd->params.tpcinfo;
	MrvlIEtypes_LocalPowerConstraint_t *pconstraint = 
					&ptpcInfo->LocalPowerConstraint;
	MrvlIEtypes_PowerCapability_t *pcap = &ptpcInfo->PowerCapability;
	wlan_adapter *Adapter = priv->adapter;
	wlan_802_11h_tpc_t *ptpc = &(Adapter->State11HTPC); 

	ENTER();

	cmd->Command = wlan_cpu_to_le16(cmdno);

	pcap -> MinPower = ptpc->MinTxPowerCapability ;
	pcap -> MaxPower = ptpc->MaxTxPowerCapability;
	pcap -> Header.Len   = wlan_cpu_to_le16(2); 
	pcap -> Header.Type  = wlan_cpu_to_le16(TLV_TYPE_POWER_CAPABILITY);

	pconstraint->Chan = ptpc->InfoTpc.Chan; 
	pconstraint->PowerConstraint = ptpc->InfoTpc.PowerConstraint;
	pconstraint->Header.Type = wlan_cpu_to_le16(TLV_TYPE_POWER_CONSTRAINT);
	pconstraint->Header.Len = wlan_cpu_to_le16(2);

	cmd->Size = wlan_cpu_to_le16(pconstraint->Header.Len + sizeof(MrvlIEtypesHeader_t) +  	/*Constraint TLV Len*/
		    pcap->Header.Len + sizeof(MrvlIEtypesHeader_t) +		/*Capability TLV Len*/
		    S_DS_GEN);
	
	HEXDUMP("11HTPC: 802_11H_TPC_INFO:", (u8 *)cmd, (int)(cmd->Size) );
 	LEAVE();

	return 0;
}

/*Private cmd: Enable 11H TPC */
int wlan_cmd_enable_11h_tpc( wlan_private *priv, struct iwreq *wrq )
{

	int data = *((int *)wrq->u.data.pointer);
	
	ENTER();

	if ( wrq->u.data.length == 0 ) {
		return -EINVAL;
	}

	PRINTK2("Enable 11D: %s\n", (data==1)?"Enable":"Disable");
	switch ( data ) {
		case CMD_ENABLED:
			wlan_802_11h_tpc_enable( priv, TRUE);
			break;
		case CMD_DISABLED:
			wlan_802_11h_tpc_enable( priv, FALSE);
			break;
		default:			
			break;
	}

	data = wlan_802_11h_tpc_enabled( priv );
	copy_to_user(wrq->u.data.pointer, &data, sizeof(int) );
	wrq->u.data.length = 1;
	
	LEAVE();

	return WLAN_STATUS_SUCCESS;
}

/*private cmd: get local power*/				
int wlan_cmd_get_local_power_11h_tpc( wlan_private *priv, struct iwreq *wrq )
{
	int data = *((int *)wrq->u.data.pointer);
	s8 *localpowerPtr = &(priv->adapter->State11HTPC.UsrDefPowerConstraint);
	int localpower;

	ENTER();
	
	if ( wrq->u.data.length != 0 ) 
		*localpowerPtr = (s8) data; 
				
	localpower = (int)(*localpowerPtr);
	copy_to_user(wrq->u.data.pointer, &localpower, sizeof(int) );
	wrq->u.data.length = 1;

	LEAVE();
	return WLAN_STATUS_SUCCESS;
}

/*private cmd: get tpc info*/
int wlan_do_request_tpc_ioctl(wlan_private *priv, struct iwreq *wrq)
{
	wlan_adapter	*Adapter = priv->adapter;
	wlan_802_11h_tpc_request_param_t *req = 
					&(Adapter->State11HTPC.RequestTpc);
	
	int             ret=0;
	u8 		mac[6];
	int 		timeout, index;
	char 		str[64];

	ENTER();

	PRINTK2("requesttpc: data=%s len=0x%x\n", wrq->u.data.pointer, 
							wrq->u.data.length );
	ret = sscanf( wrq->u.data.pointer, "%x:%x:%x:%x:%x:%x %d %d",
			(unsigned int *)&mac[0], 
			(unsigned int *)&mac[1], 
			(unsigned int *)&mac[2], 
			(unsigned int *)&mac[3], 
			(unsigned int *)&mac[4], 
			(unsigned int *)&mac[5], 
			&timeout, &index );
	PRINTK2("Addr:%x:%x:%x:%x:%x:%x t:%d i:%d\n", 
		mac[0],mac[1],mac[2],mac[3],mac[4],mac[5],timeout,index);

	if ( ret < 8 ) {
		PRINTK2( "Not Enough Parameter for TPC Request\n");
		LEAVE();
		return -EINVAL;
	}

	memset( req, 0, sizeof(wlan_802_11h_tpc_request_param_t) );
 
	memcpy( req->mac, mac, ETH_ALEN );
	req->timeout = (u16)timeout;
	req->index = (u8)index;
			
	ret = PrepareAndSendCommand(priv, HostCmd_CMD_802_11H_TPC_REQUEST,
			 HostCmd_ACT_GEN_SET,	HostCmd_OPTION_USE_INT
			| HostCmd_OPTION_WAITFORRSP
			, 0, HostCmd_PENDING_ON_NONE, NULL);

	if( ret ) {
		LEAVE();
		return -EINVAL;
	}

	sprintf(str, "|%d|%d|%d|%d|", req->RetCode, req->TxPower, 
						req->LinkMargin, req->RSSI );

	wrq->u.data.length = strlen(str)+1;
	copy_to_user( wrq->u.data.pointer, str, wrq->u.data.length );

	PRINTK2("Request TPC: %s\n", str );
	LEAVE();
	return 0;
}

/*private cmd: set power Caps*/
int wlan_do_power_cap_ioctl(wlan_private *priv, struct iwreq *wrq)
{
	int 	min, max, ret=0;
	char 	str[64];
	wlan_adapter	*Adapter = priv->adapter;
	s8 	*MaxTxPwr = &(Adapter->State11HTPC.MaxTxPowerCapability);
	s8 	*MinTxPwr = &(Adapter->State11HTPC.MinTxPowerCapability);

	ENTER();

	PRINTK2("11HTPC: PowerCap: data=%s len=0x%x\n", wrq->u.data.pointer, 
							wrq->u.data.length );
	if ( wrq->u.data.length >  2 ) {
		ret = sscanf( wrq->u.data.pointer, "%d %d", 
				(unsigned int *)&min, (unsigned int *)&max );

		PRINTK2("11HTPC: MinPwr:%d MaxPwr:%d\n", min, max );

		if ( ret < 2 ) {
			PRINTK2( "Not Enough Parameter for Set Min/Max TX Pwr\n");
			LEAVE();
			return -EINVAL;
		}
	
		*MaxTxPwr = (s8)max;
		*MinTxPwr = (s8)min;
	}

	sprintf(str, "PowerCapability: MIN:%d MAX:%d", *MinTxPwr, *MaxTxPwr );

	wrq->u.data.length = strlen(str)+1;
	copy_to_user( wrq->u.data.pointer, str, wrq->u.data.length );

	PRINTK2("11HTPC: CUR TX CAP: %s\n", str );
	LEAVE();
	return 0;
}


/* processinf the TPC Request return */
int  wlan_ret_802_11h_tpc_request( wlan_private *priv, HostCmd_DS_COMMAND *resp)
{
	HostCmd_DS_802_11H_TPC_REQUEST_RSP	
			*ptpcret = &resp->params.tpcrequestresp;
	wlan_adapter   	*Adapter = priv->adapter;
	wlan_802_11h_tpc_request_param_t	
			*preqParam = &(Adapter->State11HTPC.RequestTpc); 

	ENTER();

	HEXDUMP( "11HTPC: 11H TPC REQUEST Rsp Data:", (u8 *)resp, (int)resp->Size ); 	

	PRINTK(" TPCRetCode=0x%x, TxPower=0x%x, LinkMargin=0x%x, RSSI=0x%x\n",
		ptpcret-> TPCRetCode, ptpcret-> TxPower, ptpcret-> LinkMargin, ptpcret->RSSI);

	preqParam ->RetCode 	= ptpcret-> TPCRetCode;
	preqParam ->TxPower 	= ptpcret-> TxPower;
	preqParam ->LinkMargin	= ptpcret-> LinkMargin;
	preqParam ->RSSI	= ptpcret->RSSI;

	LEAVE();
	return 0;	
}

/* processinf the TPC Request return */
int  wlan_ret_802_11h_tpc_info( wlan_private *priv, HostCmd_DS_COMMAND *resp)
{
	/*Ignore the return resule. 
		May need to check the return codeNo any process*/

	ENTER();

	HEXDUMP( "11HTPC: 11H TPC INFO Rsp Data:", (u8 *)resp, (int)resp->Size ); 	

	LEAVE();
	return 0;	
}




