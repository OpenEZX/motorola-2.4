#include 	"cmd_macros_temp.h"
#include 	"endianSwap.h"
 

char	cmd_dtim[] = {
			1, 2
		},
		cmd_hw_spec[] = {
#ifdef FWVERSION3
			12,
#else
			11,
#endif
			2, 2, 2, 2, 6, 2, 2, 4, 4, 4, 4,
#ifdef FWVERSION3
			4
#endif
		},  
		cmd_scan[] = {
			//10, 2, 1, 6, 32, 1, 2, 14, 2, 2, 2	//old
			11, 2, 1, 6, 32, 1, 2, 14, 2, 2, 2, 14	//new
		},
		cmd_mac_ctl[] = {
			2, 2, 2
		},
#ifdef NEW_ASSOCIATION
		cmd_assoc[] = {		
#ifdef WPA
			12,
#else
			11,
#endif
			6, 2, 2, 2, 32, 1, 2, 1, 7, 8, 
#ifdef MULTI_BANDS
			14
#else			
#ifdef G_RATE
			14,
#else
			8,
#endif
#endif
#ifdef WPA
			24
#endif
		},
#else	//old way of ASSOCIATION
		cmd_assoc[] = {
			5, 6, 2, 2, 2, 4	
		},
#endif
		cmd_deauth[] = {
			2, 6, 2
		},
		cmd_setwep[] = {
			10, 2, 2, 1, 1, 1, 1, 16, 16, 16, 16
		},
		cmd_adhoc_start[] = {
			14, 32, 1, 2, 1, 1, 1, 2, 3, 1, 7, 2, 2, 14, 14	
				//here "3, 1" is added instead of 4(for union) to 
				//avoid being it treated as int
		},
		cmd_reset[] = {
			1, 2	
		},
		cmd_auth[] = {
			4, 6, 1, 2, 8
		},
		cmd_snmp[] = {
			4, 2, 2, 2, 128
		},
		cmd_txpow[] = {
			12, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2	
		},
		cmd_drate[] = {
			3, 2, 2, 
#ifdef MULTI_BANDS
			14
#else			
#ifdef G_RATE
			14
#else
			8
#endif
#endif
		},
		cmd_madr[] = {
			3, 2, 2, 192
		},
		cmd_adj[] = {
			14, 6, 32, 1, 2, 1, 8, 8, 7, 8, 2, 14, 2, 2, 14
		},
		cmd_rssi[] = {
			4, 2, 2, 2, 2
		},
		cmd_dassoc[] = {
			2, 6, 2	
		},
		cmd_adstop[] = {
			0	/* nothing to convert except command and size */	
		},
		cmd_enrsn[] = {
			2, 2, 2	
		},
		cmd_rsnauth[] = {
			6, 2, 1, 1, 1, 1, 2	// Here array 4 chars is split and writen as 4 separate bytes 
		},				// to avoid seeing it as a long
		cmd_pwkkey[] = {
			4, 2, 16, 8, 8
		},
		cmd_grpkey[] = {
			4, 2, 16, 8, 8
		},
		cmd_radioctl[] = {
			2, 2, 2 	//3, 2, 1, 1 SAJIT 
		},
		cmd_rfantenna[] = {
			2, 2, 2
		},
		cmd_rf_chan[] = {
			5, 2,
#ifdef FWVERSION3
			2, 2, 2, 32
#else
			2, 14, 1, 1
#endif
		},
		cmd_mac_addr_set[] ={
			2,2,6

		},
		cmd_hw_spec_rsp[] = {
#ifdef FWVERSION3
			12,
#else
			11,
#endif
			2, 2, 2, 2, 6, 2, 2, 4, 4, 4, 4,
#ifdef FWVERSION3
			4
#endif
		},
		cmd_scan_rsp[] = {
			4, 6, 16, 2, 1	
		},
		cmd_assoc_rsp[] = {
#ifdef FWVERSION3
#ifdef NEW_ASSOCIATION_RSP
			10, 2, 2, 2, 2, 1, 1, 8, 1, 1, 8
#else
			6, 2, 2, 2, 2, 8, 12
#endif
#else
			2, 1, 3		
#endif
		},

		cmd_snmp_rsp[] = {
			4, 2, 2, 2, 128
		},
		cmd_txpow_rsp[] = {
			12, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2	
		},
		cmd_mac_ctl_rsp[] = {
			0
		},
		cmd_adstart_resp[] = {
			0
		},
		cmd_adjoin_resp[] = {
			0
		},
		cmd_adstop_resp[] = {
			0
		},
		cmd_drate_rsp[] ={
			3, 2, 2, 
#ifdef MULTI_BANDS
			14
#else			
#ifdef G_RATE
			14
#else
			8
#endif
#endif
			
		},
		cmd_rf_chan_rsp[] = {
			5, 2,
#ifdef FWVERSION3
			2, 2, 2, 32
#else
			2, 14, 1, 1
#endif
		},
		cmd_setwep_rsp[] = {
			0
		},
		cmd_madr_rsp[] = {
			0
		},
		cmd_rfantenna_rsp[]={
			2, 2, 2
		},
		cmd_auth_rsp[] = {
			0
		},
		cmd_deauth_rsp[] = {
			0
		};
		cmd_mac_addr_set_rsp[] ={
			2,2,6
		};






