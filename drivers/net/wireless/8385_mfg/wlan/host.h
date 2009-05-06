/*
 *  File: host.h
 *  Definitions of WLAN commands
 */

#ifndef _HOST_H_
#define _HOST_H_

/*
 *  PUBLIC DEFINITIONS
 */

#define DEFAULT_CHANNEL              1
#define DEFAULT_CHANNEL_A           36
#define DEFAULT_AD_HOC_CHANNEL       6
#define DEFAULT_AD_HOC_CHANNEL_A    36

/*
 *  IEEE 802.11 OIDs
 */

#define OID_802_11_BSSID                      0x00008001
#define OID_802_11_SSID                       0x00008002
#define OID_802_11_NETWORK_TYPES_SUPPORTED    0x00008003
#define OID_802_11_NETWORK_TYPE_IN_USE        0x00008004
#define OID_802_11_RSSI                       0x00008006
#define OID_802_11_RSSI_TRIGGER               0x00008007
#define OID_802_11_INFRASTRUCTURE_MODE        0x00008008
#define OID_802_11_FRAGMENTATION_THRESHOLD    0x00008009
#define OID_802_11_RTS_THRESHOLD              0x0000800A
#define OID_802_11_NUMBER_OF_ANTENNAS         0x0000800B
#define OID_802_11_RX_ANTENNA_SELECTED        0x0000800C
#define OID_802_11_TX_ANTENNA_SELECTED        0x0000800D
#define OID_802_11_SUPPORTED_RATES            0x0000800E
#define OID_802_11_DESIRED_RATES              0x00008010
#define OID_802_11_CONFIGURATION              0x00008011
#define OID_802_11_STATISTICS                 0x00008012
#define OID_802_11_ADD_WEP                    0x00008013
#define OID_802_11_REMOVE_WEP                 0x00008014
#define OID_802_11_DISASSOCIATE               0x00008015
#define OID_802_11_POWER_MODE                 0x00008016
#define OID_802_11_BSSID_LIST                 0x00008017
#define OID_802_11_AUTHENTICATION_MODE        0x00008018
#define OID_802_11_PRIVACY_FILTER             0x00008019
#define OID_802_11_BSSID_LIST_SCAN            0x0000801A
#define OID_802_11_WEP_STATUS                 0x0000801B
#define OID_802_11_RELOAD_DEFAULTS            0x0000801C
#define OID_802_11_TX_RETRYCOUNT              0x0000801D

#ifdef ENABLE_802_11D
#define OID_802_11D_ENABLE                    0x00008020
#endif

#ifdef ENABLE_802_11H_TPC
#define OID_802_11H_TPC_ENABLE                0x00008021
#endif

//#define HostCmd_CMD_MFG_COMMAND               0x0080
//#define HostCmd_RET_MFG_COMMAND               0x8080
#ifdef MFG_CMD_SUPPORT
#define HostCmd_CMD_MFG_COMMAND               0x0040
#define HostCmd_RET_MFG_COMMAND               0x8040
#endif

/* Marvel specific OIDs */
#define OID_MRVL_OEM_GET_ULONG                0xff010201
#define OID_MRVL_OEM_SET_ULONG                0xff010202
#define OID_MRVL_OEM_GET_STRING               0xff010203
#define OID_MRVL_OEM_COMMAND                  0xff010204

/*
 *  Define Command Processing States and Options
 */
#define HostCmd_STATE_IDLE                    0x0000
#define HostCmd_STATE_IN_USE_BY_HOST          0x0001
#define HostCmd_STATE_IN_USE_BY_MINIPORT      0x0002
#define HostCmd_STATE_FINISHED                0x000f

#define HostCmd_Q_NONE                        0x0000
#define HostCmd_Q_INIT                        0x0001
#define HostCmd_Q_RESET                       0x0002
#define HostCmd_Q_STAT                        0x0003

/*
 *  Command pending states
 */
#define HostCmd_PENDING_ON_NONE               0x0000
#define HostCmd_PENDING_ON_MISC_OP            0x0001
#define HostCmd_PENDING_ON_INIT               0x0002
#define HostCmd_PENDING_ON_RESET              0x0003
#define HostCmd_PENDING_ON_SET_OID            0x0004
#define HostCmd_PENDING_ON_GET_OID            0x0005
#define HostCmd_PENDING_ON_CMD                0x0006
#define HostCmd_PENDING_ON_STAT               0x0007

