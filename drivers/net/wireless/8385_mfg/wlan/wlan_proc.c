/*
 * 	File: If the proc interface defined than lets do this!! 
 */
#include        <linux/module.h>
#include        <linux/kernel.h>
#include	<linux/net.h>
#include	<linux/netdevice.h>
#include	<linux/wireless.h>
#include        <linux/proc_fs.h>
#include 	<linux/types.h>

#include        <net/iw_handler.h>

#include	"os_headers.h"
#include 	"wlan.h"

static struct proc_dir_entry *proc_wlan;

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


int wlan_proc_read(char *page, char **start, off_t offset,
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

exit:
	return (p - page);
}

void wlan_proc_entry(wlan_private *priv, struct net_device *dev)
{

#ifdef	CONFIG_PROC_FS		// TODO
	PRINTK1("Creating Proc Interface\n");

	if (!proc_wlan) {
		priv->proc_entry = proc_mkdir("wlan", proc_net);

		if (priv->proc_entry) {
			priv->proc_dev = create_proc_read_entry
				("info", 0, priv->proc_entry, wlan_proc_read, 
				 dev);
		}
	}
#endif
}

void wlan_proc_remove(char *name)
{
#ifdef CONFIG_PROC_FS
	proc_net_remove(name);
#endif
}
