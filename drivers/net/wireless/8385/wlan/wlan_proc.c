/*
 * 	File: If the proc interface defined than lets do this!! 
 *
 * (c) Copyright � 2003-2006, Marvell International Ltd. 
 *
 * (c) Copyright 2006, Motorola, Inc.
 *
 * This software file (the "File") is distributed by Marvell International 
 * Ltd. under the terms of the GNU General Public License Version 2, June 1991 
 * (the "License").  You may use, redistribute and/or modify this File in 
 * accordance with the terms and conditions of the License, a copy of which 
 * is available along with the File in the license.txt file or by writing to 
 * the Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 
 * 02111-1307 or on the worldwide web at http://www.gnu.org/licenses/gpl.txt.
 *
 * THE FILE IS DISTRIBUTED AS-IS, WITHOUT WARRANTY OF ANY KIND, AND THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE 
 * ARE EXPRESSLY DISCLAIMED.  The License provides additional details about 
 * this warranty disclaimer.
 *
 *
 * 2006-Jul-07 Motorola - Add power management related changes.
 */
#include 	"include.h"



static char    *szModes[] = {
	"Ad-hoc",
	"Managed",
	"Auto"
};

static char    *szStates[] = {
	"Connected",
	"Disconnected"
};

/*----------------------------------------------------------------
* proc_read
*
* Read function for /proc/net/wlan/<device>/dev
*
* Arguments:
*	buf
*	start 
*	offset 
*	count
*	eof
*	data
* Returns: 
*	zero on success, non-zero otherwise.
* Call Context:
*	Can be either interrupt or not.
----------------------------------------------------------------*/
// TODO Fill in all the required fields in the proc interface! 
// Its just a skeleton , need to push some more stuff