#define HostCmd_OPTION_USE_INT                0x0000
#define HostCmd_OPTION_NO_INT                 0x0001
#define HostCmd_OPTION_WAITFORRSP             0x0002
#define HostCmd_DELAY_NORMAL                  0x00000200      //  512 usec
#define HostCmd_DELAY_MIN                     0x00000100      //  256 usec
#define HostCmd_DELAY_MAX                     0x00000400      // 1024 usec

#define HostCmd_ENABLE_GENERATE_ATIM          0x0001
#define HostCmd_DISABLE_GENERATE_ATIM         0x0000

/*
 *  16 bit host command code - HHH updated on 110201
 */
#define HostCmd_CMD_NONE                      0x0000
#define HostCmd_CMD_CODE_DNLD                 0x0002
#define HostCmd_CMD_GET_HW_SPEC               0x0003
#define HostCmd_CMD_EEPROM_UPDATE             0x0004
#define HostCmd_CMD_802_11_RESET              0x0005
#define HostCmd_CMD_802_11_SCAN               0x0006
#define HostCmd_CMD_802_11_QUERY_TRAFFIC      0x0009
#define HostCmd_CMD_802_11_QUERY_STATUS       0x000a
#define HostCmd_CMD_802_11_GET_LOG            0x000b
#define HostCmd_CMD_MAC_MULTICAST_ADR         0x0010
#define HostCmd_CMD_802_11_AUTHENTICATE       0x0011
#define HostCmd_CMD_802_11_EEPROM_ACCESS      0x0059
#ifdef NEW_ASSOCIATION
#define HostCmd_CMD_802_11_ASSOCIATE          0x0050
#else
#define HostCmd_CMD_802_11_ASSOCIATE          0x0012
#endif
#define HostCmd_CMD_802_11_SET_WEP            0x0013
#define HostCmd_CMD_802_11_GET_STAT           0x0014
#define HostCmd_CMD_802_3_GET_STAT            0x0015
#define HostCmd_CMD_802_11_SNMP_MIB           0x0016
#define HostCmd_CMD_MAC_REG_MAP               0x0017
#define HostCmd_CMD_BBP_REG_MAP               0x0018
#define HostCmd_CMD_MAC_REG_ACCESS            0x0019
#define HostCmd_CMD_BBP_REG_ACCESS            0x001a
#define HostCmd_CMD_RF_REG_ACCESS             0x001b
#define HostCmd_CMD_802_11_RADIO_CONTROL      0x001c
#define HostCmd_CMD_802_11_RF_CHANNEL         0x001d
#define HostCmd_CMD_802_11_RF_TX_POWER        0x001e
#define HostCmd_CMD_802_11_RSSI               0x001f
#define HostCmd_CMD_802_11_RF_ANTENNA         0x0020

#ifdef PS_REQUIRED
#define HostCmd_CMD_802_11_PS_MODE            0x0021
#endif

#define HostCmd_CMD_802_11_DATA_RATE          0x0022
#define HostCmd_CMD_RF_REG_MAP                0x0023
#define HostCmd_CMD_802_11_DEAUTHENTICATE     0x0024
#define HostCmd_CMD_802_11_REASSOCIATE        0x0025
#define HostCmd_CMD_802_11_DISASSOCIATE       0x0026
#define HostCmd_CMD_MAC_CONTROL               0x0028
#define HostCmd_CMD_802_11_AD_HOC_START       0x002b
#define HostCmd_CMD_802_11_AD_HOC_JOIN        0x002c

