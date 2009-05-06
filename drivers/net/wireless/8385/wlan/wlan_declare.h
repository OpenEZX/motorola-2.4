
int wlan_ret_game_mode(wlan_private *priv, HostCmd_DS_COMMAND *resp);

int wlan_ret_disable_game_data_tx(wlan_private *priv, 
						HostCmd_DS_COMMAND *resp);

inline int wlan_cmd_disable_game_data_tx(wlan_private *priv,
		HostCmd_DS_COMMAND *cmd, int CmdOption, int *flag);

inline int wlan_cmd_game_mode(wlan_private *priv,
		HostCmd_DS_COMMAND *cmd, int CmdOption,
			HostCmd_DS_802_11_GAME_MODE *game_config);

void GMConfirmSleep(wlan_private *priv);

inline int wlan_cmd_set_pre_tbtt(wlan_private *priv,
					HostCmd_DS_COMMAND *cmd, int *time);

int wlan_ret_set_pre_tbtt(wlan_private *priv, 
					HostCmd_DS_COMMAND *resp);

int wlan_lobby2_ioctl(wlan_private *priv, struct ifreq *req);
int wlan_ret_lobby2_scan(wlan_private *priv);
int wlan_ret_lobby2_pause_scan(wlan_private *priv);
int wlan_cmd_lobby2_scan(wlan_adapter *adapter, 
				HostCmd_DS_802_11_SCAN  *scan);

int AllocateAggrList(wlan_private *priv);

void FreeAggrList(wlan_adapter *Adapter);

int wlan_exit_extended_mode(wlan_private *priv);
int wlan_do_extended_ioctl(struct net_device *dev, 
					struct ifreq *req, int cmd);

inline int wlan_cmd_get_fw_game_stats(wlan_private *priv,
					HostCmd_DS_COMMAND *cmd);
int wlan_ret_get_fw_game_stats(wlan_private *priv, 
					HostCmd_DS_COMMAND *resp);
