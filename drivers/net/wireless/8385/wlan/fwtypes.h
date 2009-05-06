#ifndef FW_TYPES
#define FW_TYPES

#define PACK_START
#define PACK_END
#define UINT8   	u8
#define UINT16  	u16
#define UINT32		u32

#ifdef USE_FW_HOST

#define HostCmd_CMD_NONE                        host_MSG_NONE 
#define HostCmd_CMD_CODE_DNLD                   host_CMD_CODE_DNLD
#define HostCmd_CMD_GET_HW_SPEC                	host_CMD_GET_HW_SPEC
#define HostCmd_CMD_EEPROM_UPDATE               host_CMD_EEPROM_UPDATE
#define HostCmd_CMD_802_11_RESET                host_CMD_802_11_RESET
#define HostCmd_CMD_802_11_SCAN                 host_CMD_802_11_SCAN
#define HostCmd_CMD_802_11_QUERY_TRAFFIC        host_CMD_802_11_QUERY_TRAFFIC
#define HostCmd_CMD_802_11_QUERY_STATUS         host_CMD_802_11_QUERY_STATUS
#define HostCmd_CMD_802_11_GET_LOG              host_CMD_802_11_GET_LOG
#define HostCmd_CMD_MAC_MULTICAST_ADR           host_CMD_MAC_MULTICAST_ADR
#define HostCmd_CMD_802_11_AUTHENTICATE         host_CMD_802_11_AUTHENTICATE
#ifdef NEW_ASSOCIATION
#define	HostCmd_CMD_802_11_ASSOCIATE		0x0050
#else
#define HostCmd_CMD_802_11_ASSOCIATE            host_CMD_802_11_ASSOCIATE
#endif
#define HostCmd_CMD_802_11_SET_WEP              host_CMD_802_11_SET_WEP
#define HostCmd_CMD_802_11_GET_STAT             host_CMD_802_11_GET_STAT
#define HostCmd_CMD_802_3_GET_STAT              host_CMD_802_3_GET_STAT
#define HostCmd_CMD_802_11_SNMP_MIB             host_CMD_802_11_SNMP_MIB
#define HostCmd_CMD_MAC_REG_MAP                 host_CMD_MAC_REG_MAP        
#define HostCmd_CMD_BBP_REG_MAP                 host_CMD_BBP_REG_MAP         
#define HostCmd_CMD_MAC_REG_ACCESS              host_CMD_MAC_REG_ACCESS      
#define HostCmd_CMD_BBP_REG_ACCESS              host_CMD_BBP_REG_ACCESS      
#define HostCmd_CMD_RF_REG_ACCESS               host_CMD_RF_REG_ACCESS       
#define HostCmd_CMD_802_11_RADIO_CONTROL        host_CMD_802_11_RADIO_CONTROL
#define HostCmd_CMD_802_11_RF_CHANNEL           host_CMD_802_11_RF_CHANNEL   
#define HostCmd_CMD_802_11_RF_TX_POWER          host_CMD_802_11_RF_TX_POWER  
#define HostCmd_CMD_802_11_RSSI                 host_CMD_802_11_RSSI         
#define HostCmd_CMD_802_11_RF_ANTENNA           host_CMD_802_11_RF_ANTENNA   

#ifdef PS_REQUIRED
#define HostCmd_CMD_802_11_PS_MODE              host_CMD_802_11_PS_MODE        
#endif                                                                            
                                                                                  
#define HostCmd_CMD_802_11_DATA_RATE            host_CMD_802_11_DATA_RATE      
#define HostCmd_CMD_RF_REG_MAP                  host_CMD_RF_REG_MAP            
#define HostCmd_CMD_802_11_DEAUTHENTICATE       host_CMD_802_11_DEAUTHENTICATE 
#define HostCmd_CMD_802_11_REASSOCIATE          host_CMD_802_11_REASSOCIATE    
#define HostCmd_CMD_802_11_DISASSOCIATE         host_CMD_802_11_DISASSOCIATE   
#define HostCmd_CMD_MAC_CONTROL                 host_CMD_MAC_CONTROL           
#define HostCmd_CMD_802_11_AD_HOC_START         host_CMD_802_11_AD_HOC_START   
#define HostCmd_CMD_802_11_AD_HOC_JOIN          host_CMD_802_11_AD_HOC_JOIN    