static int wlan_proc_read(char *page, char **start, off_t offset,
	       int count, int *eof, void *data)
{
#ifdef CONFIG_PROC_FS
	int             	i;
	char           		*p = page;
	struct net_device 	*netdev = data;
	struct dev_mc_list 	*mcptr = netdev->mc_list;
	char           		fmt[64];
	wlan_private		*priv = netdev->priv;
#endif

	if (offset != 0) {
		*eof = 1;
		goto exit;
	}

	get_version(priv->adapter, fmt, sizeof(fmt) - 1);
	
	p += sprintf(p, "driver_name = " "\"SD83xx\"\n");
	p += sprintf(p, "driver_version = %s", fmt);
	p += sprintf(p, "\nInterfaceName=\"%s\"\n", netdev->name);
	p += sprintf(p, "Mode=\"%s\"\n", szModes[priv->adapter->InfrastructureMode]);
	p += sprintf(p, "State=\"%s\"\n",
			szStates[priv->adapter->MediaConnectStatus]);
	p += sprintf(p, "MACAddress=\"%02x:%02x:%02x:%02x:%02x:%02x\"\n",
			netdev->dev_addr[0], netdev->dev_addr[1],
			netdev->dev_addr[2], netdev->dev_addr[3],
			netdev->dev_addr[4], netdev->dev_addr[5]);

    	p += sprintf(p, "MCCount=\"%d\"\n", netdev->mc_count);
	p += sprintf(p, "ESSID=\"%s\"\n", (u8 *)priv->adapter->CurBssParams.ssid.Ssid);
	/* TODO: this works only if user calls wlan_get_freq() */
	p += sprintf(p, "Channel=\"%d\"\n", priv->adapter->Channel);
	p += sprintf(p, "region_code = \"%02x\"\n", (u32)priv->adapter->RegionCode);
	
	/*
	 * Put out the multicast list 
	 */
	for (i = 0; i < netdev->mc_count; i++) {
		p += sprintf(p,
				"MCAddr[%d]=\"%02x:%02x:%02x:%02x:%02x:%02x\"\n",
				i, 
				mcptr->dmi_addr[0], mcptr->dmi_addr[1],
				mcptr->dmi_addr[2], mcptr->dmi_addr[3], 
				mcptr->dmi_addr[4], mcptr->dmi_addr[5]);

		mcptr = mcptr->next;
	}
	p += sprintf(p, "num_tx_bytes_transmited = \"%lu\"\n",priv->stats.tx_bytes);
	p += sprintf(p, "num_rx_bytes_recieved = \"%lu\"\n",priv->stats.rx_bytes);
	p += sprintf(p, "num_tx_packets_transmited = \"%lu\"\n",priv->stats.tx_packets);
	p += sprintf(p, "num_rx_packets_received = \"%lu\"\n",priv->stats.rx_packets);
	p += sprintf(p, "num_tx_packets_dropped = \"%lu\"\n",priv->stats.tx_dropped);
	p += sprintf(p, "num_rx_packets_dropped = \"%lu\"\n",priv->stats.rx_dropped);
	p += sprintf(p, "num_tx_errors= \"%lu\"\n",priv->stats.tx_errors);
	p += sprintf(p, "num_rx_errors= \"%lu\"\n",priv->stats.rx_errors);


#ifdef	DEBUG
	p += sprintf(p, "SleepEvents=\"%d\"\n", priv->adapter->SleepCounter);
	p += sprintf(p, "ConfirmCounter=\"%d\"\n", priv->adapter->ConfirmCounter);
	p += sprintf(p, "ConsecutiveAwakes=\"%d\"\n",
			priv->adapter->NumConsecutiveAwakes);
#endif

#ifdef WPRM_DRV
#ifdef HOST_WAKEUP
        p += sprintf(p, "Adapter->bHostWakeupDevRequired = %d.\n", priv->adapter->bHostWakeupDevRequired);
#endif
#ifdef PS_REQUIRED
        p += sprintf(p, "Adapter->PSState = %d.\n", priv->adapter->PSState);
        p += sprintf(p, "Adapter->PSMode = %d.\n", priv->adapter->PSMode);
        p += sprintf(p, "Adapter->NeedToWakeup = %d.\n", priv->adapter->NeedToWakeup);
#endif
        p += sprintf(p, "WPrM Traffic Meter:\n");
        p += sprintf(p, "wprm_tm_ps_cmd_in_list = %d.\n", wprm_tm_ps_cmd_in_list);
        p += sprintf(p, "wprm_tm_ps_cmd_no = %d.\n", wprm_tm_ps_cmd_no);
        p += sprintf(p, "is_wprm_traffic_meter_timer_set =%d.\n", is_wprm_traffic_meter_timer_set);
#ifdef WMM_UAPSD
        p += sprintf(p, "UAPSD related:\n");
        p += sprintf(p, "Voice session Status = %d.\n", voicesession_status);
	p += sprintf(p, "Sleep Period  = %d.\n", priv->adapter->sleep_period.period);
        p += sprintf(p, "CurBssParams.wmm_uapsd_enabled =%d.\n", priv->adapter->CurBssParams.wmm_uapsd_enabled );
        p += sprintf(p, "priv->Adapter->wmm.qosinfo =%d.\n", priv->adapter->wmm.qosinfo);	
#endif
#endif

exit:
	return (p - page);
}

void wlan_proc_entry(wlan_private *priv, struct net_device *dev)
{

#ifdef	CONFIG_PROC_FS		// TODO
	PRINTK1("Creating Proc Interface\n");

	if (!priv->proc_entry) {
		priv->proc_entry = proc_mkdir("wlan", proc_net);

		if (priv->proc_entry) {
			priv->proc_dev = create_proc_read_entry
				("info", 0, priv->proc_entry, wlan_proc_read, 
				 dev);
		}
	}
#endif
}

void wlan_proc_remove(wlan_private *priv)
{
#ifdef CONFIG_PROC_FS
	if (priv->proc_entry) {
		remove_proc_entry("info", priv->proc_entry);
	}
	remove_proc_entry("wlan", proc_net);
#endif
}

int string_to_number(char *s)
{
  int r = 0;
  int base = 0;

  if ((strncmp(s, "0x", 2) == 0) || (strncmp(s, "0X", 2) == 0))
    base = 16;
  else
    base = 10;

  if (base == 16)
    s += 2;

  for (s = s; *s !=0; s++)
  {
    if ((*s >= 48) && (*s <= 57))
      r = (r * base) + (*s - 48);
    else if ((*s >= 65) && (*s <= 70))
      r = (r * base) + (*s - 55);
    else if ((*s >= 97) && (*s <= 102))
      r = (r * base) + (*s - 87);
    else
      break;
  }

	return r;
}