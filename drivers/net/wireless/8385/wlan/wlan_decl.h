/*
 *  File: wlan_decl.h
 */

#ifndef _WLAN_DECL_H_
#define _WLAN_DECL_H_

void MacEventDisconnected(wlan_private *priv);
int wlan_tx_packet(wlan_private *priv, struct sk_buff *skb);
void wlan_free_adapter(wlan_private *priv);
int wlan_hard_start_xmit(struct sk_buff *skb, struct net_device *dev);
struct sk_buff* wlan_pop_rx_skb(struct sk_buff *RxSkbQ); 
int SetMacPacketFilter(wlan_private *priv);
int wlan_Start_Tx_Packet(wlan_private *priv,int pktlen);
int MrvDrvSend (wlan_private *priv, struct sk_buff *skb);
int SendSinglePacket(wlan_private *priv, struct sk_buff *skb);
#ifdef WMM_UAPSD
int SendNullPacket(wlan_private *priv, u8 pwr_mgmt);
int wlan_null_pkg_gen( wlan_private *priv, struct iwreq *wrq );
#endif
void Wep_encrypt(wlan_private *priv,  u8* Buf, u32 Len);
int FreeCmdBuffer(wlan_private *priv);
void CleanUpCmdCtrlNode(CmdCtrlNode *pTempNode);
CmdCtrlNode *GetFreeCmdCtrlNode(wlan_private *priv);
void SetCmdCtrlNode(wlan_private *priv,
			CmdCtrlNode *pTempNode, WLAN_OID PendingOID,
			u16 PendingInfo, u16 INTOption, void *InfoBuf);
void QueueCmd(wlan_adapter *Adapter, CmdCtrlNode *CmdNode, BOOLEAN addtail);
int PrepareAndSendCommand(wlan_private *priv, u16 cmdno, u16 cmd_option,
				u16 intoption, WLAN_OID PendingOID,
				u16 PendingInfo,
				void *InformationBuffer);
int DownloadCommandToStation(wlan_private *priv, CmdCtrlNode *CmdNode);
int JoinAdhocNetwork(wlan_private *priv, WLAN_802_11_SSID *AdhocSSID, int i);
int StartAdhocNetwork(wlan_private *priv, WLAN_802_11_SSID *AdhocSSID);
int SendSpecificScan(wlan_private *priv, WLAN_802_11_SSID *RequestedSSID);
int ResetSingleTxDoneAck(wlan_private *priv);
int AllocateCmdBuffer(wlan_private *priv);
int ExecuteNextCommand(wlan_private *priv);
int wlan_process_event(wlan_private *priv);
void wlan_interrupt(struct net_device *);
int wlan_scan_networks(wlan_private *priv, u16 pending_info);
int wlan_associate(wlan_private *priv, WLAN_802_11_SSID *ssid);
int SetRadioControl( wlan_private *priv );
inline int get_common_rates(wlan_adapter *Adapter, u8 *dest, int size1, 
				u8 *card_rates, int size2);
u32 index_to_data_rate(u8 index);
u8 data_rate_to_index(u32 rate);
void HexDump(char *prompt, u8 *data, int len);
void get_version(wlan_adapter *adapter, char *version, int maxlen);
void wlan_read_write_rfreg(wlan_private *priv);
/* The proc fs interface */
int wlan_proc_read(char *page, char **start, off_t offset,
		 int count, int *eof, void *data);
void wlan_proc_entry(wlan_private *priv, struct net_device *dev);
void wlan_proc_remove(char *name);
void wlan_debug_entry(wlan_private *priv, struct net_device *dev);
void wlan_debug_remove(wlan_private *priv);
int wlan_set_mac_address(struct net_device *dev, void *addr);
int wlan_process_rx_command(wlan_private * priv);
void wlan_process_tx(wlan_private * priv);
void CleanupAndInsertCmd(wlan_private * priv, CmdCtrlNode * pTempCmd);
int  wlan_setup_station_hw(wlan_private * priv);
int  wlan_allocate_adapter(wlan_private * priv);
void wlan_init_adapter(wlan_private * priv);
int  init_sync_objects(wlan_private * priv);
int  wlan_dnld_ready(wlan_private * priv);
void MrvDrvTimerFunction(void *FunctionContext);
inline int wlan_scan_mode_ioctl(wlan_private *priv,
				struct iwreq *wrq);
int wlan_set_rate(struct net_device *dev, struct iw_request_info *info, 
					struct iw_param *vwrq, char *extra);