#ifdef WPA
#define HostCmd_CMD_802_11_QUERY_RSN_OPTION	host_CMD_802_11_QUERY_RSN_OPTION	
#define HostCmd_CMD_802_11_QUERY_TKIP_REPLY_CNTRS host_CMD_802_11_QUERY_TKIP_REPLY_CNTRS	
#define HostCmd_CMD_802_11_ENABLE_RSN		host_CMD_802_11_ENABLE_RSN		
#define HostCmd_CMD_802_11_CONFIG_RSN		host_CMD_802_11_CONFIG_RSN		
#define HostCmd_CMD_802_11_UNICAST_CIPHER	host_CMD_802_11_UNICAST_CIPHER	
#define HostCmd_CMD_802_11_MCAST_CIPHER		host_CMD_802_11_MCAST_CIPHER	
#define HostCmd_CMD_802_11_RSN_AUTH_SUITES	host_CMD_802_11_RSN_AUTH_SUITES	
#define HostCmd_CMD_802_11_RSN_STATS		host_CMD_802_11_RSN_STATS		
#define HostCmd_CMD_802_11_PWK_KEY		host_CMD_802_11_PWK_KEY		
#define HostCmd_CMD_802_11_GRP_KEY		host_CMD_802_11_GRP_KEY		
#define HostCmd_CMD_802_11_PAIRWISE_TSC		host_CMD_802_11_PAIRWISE_TSC		
#define HostCmd_CMD_802_11_GROUP_TSC		host_CMD_802_11_GROUP_TSC		
#endif //end WPA


#if defined(DEEP_SLEEP_CMD) 
#define HostCmd_CMD_802_11_DEEP_SLEEP		host_CMD_802_11_DEEP_SLEEP	
#endif // DEEP_SLEEP_CMD

/* TODO: Move these down */
/* RSN Options */
#ifdef WPA
#define HostCmd_ENABLE_RSN_SUITES               0x0001
#define HostCmd_DISABLE_RSN_SUITES              0x0000
#endif //end WPA

#ifdef AUTO_FREQ_CTRL
#define HostCmd_CMD_802_11_SET_AFC 		0x003c
#define HostCmd_CMD_802_11_GET_AFC 		0x003d
#endif

#define	HostCmd_CMD_802_11_AD_HOC_STOP		host_CMD_802_11_AD_HOC_STOP

#ifdef HOST_WAKEUP
#define	HostCmd_CMD_802_11_HOST_WAKE_UP_CFG	0x0043
#define	HostCmd_CMD_802_11_HOST_AWAKE_CONFIRM	0x0044
#endif

#define HostCmd_CMD_802_11_PRE_TBTT            	host_CMD_802_11_PRE_TBTT     
                                                                                
#ifdef PASSTHROUGH_MODE                         
#define HostCmd_CMD_802_11_PASSTHROUGH          host_CMD_802_11_PASSTHROUGH  
#endif                                                                          
                                                                                
#define HostCmd_CMD_802_11_BEACON_STOP          host_CMD_802_11_DIS_BCN_TX
                                                                                
#define HostCmd_CMD_802_11_TX_MODE		host_CMD_802_11_TX_MODE	
#define HostCmd_CMD_802_11_RX_MODE		host_CMD_802_11_RX_MODE	
                                                                                
#define HostCmd_CMD_802_11_MAC_ADDRESS		host_CMD_802_11_MAC_ADDR	
                                                                                
#ifdef	DTIM_PERIOD                             
#define HostCmd_CMD_SET_DTIM_MULTIPLE		host_CMD_SET_MULTIPLE_DTIM	
#endif                                                                          
                                                                                
#ifdef  BCA                                     
#define HostCmd_CMD_BCA_CONFIG                  host_CMD_802_11_BNC_TUNE_OPT
#endif                                                                          

#define HostCmd_CMD_802_11_GENERATE_ATIM	0x0055
#define HostCmd_CMD_802_11_BEACON_CW		0x0056

#ifdef CAL_DATA
#define HostCmd_CMD_802_11_CAL_DATA		0x0057
#endif

#define HostCmd_CMD_802_11_BAND_CONFIG		0x0058
#define HostCmd_CMD_802_11_EEPROM_ACCESS	0x0059

#ifdef GSPI8385
#define HostCmd_CMD_GSPI_BUS_CONFIG             0x005A
#endif /* GSPI8385 */

#define HostCmd_CMD_802_11_SLEEP_PARAMS        	0x0066
#define HostCmd_CMD_802_11_BCA_CONFIG_TIMESHARE 0x0069