#ifdef WPA
#define HostCmd_CMD_802_11_QUERY_RSN_OPTION        0x002d
#define HostCmd_CMD_802_11_QUERY_TKIP_REPLY_CNTRS  0x002e
#define HostCmd_CMD_802_11_ENABLE_RSN              0x002f
#ifndef WPA2
#define HostCmd_CMD_802_11_CONFIG_RSN         0x0030
#define HostCmd_CMD_802_11_UNICAST_CIPHER     0x0031
#define HostCmd_CMD_802_11_RSN_AUTH_SUITES    0x0032
#endif
#define HostCmd_CMD_802_11_RSN_STATS          0x0033
#define HostCmd_CMD_802_11_PWK_KEY            0x0034
#ifndef WPA2
#define HostCmd_CMD_802_11_GRP_KEY            0x0035
#endif
#define HostCmd_CMD_802_11_PAIRWISE_TSC       0x0036
#define HostCmd_CMD_802_11_GROUP_TSC          0x0037
#ifndef WPA2
#define HostCmd_CMD_802_11_MCAST_CIPHER       0x003a
#endif
#ifdef WPA2
#define HostCmd_CMD_802_11_KEY_MATERIAL       0x005e
#endif
#endif /* WPA */

#if defined(DEEP_SLEEP_CMD)
#define HostCmd_CMD_802_11_DEEP_SLEEP         0x003e
#endif /* DEEP_SLEEP_CMD */

/* TODO: Move these down */
/* RSN Options */
#ifdef WPA
#define HostCmd_ENABLE_RSN_SUITES             0x0001
#define HostCmd_DISABLE_RSN_SUITES            0x0000
#endif /* WPA */

#ifdef AUTO_FREQ_CTRL
#define HostCmd_CMD_802_11_SET_AFC            0x003c
#define HostCmd_CMD_802_11_GET_AFC            0x003d
#endif

#ifdef MFG_CMD_SUPPORT
#define HostCmd_CMD_802_11_AD_HOC_STOP	      0x0080
#else
#define HostCmd_CMD_802_11_AD_HOC_STOP        0x0040
#endif

#ifdef HOST_WAKEUP
#define HostCmd_CMD_802_11_HOST_WAKE_UP_CFG   0x0043
#define HostCmd_CMD_802_11_HOST_AWAKE_CONFIRM 0x0044
#endif

#ifdef PS_REQUIRED
#define HostCmd_CMD_802_11_PRE_TBTT           0x0047
#endif

#ifdef PASSTHROUGH_MODE
#define HostCmd_CMD_802_11_PASSTHROUGH        0x0048
#endif

#define HostCmd_CMD_802_11_BEACON_STOP        0x0049

#define HostCmd_CMD_802_11_TX_MODE            0x004A
#define HostCmd_CMD_802_11_RX_MODE            0x004B

#define HostCmd_CMD_802_11_MAC_ADDRESS        0x004D
#define HostCmd_CMD_802_11_EEPROM_ACCESS      0x0059

#ifdef GSPI8385
#define HostCmd_CMD_GSPI_BUS_CONFIG           0x005A
#endif /* GSPI8385 */

#ifdef PS_REQUIRED
#ifdef  DTIM_PERIOD
#define HostCmd_CMD_SET_DTIM_MULTIPLE         0x004f
#endif
#endif

#define HostCmd_CMD_802_11_GENERATE_ATIM      0x0055
#define HostCmd_CMD_802_11_BEACON_CW          0x0056

#ifdef CAL_DATA
#define HostCmd_CMD_802_11_CAL_DATA           0x0057
#endif

#define HostCmd_CMD_802_11_BAND_CONFIG        0x0058

#ifdef ENABLE_802_11D
#define HostCmd_CMD_802_11D_DOMAIN_INFO       0x005b
#endif

#ifdef ENABLE_802_11H_TPC
#define HostCmd_CMD_802_11H_TPC_INFO          0x005f
#define HostCmd_CMD_802_11H_TPC_REQUEST       0x0060
#endif

#ifdef  BCA
#define HostCmd_CMD_BCA_CONFIG                0x0065
#endif

#define HostCmd_CMD_802_11_SLEEP_PARAMS          0x0066

#ifdef V4_CMDS
#define HostCmd_CMD_802_11_INACTIVITY_TIMEOUT    0x0067
#endif

#define HostCmd_CMD_802_11_SLEEP_PERIOD          0x0068
#ifdef  BCA
#define HostCmd_CMD_802_11_BCA_CONFIG_TIMESHARE  0x0069
#endif
#ifdef	BG_SCAN
#define HostCmd_CMD_802_11_BG_SCAN_CONFIG	0x006b
#define HostCmd_CMD_802_11_BG_SCAN_QUERY	0x006c
#endif /* BG_SCAN */
/*
 *  For the IEEE Power Save
 */