int wlan_get_rate(struct net_device *dev, struct iw_request_info *info, 
					struct iw_param *vwrq, char *extra);
int wlan_rfi_txmode(wlan_private *priv, struct iwreq *wrq);
int wlan_tfi_txmode(wlan_private *priv, struct iwreq *wrq);
int wlan_set_regiontable(wlan_private *priv, u8 region, u8 band);

#ifdef DEEP_SLEEP
int wlan_deep_sleep_ioctl(wlan_private *priv, struct ifreq *rq);
#endif /* DEEP_SLEEP */

#ifdef PASSTHROUGH_MODE
int             AllocatePassthroughList(wlan_private * priv);
void            FreePassthroughList(wlan_adapter * Adapter);
#endif /* PASSTHROUGH_MODE */

#ifdef WPA
void            DisplayTkip_MicKey(HostCmd_DS_802_11_PWK_KEY * pCmdPwk);
#endif /* WPA */

#ifndef	MULTIPLERX_SUPPORT
int ProcessRxed_802_3_Packet(wlan_private *priv, struct sk_buff *);
#define	ProcessRxedPacket	ProcessRxed_802_3_Packet
#else
int ProcessRxedPacket(wlan_private *priv, RxPD *pRxPD);
#endif /* MULTIPLERX_SUPPORT */

#ifdef ADHOCAES
int HandleAdhocEncryptResponse(wlan_private *priv, 
					PHostCmd_DS_802_11_PWK_KEY pEncKey);
int SetupAdhocEncryptionKey(wlan_private *priv, PHostCmd_DS_802_11_PWK_KEY pEnc,
		int CmdOption, char *InformationBuffer);
#endif /* ADHOCAES */

#ifdef PS_REQUIRED
int AllocatePSConfirmBuffer(wlan_private *priv);
void PSSleep(wlan_private *priv, u16 PSMode, int wait_option);
void PSConfirmSleep(wlan_private *priv, u16 PSMode);
void PSWakeup(wlan_private *priv, int wait_option);
int SendConfirmSleep(wlan_private * priv, u8 * CmdPtr, u16 size);
#ifdef BULVERDE_SDIO
void sdio_clear_imask(mmc_controller_t);
int  sdio_check_idle_state(mmc_controller_t);
void sdio_print_imask(mmc_controller_t);
void sdio_clear_imask(mmc_controller_t);
#endif /* BULVERDE_SDIO */
#endif /* PS_REQUIRED */

#ifdef BULVERDE_SDIO
int start_bus_clock(mmc_controller_t);
int stop_bus_clock_2(mmc_controller_t);
#endif /* BULVERDE_SDIO */

#ifdef BCA
int wlan_bca_ioctl(wlan_private *priv, struct iwreq *wrq);
int wlan_bca_timeshare_ioctl(wlan_private *priv, struct iwreq *wrq);
#endif /* BCA */

#ifdef PASSTHROUGH_MODE
int GetPassthroughPacket(wlan_adapter * Adapter, u8 ** ptr, int *payload);
#endif /* PASSTHROUGH_MODE */

#ifdef BG_SCAN 
int wlan_bg_scan_enable(wlan_private *priv, BOOLEAN enable);
#endif /* BG_SCAN */

#ifdef BULVERDE_SDIO
int wlan_sdio_clock(wlan_private *priv, BOOLEAN on);
#endif /* BULVERDE_SDIO */

#ifdef ADHOC_GRATE	
int wlan_do_set_grate_ioctl(wlan_private *priv, struct iwreq *wrq);
#endif

#ifdef PS_REQUIRED
#ifdef FW_WAKEUP_METHOD
int wlan_cmd_fw_wakeup_method( wlan_private *priv, struct iwreq *wrq );
#endif
#endif

/* adds wlan_txcontrol() for TxControl ioctl cmd */
int wlan_txcontrol(wlan_private *priv, struct iwreq *wrq);

int wlan_atimgen( wlan_private *priv, struct iwreq *wrq );
		
static inline void wlan_send_rxskbQ(wlan_private *priv)
{
	struct sk_buff	*skb;

	if (priv->adapter)
		while((skb = wlan_pop_rx_skb(&priv->adapter->RxSkbQ)))
			ProcessRxedPacket(priv, skb);
}

#if WIRELESS_EXT > 14
void send_iwevcustom_event(wlan_private *priv, char *str);
#endif

#ifdef READ_HELPER_FW
int fw_read( char *name, u8 **addr, u32 *len );
#endif

#endif /* _WLAN_DECL_H_ */