char *str_arr[] = {
	cmd_dtim,	//0
	cmd_hw_spec,	
	cmd_scan,
	cmd_mac_ctl,
	cmd_assoc,
	cmd_deauth,	//5
	cmd_setwep,
	cmd_adhoc_start,
	cmd_reset,
	cmd_auth,
	cmd_snmp,	//10	
	cmd_txpow,
	cmd_drate,
	cmd_madr,
	cmd_adj,
	cmd_rssi,	//15
	cmd_dassoc,
	cmd_adstop,
	cmd_enrsn,
	cmd_rsnauth,
	cmd_pwkkey,	//20
	cmd_grpkey,
	cmd_radioctl,
	cmd_rfantenna,
	cmd_rf_chan,
	cmd_mac_addr_set,
	cmd_hw_spec_rsp, //25
	cmd_scan_rsp,	
	cmd_assoc_rsp,
	cmd_snmp_rsp,
	cmd_txpow_rsp,
	cmd_mac_ctl_rsp, //30
	cmd_adstart_resp,
	cmd_adjoin_resp,
	cmd_adstop_resp,
	cmd_drate_rsp,
	cmd_rf_chan_rsp, //35
	cmd_setwep_rsp,  
	cmd_madr_rsp,
	cmd_rfantenna_rsp,
	cmd_auth_rsp,
	cmd_deauth_rsp,  //40
	cmd_mac_addr_set_rsp
};


unsigned short index_arr[] = {
	//commands
	HostCmd_CMD_SET_DTIM_MULTIPLE,
	HostCmd_CMD_GET_HW_SPEC,
	HostCmd_CMD_802_11_SCAN,
	HostCmd_CMD_MAC_CONTROL,
	HostCmd_CMD_802_11_ASSOCIATE,
//	HostCmd_CMD_802_11_REASSOCIATE,
	HostCmd_CMD_802_11_DEAUTHENTICATE,
	HostCmd_CMD_802_11_SET_WEP,
	HostCmd_CMD_802_11_AD_HOC_START,
	HostCmd_CMD_802_11_RESET,
	HostCmd_CMD_802_11_AUTHENTICATE,
	HostCmd_CMD_802_11_SNMP_MIB,
	HostCmd_CMD_802_11_RF_TX_POWER,
	HostCmd_CMD_802_11_DATA_RATE,
	HostCmd_CMD_MAC_MULTICAST_ADR,
	HostCmd_CMD_802_11_AD_HOC_JOIN,
	HostCmd_CMD_802_11_RSSI,
	HostCmd_CMD_802_11_DISASSOCIATE,
	HostCmd_CMD_802_11_AD_HOC_STOP,
	HostCmd_CMD_802_11_ENABLE_RSN,
	HostCmd_CMD_802_11_RSN_AUTH_SUITES,
	HostCmd_CMD_802_11_PWK_KEY,
	HostCmd_CMD_802_11_GRP_KEY,
	HostCmd_CMD_802_11_RADIO_CONTROL,
	HostCmd_CMD_802_11_RF_ANTENNA,
	HostCmd_CMD_802_11_RF_CHANNEL,
	HostCmd_CMD_802_11_MAC_ADDRESS,
	//commands responses											
	HostCmd_RET_HW_SPEC_INFO,
	HostCmd_RET_802_11_SCAN,
	HostCmd_RET_802_11_ASSOCIATE,
//	HostCmd_RET_802_11_REASSOCIATE
	HostCmd_RET_802_11_SNMP_MIB,
	HostCmd_RET_802_11_RF_TX_POWER,
	HostCmd_RET_802_11_DATA_RATE,
	HostCmd_RET_MAC_CONTROL,
	HostCmd_RET_802_11_AD_HOC_START,
	HostCmd_RET_802_11_AD_HOC_JOIN,
	HostCmd_RET_802_11_AD_HOC_STOP,
	HostCmd_RET_802_11_RF_CHANNEL,
	HostCmd_RET_802_11_SET_WEP,
	HostCmd_RET_802_11_RADIO_CONTROL,
	HostCmd_RET_MAC_MULTICAST_ADR,
	HostCmd_RET_802_11_DATA_RATE,
	HostCmd_RET_802_11_RF_ANTENNA,
	HostCmd_RET_802_11_AUTHENTICATE,
	HostCmd_RET_802_11_DEAUTHENTICATE,
	HostCmd_RET_802_11_MAC_ADDRESS
};


extern int get_index(unsigned short);
extern int endian_convert(void *);

int get_index(unsigned short index) {
	int i;
	
	dprintf("get_index(cmd=%x)\n", index);
	for(i = 0; i < sizeof(index_arr)/sizeof(index_arr[0]); i++) {
		if(index_arr[i] == index)
			return i;
	}
	

	if(index == HostCmd_CMD_802_11_REASSOCIATE)
		return 4;
	else if(index == HostCmd_RET_802_11_REASSOCIATE)
		return 26;
	else if( index == HostCmd_CMD_802_11_NEW_ASSOCIATE)
		return 4;
	else {
		dprintf("cmd not found in table! %x \n",index);
		return -1;
	}
}


int endian_convert(void *strct) {
	char *temp;
	short index;
	unsigned short *cmd = strct, *temp_var;
	int i = 0, j, val, incr = 0;

	index=*cmd;
	dprintf("index cmd=0x%X\n", index);
	
	/* first convert initial 4 members of the DS command structure structure */
	while(i++ < 4) {
		unsigned short *adr = (strct+incr);
	
		*adr =  le16_to_cpu(*adr);
		incr += 2;
	}

	if((index = get_index(index)) < 0){		//get the index to correct structure 
		dprintf("RETURNING ERRRR.. cmd index=%d\n", index);
		return -1;
	}
	
	temp = str_arr[index];	
	j = (int) *(temp + 0);
	

	/* Here convert the union of structures */
	for(i = 1; i <= j; i++) {
		val = (int)(*(temp + i));
		if(val == 2) {
			unsigned short *adr = (strct+incr);

			*adr = le16_to_cpu(*adr);
		} else if(val == 4) {
			unsigned long *adr = (strct+incr);
			
			*adr = le32_to_cpu(*adr);
		}
		incr += val;
	}

	return 0;
}