#define HostCmd_SubCmd_Enter_PS               0x0030
#define HostCmd_SubCmd_Exit_PS                0x0031
#define HostCmd_SubCmd_TxPend_PS              0x0032
#define HostCmd_SubCmd_ChangeSet_PS           0x0033
#define HostCmd_SubCmd_Sleep_Confirmed        0x0034
#define HostCmd_SubCmd_Full_PowerDown         0x0035
#define HostCmd_SubCmd_Full_PowerUp           0x0036
#define HostCmd_SubCmd_No_Tx_Pkt              0x0037

/*
 *  Obsolete CMD code
 */
#define HostCmd_CMD_802_11_QUERY_AP           0x0007
#define HostCmd_CMD_802_11_QUERY_IBSS_STA     0x0008
#define HostCmd_CMD_MAC_TX_ENABLE             0x000c
#define HostCmd_CMD_MAC_RX_ENABLE             0x000d
#define HostCmd_CMD_MAC_LOOP_BACK_ENABLE      0x000e
#define HostCmd_CMD_MAC_INT_ENABLE            0x000f
//#endif

/*
 *  16 bit RET code, MSB is set to 1
 */
#define HostCmd_RET_NONE                      0x8000
#define HostCmd_RET_HW_SPEC_INFO              0x8003
#define HostCmd_RET_EEPROM_UPDATE             0x8004
#define HostCmd_RET_802_11_RESET              0x8005
#define HostCmd_RET_802_11_SCAN               0x8006
#define HostCmd_RET_802_11_QUERY_TRAFFIC      0x8009
#define HostCmd_RET_802_11_STATUS_INFO        0x800a
#define HostCmd_RET_802_11_GET_LOG            0x800b
#define HostCmd_RET_MAC_CONTROL               0x8028
#define HostCmd_RET_MAC_MULTICAST_ADR         0x8010
#define HostCmd_RET_802_11_AUTHENTICATE       0x8011
#define HostCmd_RET_802_11_DEAUTHENTICATE     0x8024
#define HostCmd_RET_802_11_ASSOCIATE          0x8012
#define HostCmd_RET_802_11_REASSOCIATE        0x8025
#define HostCmd_RET_802_11_DISASSOCIATE       0x8026
#define HostCmd_RET_802_11_SET_WEP            0x8013
#define HostCmd_RET_802_11_STAT               0x8014
#define HostCmd_RET_802_3_STAT                0x8015
#define HostCmd_RET_802_11_SNMP_MIB           0x8016
#define HostCmd_RET_MAC_REG_MAP               0x8017
#define HostCmd_RET_BBP_REG_MAP               0x8018
#define HostCmd_RET_RF_REG_MAP                0x8023
#define HostCmd_RET_MAC_REG_ACCESS            0x8019
#define HostCmd_RET_BBP_REG_ACCESS            0x801a
#define HostCmd_RET_RF_REG_ACCESS             0x801b
#define HostCmd_RET_802_11_RADIO_CONTROL      0x801c
#define HostCmd_RET_802_11_RF_CHANNEL         0x801d
#define HostCmd_RET_802_11_RSSI               0x801f
#define HostCmd_RET_802_11_RF_TX_POWER        0x801e
#define HostCmd_RET_802_11_RF_ANTENNA         0x8020
#ifdef PS_REQUIRED
#define HostCmd_RET_802_11_PS_MODE            0x8021
#endif
#define HostCmd_RET_802_11_DATA_RATE          0x8022

#define HostCmd_RET_802_11_AD_HOC_START       0x802B
#define HostCmd_RET_802_11_AD_HOC_JOIN        0x802C