// For the IEEE Power Save
#define HostCmd_SubCmd_Enter_PS				0x0030
#define HostCmd_SubCmd_Exit_PS				0x0031
#define HostCmd_SubCmd_TxPend_PS			0x0032
#define HostCmd_SubCmd_ChangeSet_PS			0x0033
#define HostCmd_SubCmd_Sleep_Confirmed			0x0034
#define HostCmd_SubCmd_Full_PowerDown			0x0035
#define HostCmd_SubCmd_Full_PowerUp			0x0036
#define HostCmd_SubCmd_No_Tx_Pkt                	0x0037

//
//          16 bit RET code, MSB is set to 1
//
#define HostCmd_RET_NONE                        	0x8000                    
#define HostCmd_RET_HW_SPEC_INFO                	host_RET_GET_HW_SPEC             
#define HostCmd_RET_EEPROM_UPDATE               	host_RET_EEPROM_UPDATE             
#define HostCmd_RET_802_11_RESET                	host_RET_802_11_RESET               
#define HostCmd_RET_802_11_SCAN                 	host_RET_802_11_SCAN                 
#define HostCmd_RET_802_11_QUERY_TRAFFIC        	host_RET_802_11_QUERY_TRAFFIC 
#define HostCmd_RET_802_11_STATUS_INFO          	host_RET_802_11_STATUS_INFO    
#define HostCmd_RET_802_11_GET_LOG              	host_RET_802_11_GET_LOG         
#define HostCmd_RET_MAC_CONTROL                 	host_RET_MAC_CONTROL             
#define HostCmd_RET_MAC_MULTICAST_ADR           	host_RET_MAC_MULTICAST_ADR        
#define HostCmd_RET_802_11_AUTHENTICATE         	host_RET_802_11_AUTHENTICATE       
#define HostCmd_RET_802_11_DEAUTHENTICATE       	host_RET_802_11_DEAUTHENTICATE      
#define HostCmd_RET_802_11_ASSOCIATE            	host_RET_802_11_ASSOCIATE            
#define	HostCmd_RET_802_11_REASSOCIATE			host_RET_802_11_REASSOCIATE	
#define HostCmd_RET_802_11_DISASSOCIATE         	host_RET_802_11_DISASSOCIATE        
#define HostCmd_RET_802_11_SET_WEP              	host_RET_802_11_SET_WEP              
#define HostCmd_RET_802_11_STAT                 	host_RET_802_11_STAT             
#define HostCmd_RET_802_3_STAT                  	host_RET_802_3_STAT   
#define HostCmd_RET_802_11_SNMP_MIB             	host_RET_802_11_SNMP_MIB
#define HostCmd_RET_MAC_REG_MAP                 	host_RET_MAC_REG_MAP   
#define HostCmd_RET_BBP_REG_MAP                 	host_RET_BBP_REG_MAP    
#define HostCmd_RET_RF_REG_MAP                  	host_RET_RF_REG_MAP      
#define HostCmd_RET_MAC_REG_ACCESS              	host_RET_MAC_REG_ACCESS   
#define HostCmd_RET_BBP_REG_ACCESS              	host_RET_BBP_REG_ACCESS    
#define HostCmd_RET_RF_REG_ACCESS               	host_RET_RF_REG_ACCESS      
#define HostCmd_RET_802_11_RADIO_CONTROL        	host_RET_802_11_RADIO_CONTROL
#define HostCmd_RET_802_11_RF_CHANNEL           	host_RET_802_11_RF_CHANNEL    
#define HostCmd_RET_802_11_RSSI                 	host_RET_802_11_RSSI           
#define HostCmd_RET_802_11_RF_TX_POWER          	host_RET_802_11_RF_TX_POWER     
#define HostCmd_RET_802_11_RF_ANTENNA           	host_RET_802_11_RF_ANTENNA       
#ifdef PS_REQUIRED                                      
#define HostCmd_RET_802_11_PS_MODE              	host_RET_802_11_PS_MODE           
#endif                                                                                                  
#define HostCmd_RET_802_11_DATA_RATE            	host_RET_802_11_DATA_RATE          
                                                                                                        
#define HostCmd_RET_802_11_AD_HOC_START         	host_RET_802_11_AD_HOC_START        
#define HostCmd_RET_802_11_AD_HOC_JOIN          	host_RET_802_11_AD_HOC_JOIN          
                                                                                                        