#ifdef WPA
#define HostCmd_RET_802_11_QUERY_RSN_OPTION        0x802d
#define HostCmd_RET_802_11_QUERY_TKIP_REPLY_CNTRS  0x802e
#define HostCmd_RET_802_11_ENABLE_RSN              0x802f
#ifndef WPA2
#define HostCmd_RET_802_11_CONFIG_RSN         0x8030
#define HostCmd_RET_802_11_UNICAST_CIPHER     0x8031
#define HostCmd_RET_802_11_RSN_AUTH_SUITES    0x8032
#endif
#define HostCmd_RET_802_11_RSN_STATS          0x8033
#define HostCmd_RET_802_11_PWK_KEY            0x8034
#ifndef WPA2
#define HostCmd_RET_802_11_GRP_KEY            0x8035
#endif
#define HostCmd_RET_802_11_PAIRWISE_TSC       0x8036
#define HostCmd_RET_802_11_GROUP_TSC          0x8037
#ifndef WPA2
#define HostCmd_RET_802_11_MCAST_CIPHER       0x803a
#endif
#ifdef WPA2
#define HostCmd_RET_802_11_KEY_MATERIAL       0x805e
#endif

#define HostCmd_ENABLE_RSN                    0x0001
#define HostCmd_DISABLE_RSN                   0x0000
#define TYPE_ANTENNA_DIVERSITY                0xffff

#endif

#define HostCmd_ACT_SET                       0x0001
#define HostCmd_ACT_GET                       0x0000

#define HostCmd_ACT_GET_AES                   (HostCmd_ACT_GET + 2)
#define HostCmd_ACT_SET_AES                   (HostCmd_ACT_SET + 2)
#define HostCmd_ACT_REMOVE_AES                (HostCmd_ACT_SET + 3)

#if defined(DEEP_SLEEP_CMD)
#define HostCmd_RET_802_11_DEEP_SLEEP         0x803e
#endif /* DEEP_SLEEP_CMD */

#ifdef AUTO_FREQ_CTRL
#define HostCmd_RET_802_11_SET_AFC            0x803c
#define HostCmd_RET_802_11_GET_AFC            0x803d
#endif
#ifdef MFG_CMD_SUPPORT
#define HostCmd_RET_802_11_AD_HOC_STOP	      0x8080
#else
#define HostCmd_RET_802_11_AD_HOC_STOP        0x8040
#endif

#ifdef HOST_WAKEUP
#define HostCmd_RET_802_11_HOST_WAKE_UP_CFG   0x8043
#define HostCmd_RET_802_11_HOST_AWAKE_CONFIRM 0x8044
#endif

#ifdef PS_REQUIRED
#define HostCmd_RET_802_11_PRE_TBTT           0x8047
#endif

#ifdef PASSTHROUGH_MODE
#define HostCmd_RET_802_11_PASSTHROUGH        0x8048
#endif

#define HostCmd_RET_802_11_BEACON_STOP        0x8049

#define HostCmd_RET_802_11_TX_MODE            0x804A
#define HostCmd_RET_802_11_RX_MODE            0x804B

#define HostCmd_RET_802_11_MAC_ADDRESS        0x804D
#define HostCmd_RET_802_11_EEPROM_ACCESS      0x8059

#ifdef GSPI8385
#define HostCmd_RET_CMD_GSPI_BUS_CONFIG       0x805A
#endif /* GSPI8385 */

#ifdef PS_REQUIRED
#ifdef  DTIM_PERIOD
#define HostCmd_RET_SET_DTIM_MULTIPLE         0x804f
#endif
#endif

#define HostCmd_RET_802_11_GENERATE_ATIM      0x8055
#define HostCmd_RET_802_11_BEACON_CW          0x8056

#ifdef CAL_DATA
#define HostCmd_RET_802_11_CAL_DATA           0x8057
#endif

#define HostCmd_RET_802_11_BAND_CONFIG        0x8058

#ifdef  BCA
#define HostCmd_RET_BCA_CONFIG                0x8065
#endif

#define HostCmd_RET_802_11_SLEEP_PARAMS          0x8066

#ifdef V4_CMDS
#define HostCmd_RET_802_11_INACTIVITY_TIMEOUT    0x8067
#endif

#define HostCmd_RET_802_11_SLEEP_PERIOD          0x8068
#ifdef  BCA
#define HostCmd_RET_802_11_BCA_CONFIG_TIMESHARE  0x8069
#endif

#ifdef ENABLE_802_11D
#define HostCmd_RET_802_11D_DOMAIN_INFO      (0x8000 |                  \
					      HostCmd_CMD_802_11D_DOMAIN_INFO)
#endif

#ifdef ENABLE_802_11H_TPC
#define HostCmd_RET_802_11H_TPC_INFO         (0x8000 |                  \
					      HostCmd_CMD_802_11H_TPC_INFO)