#ifdef WPA                                              
#define HostCmd_RET_802_11_QUERY_RSN_OPTION             host_RET_802_11_QUERY_RSN_OPTION      
#define HostCmd_RET_802_11_QUERY_TKIP_REPLY_CNTRS       host_RET_802_11_QUERY_TKIP_REPLY_CNTRS 
#define HostCmd_RET_802_11_ENABLE_RSN                   host_RET_802_11_ENABLE_RSN    
#define HostCmd_RET_802_11_CONFIG_RSN                   host_RET_802_11_CONFIG_RSN     
#define HostCmd_RET_802_11_UNICAST_CIPHER               host_RET_802_11_UNICAST_CIPHER  
#define HostCmd_RET_802_11_MCAST_CIPHER                 host_RET_802_11_MCAST_CIPHER  
#define HostCmd_RET_802_11_RSN_AUTH_SUITES              host_RET_802_11_RSN_AUTH_SUITES
#define HostCmd_RET_802_11_RSN_STATS                    host_RET_802_11_RSN_STATS 
#define HostCmd_RET_802_11_PWK_KEY                      host_RET_802_11_PWK_KEY    
#define HostCmd_RET_802_11_GRP_KEY                      host_RET_802_11_GRP_KEY     
#define HostCmd_RET_802_11_PAIRWISE_TSC                 host_RET_802_11_PAIRWISE_TSC 
#define HostCmd_RET_802_11_GROUP_TSC                    host_RET_802_11_GROUP_TSC     

#define HostCmd_ENABLE_RSN                      	0x0001      
#define HostCmd_DISABLE_RSN                     	0x0000      
#define TYPE_ANTENNA_DIVERSITY                  	host_TYPE_ANTENNA_DIVERSITY    
                                                                                                        
#endif                                                                                                  
                                                                                                        
#define HostCmd_ACT_SET                         	host_ACT_SET            
#define HostCmd_ACT_GET                         	host_ACT_GET  

#define HostCmd_ACT_GET_AES				host_ACT_GET+2
#define HostCmd_ACT_SET_AES				host_ACT_SET+2
#define HostCmd_ACT_REMOVE_AES				host_ACT_SET+3
                                                                                                        
#if defined(DEEP_SLEEP_CMD)                             
#define HostCmd_RET_802_11_DEEP_SLEEP			host_RET_802_11_DEEP_SLEEP
#endif // DEEP_SLEEP_CMD                                
                                                                                                        
#ifdef AUTO_FREQ_CTRL
#define HostCmd_RET_802_11_SET_AFC 			0x803c
#define HostCmd_RET_802_11_GET_AFC 			0x803d
#endif

#define HostCmd_RET_802_11_AD_HOC_STOP			host_RET_802_11_AD_HOC_STOP
                                                                                                        
#ifdef HOST_WAKEUP                                    
#define	HostCmd_RET_802_11_HOST_WAKE_UP_CFG		0x8043
#define	HostCmd_RET_802_11_HOST_AWAKE_CONFIRM		0x8044
#endif
                                                                                                        
#define HostCmd_RET_802_11_PRE_TBTT			host_RET_802_11_PRE_TBTT
                                                                                                        
#ifdef PASSTHROUGH_MODE                                 
#define HostCmd_RET_802_11_PASSTHROUGH          	host_RET_802_11_PASSTHROUGH
#endif                                                                                                  
                                                                                                        
#define HostCmd_RET_802_11_BEACON_STOP			host_RET_CMD_802_11_DIS_BCN_TX
                                                                                                        
#define HostCmd_RET_802_11_TX_MODE			host_RET_802_11_TX_MODE
#define HostCmd_RET_802_11_RX_MODE			host_RET_802_11_RX_MODE	
                                                                                                        
#define HostCmd_RET_802_11_MAC_ADDRESS			host_RET_802_11_MAC_ADDR		
                                                                                                        
#ifdef	DTIM_PERIOD                                     
#define HostCmd_RET_SET_DTIM_MULTIPLE			host_RET_SET_DTIM_MULTIPLE			
#endif                                                                                                  
                                                                                                        
#ifdef  BCA                                             
#define HostCmd_RET_BCA_CONFIG                  	host_RET_802_11_BNC_TUNE_OPT                  	
#endif                                                                                                  

#define HostCmd_RET_802_11_GENERATE_ATIM		0x8055
#define HostCmd_RET_802_11_BEACON_CW			0x8056

#ifdef CAL_DATA
#define HostCmd_RET_802_11_CAL_DATA			0x8057
#endif

#ifdef MULTI_BANDS
#define HostCmd_RET_802_11_BAND_CONFIG			0x8058
#endif
#define HostCmd_RET_802_11_EEPROM_ACCESS		0x8059