#define HostCmd_RET_802_11H_TPC_REQUEST      (0x8000 |                  \
					      HostCmd_CMD_802_11H_TPC_REQUEST)
#endif

#ifdef	BG_SCAN
#define HostCmd_RET_802_11_BG_SCAN_CONFIG	0x806b
#define HostCmd_RET_802_11_BG_SCAN_QUERY	0x806c
#endif /* BG_SCAN */

/*
 *  Define general result code for each command
 */
#define HostCmd_RESULT_OK                    0x0000  // OK
#define HostCmd_RESULT_ERROR                 0x0001  // Genenral error
#define HostCmd_RESULT_NOT_SUPPORT           0x0002  // Command is not valid
#define HostCmd_RESULT_PENDING               0x0003  // Command is pending
// System is busy (command ignored)
#define HostCmd_RESULT_BUSY                  0x0004
// Data buffer is not big enough
#define HostCmd_RESULT_PARTIAL_DATA          0x0005

/*
 *  Definition of action or option for each command
 */

/* Define general purpose action */
#define HostCmd_ACT_GEN_READ                    0x0000
#define HostCmd_ACT_GEN_WRITE                   0x0001
#define HostCmd_ACT_GEN_GET                     0x0000
#define HostCmd_ACT_GEN_SET                     0x0001
#define HostCmd_ACT_GEN_OFF                     0x0000
#define HostCmd_ACT_GEN_ON                      0x0001

/* Define action or option for HostCmd_CMD_802_11_AUTHENTICATE */
#define HostCmd_ACT_AUTHENTICATE                0x0001
#define HostCmd_ACT_DEAUTHENTICATE              0x0002

/* Define action or option for HostCmd_CMD_802_11_ASSOCIATE */
#define HostCmd_ACT_ASSOCIATE                   0x0001
#define HostCmd_ACT_DISASSOCIATE                0x0002
#define HostCmd_ACT_REASSOCIATE                 0x0003

#define HostCmd_CAPINFO_ESS                     0x0001
#define HostCmd_CAPINFO_IBSS                    0x0002
#define HostCmd_CAPINFO_CF_POLLABLE             0x0004
#define HostCmd_CAPINFO_CF_REQUEST              0x0008
#define HostCmd_CAPINFO_PRIVACY                 0x0010

/* Define action or option for HostCmd_CMD_802_11_SET_WEP */
#define HostCmd_ACT_ADD                         0x0002
#define HostCmd_ACT_REMOVE                      0x0004
#define HostCmd_ACT_USE_DEFAULT                 0x0008

#define HostCmd_TYPE_WEP_40_BIT                 0x0001  // 40 bit
#define HostCmd_TYPE_WEP_104_BIT                0x0002  // 104 bit

#define HostCmd_NUM_OF_WEP_KEYS                 4

#define HostCmd_WEP_KEY_INDEX_MASK              0x3fffffff


/* Define action or option for HostCmd_CMD_802_11_RESET */
#define HostCmd_ACT_NOT_REVERT_MIB              0x0001
#define HostCmd_ACT_REVERT_MIB                  0x0002
#define HostCmd_ACT_HALT                        0x0003

/* Define action or option for HostCmd_CMD_802_11_SCAN */
#define HostCmd_BSS_TYPE_BSS                    0x0001
#define HostCmd_BSS_TYPE_IBSS                   0x0002
#define HostCmd_BSS_TYPE_ANY                    0x0003

/* Define action or option for HostCmd_CMD_802_11_SCAN */
#define HostCmd_SCAN_TYPE_ACTIVE                0x0000
#define HostCmd_SCAN_TYPE_PASSIVE               0x0001

#define HostCmd_SCAN_802_11_B_CHANNELS          11

#define HostCmd_SCAN_MIN_CH_TIME                6

#ifdef MULTI_BANDS
#define HostCmd_SCAN_MAX_CH_TIME_MULTI_BANDS    20
#endif

#ifdef FWVERSION3
#define HostCmd_SCAN_MAX_CH_TIME                100
#endif

#ifdef TLV_SCAN
#define HostCmd_SCAN_RADIO_TYPE_BG		0
#define HostCmd_SCAN_RADIO_TYPE_A		1
#define HostCmd_SCAN_PASSIVE_MAX_CH_TIME        100
#endif

#define HostCmd_SCAN_PROBE_DELAY_TIME           0

/* Define action or option for HostCmd_CMD_802_11_QUERY_STATUS */
#define HostCmd_STATUS_FW_INIT                  0x0000
#define HostCmd_STATUS_FW_IDLE                  0x0001
#define HostCmd_STATUS_FW_WORKING               0x0002
#define HostCmd_STATUS_FW_ERROR                 0x0003
#define HostCmd_STATUS_FW_POWER_SAVE            0x0004

#define HostCmd_STATUS_MAC_RX_ON                0x0001
#define HostCmd_STATUS_MAC_TX_ON                0x0002
#define HostCmd_STATUS_MAC_LOOP_BACK_ON         0x0004
#define HostCmd_STATUS_MAC_WEP_ENABLE           0x0008
#define HostCmd_STATUS_MAC_INT_ENABLE           0x0010

/* Define action or option for HostCmd_CMD_MAC_CONTROL */
#define HostCmd_ACT_MAC_RX_ON                   0x0001
#define HostCmd_ACT_MAC_TX_ON                   0x0002
#define HostCmd_ACT_MAC_LOOPBACK_ON             0x0004
#define HostCmd_ACT_MAC_WEP_ENABLE              0x0008
#define HostCmd_ACT_MAC_INT_ENABLE              0x0010
#define HostCmd_ACT_MAC_MULTICAST_ENABLE        0x0020
#define HostCmd_ACT_MAC_BROADCAST_ENABLE        0x0040
#define HostCmd_ACT_MAC_PROMISCUOUS_ENABLE      0x0080
#define HostCmd_ACT_MAC_ALL_MULTICAST_ENABLE    0x0100
#ifdef WPA2
#define HostCmd_ACT_MAC_STRICT_PROTECTION_ENABLE  0x0400
#endif

/* Define action or option or constant for HostCmd_CMD_MAC_MULTICAST_ADR */
#define HostCmd_SIZE_MAC_ADR                    6
#define HostCmd_MAX_MCAST_ADRS                  32

/* Define action or option for HostCmd_CMD_802_11_SNMP_MIB */
#define HostCmd_TYPE_MIB_FLD_BOOLEAN            0x0001  // Boolean
#define HostCmd_TYPE_MIB_FLD_INTEGER            0x0002  // 32 u8 unsigned integer
#define HostCmd_TYPE_MIB_FLD_COUNTER            0x0003  // Counter
#define HostCmd_TYPE_MIB_FLD_OCT_STR            0x0004  // Octet string
#define HostCmd_TYPE_MIB_FLD_DISPLAY_STR        0x0005  // String
#define HostCmd_TYPE_MIB_FLD_MAC_ADR            0x0006  // MAC address
#define HostCmd_TYPE_MIB_FLD_IP_ADR             0x0007  // IP address
#define HostCmd_TYPE_MIB_FLD_WEP                0x0008  // WEP

/* Define action or option for HostCmd_CMD_802_11_RADIO_CONTROL */
#define HostCmd_TYPE_AUTO_PREAMBLE              0x0001
#define HostCmd_TYPE_SHORT_PREAMBLE             0x0002
#define HostCmd_TYPE_LONG_PREAMBLE              0x0003

#define TURN_ON_RF                              0x01
#define RADIO_ON                                0x01
#define RADIO_OFF                               0x00

#define SET_AUTO_PREAMBLE                       0x05
#define SET_SHORT_PREAMBLE                      0x03
#define SET_LONG_PREAMBLE                       0x01
/* Define action or option for CMD_802_11_RF_CHANNEL */
#define HostCmd_TYPE_802_11A                    0x0001
#define HostCmd_TYPE_802_11B                    0x0002

/* Define action or option for HostCmd_CMD_802_11_RF_TX_POWER */
#define HostCmd_ACT_TX_POWER_OPT_GET            0x0000
#define HostCmd_ACT_TX_POWER_OPT_SET_HIGH       0x8007
#define HostCmd_ACT_TX_POWER_OPT_SET_MID        0x8004
#define HostCmd_ACT_TX_POWER_OPT_SET_LOW        0x8000