#ifdef GSPI8385
#define HostCmd_RET_CMD_GSPI_BUS_CONFIG             	0x805A
#endif /* GSPI8385 */

#define HostCmd_RET_802_11_SLEEP_PARAMS        		0x8066
#define	HostCmd_RET_802_11_BCA_CONFIG_TIMESHARE		0x8069

/*
 * IEEE 802.11 OIDs
 */
#define OID_802_11_BSSID				0x00008001
#define OID_802_11_SSID					0x00008002
#define OID_802_11_NETWORK_TYPES_SUPPORTED		0x00008003
#define OID_802_11_NETWORK_TYPE_IN_USE			0x00008004
#define OID_802_11_RSSI					0x00008006
#define OID_802_11_RSSI_TRIGGER				0x00008007
#define OID_802_11_INFRASTRUCTURE_MODE			0x00008008
#define OID_802_11_FRAGMENTATION_THRESHOLD		0x00008009
#define OID_802_11_RTS_THRESHOLD			0x0000800A
#define OID_802_11_NUMBER_OF_ANTENNAS			0x0000800B
#define OID_802_11_RX_ANTENNA_SELECTED			0x0000800C
#define OID_802_11_TX_ANTENNA_SELECTED			0x0000800D
#define OID_802_11_SUPPORTED_RATES			0x0000800E
#define OID_802_11_DESIRED_RATES			0x00008010
#define OID_802_11_CONFIGURATION			0x00008011
#define OID_802_11_STATISTICS				0x00008012
#define OID_802_11_ADD_WEP				0x00008013
#define OID_802_11_REMOVE_WEP				0x00008014
#define OID_802_11_DISASSOCIATE				0x00008015
#define OID_802_11_POWER_MODE				0x00008016
#define OID_802_11_BSSID_LIST				0x00008017
#define OID_802_11_AUTHENTICATION_MODE			0x00008018
#define OID_802_11_PRIVACY_FILTER			0x00008019
#define OID_802_11_BSSID_LIST_SCAN			0x0000801A
#define OID_802_11_WEP_STATUS				0x0000801B
#define OID_802_11_RELOAD_DEFAULTS			0x0000801C
#define	OID_802_11_TX_RETRYCOUNT			0x0000801D

//
#define HostCmd_CMD_MFG_COMMAND				0x0080 
#define HostCmd_RET_MFG_COMMAND				0x8080 

//          Marvel specific OIDs
#define OID_MRVL_OEM_GET_ULONG				0xff010201
#define OID_MRVL_OEM_SET_ULONG				0xff010202
#define OID_MRVL_OEM_GET_STRING				0xff010203
#define OID_MRVL_OEM_COMMAND				0xff010204

//
//          Define Command Processing States and Options
//
#define HostCmd_STATE_IDLE                     		0x0000
#define HostCmd_STATE_IN_USE_BY_HOST           		0x0001
#define HostCmd_STATE_IN_USE_BY_MINIPORT 		0x0002
#define HostCmd_STATE_FINISHED 		                0x000f

#define HostCmd_Q_NONE                  	       	0x0000
#define HostCmd_Q_INIT                         		0x0001
#define HostCmd_Q_RESET 		                0x0002
#define HostCmd_Q_STAT                          	0x0003

//
//              Command pending states
//
#define HostCmd_PENDING_ON_NONE				0x0000
#define HostCmd_PENDING_ON_MISC_OP			0x0001
#define HostCmd_PENDING_ON_INIT				0x0002
#define HostCmd_PENDING_ON_RESET			0x0003
#define HostCmd_PENDING_ON_SET_OID              	0x0004
#define HostCmd_PENDING_ON_GET_OID              	0x0005
#define HostCmd_PENDING_ON_CMD                  	0x0006
#define HostCmd_PENDING_ON_STAT                 	0x0007

#define HostCmd_OPTION_USE_INT                  	0x0000
#define HostCmd_OPTION_NO_INT                   	0x0001
#ifdef 	PER_CMD_WAIT_Q
#define	HostCmd_OPTION_WAITFORRSP			0x0002
#endif	
#define HostCmd_DELAY_NORMAL                    	0x00000200      //  512 usec
#define HostCmd_DELAY_MIN                       	0x00000100      //  256 usec
#define HostCmd_DELAY_MAX                       	0x00000400      // 1024 usec

#define HostCmd_ENABLE_GENERATE_ATIM		0x0001
#define HostCmd_DISABLE_GENERATE_ATIM		0x0000

#endif /* end of USE_FW_HOST */

#endif /* FW_TYPES */