#define HostCmd_ACT_TX_POWER_INDEX_HIGH         0x0007
#define HostCmd_ACT_TX_POWER_INDEX_MID          0x0004
#define HostCmd_ACT_TX_POWER_INDEX_LOW          0x0000

/* Define action or option for HostCmd_CMD_802_11_DATA_RATE */
#define HostCmd_ACT_SET_TX_AUTO                 0x0000
#define HostCmd_ACT_SET_TX_FIX_RATE             0x0001
#define HostCmd_ACT_GET_TX_RATE                 0x0002

#define HostCmd_ACT_SET_RX                      0x0001
#define HostCmd_ACT_SET_TX                      0x0002
#define HostCmd_ACT_SET_BOTH                    0x0003
#define HostCmd_ACT_GET_RX                      0x0004
#define HostCmd_ACT_GET_TX                      0x0008
#define HostCmd_ACT_GET_BOTH                    0x000c

//#define TYPE_ANTENNA_DIVERSITY                0xffff

#ifdef PS_REQUIRED
/* Define action or option for HostCmd_CMD_802_11_PS_MODE */
#define HostCmd_TYPE_CAM                        0x0000
#define HostCmd_TYPE_MAX_PSP                    0x0001
#define HostCmd_TYPE_FAST_PSP                   0x0002
#endif

/* Card Event definition */
#define MACREG_INT_CODE_TX_PPA_FREE             0x00000000
#define MACREG_INT_CODE_TX_DMA_DONE             0x00000001
#define MACREG_INT_CODE_LINK_LOSE_W_SCAN        0x00000002
#define MACREG_INT_CODE_LINK_LOSE_NO_SCAN       0x00000003
#define MACREG_INT_CODE_LINK_SENSED             0x00000004
#define MACREG_INT_CODE_CMD_FINISHED            0x00000005
#define MACREG_INT_CODE_MIB_CHANGED             0x00000006
#define MACREG_INT_CODE_INIT_DONE               0x00000007
#define MACREG_INT_CODE_DEAUTHENTICATED         0x00000008
#define MACREG_INT_CODE_DISASSOCIATED           0x00000009
#define MACREG_INT_CODE_PS_AWAKE                0x0000000a
#define MACREG_INT_CODE_PS_SLEEP                0x0000000b
#define MACREG_INT_CODE_DUMMY_PKT               0x0000000c
#define MACREG_INT_CODE_MIC_ERR_MULTICAST       0x0000000d
#define MACREG_INT_CODE_MIC_ERR_UNICAST         0x0000000e
#define MACREG_INT_CODE_WM_AWAKE                0x0000000f
#ifdef DEEP_SLEEP
#define MACREG_INT_CODE_DEEP_SLEEP_AWAKE        0x00000010
#endif
#define MACREG_INT_CODE_ADHOC_BCN_LOST          0x00000011
#ifdef HOST_WAKEUP
#define MACREG_INT_CODE_HOST_WAKE_UP            0x00000012
#endif
#ifdef BG_SCAN
#define MACREG_INT_CODE_BG_SCAN_REPORT		0x00000013 
#endif /* BG_SCAN */


/*
 *  Define bitmap conditions for HOST_WAKE_UP_CFG
 */
#define HOST_WAKE_UP_CFG_NON_UNICAST_DATA       0x0001
#define HOST_WAKE_UP_CFG_UNICAST_DATA           0x0002
#define HOST_WAKE_UP_CFG_MAC_EVENT              0x0004

/*
 *  HOST COMMAND DEFINITIONS
 */

/* TLV  type ID definition */
#ifdef TLV_SCAN
#define TLV_TYPE_SSID                          0x0000
#define TLV_TYPE_RATES                         0x0001
#define TLV_TYPE_CHANLIST                      0x0101
#endif

#ifdef ENABLE_802_11D
#define TLV_TYPE_DOMAIN                        0x0007
#endif

#ifdef WPA2
#define TLV_TYPE_KEY_MATERIAL                  0x0100
#endif

#ifdef ENABLE_802_11H_TPC
#define TLV_TYPE_POWER_CONSTRAINT              0x0020
#define TLV_TYPE_POWER_CAPABILITY              0x0021
#endif

#endif /* _HOST_H_ */
