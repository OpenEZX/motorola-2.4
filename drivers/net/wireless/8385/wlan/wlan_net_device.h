/****************************************************************************** 
 * $Header: //depot/MRVL/main/driver/WLan/Drv/MSI/Linux/src/wlan/wlan_net_device.h#2 $
 *
 * ============================================================================
 * ============================================================================
 *
 *  Description:
 *      wlan network device structure and interface
 *
 *  The wlan drivers use a net_device structure to maintain context.
 *  This structure was defined in some linux header files, but since we're
 *  not on linux, we really don't need anything that complex, so we'll
 *  just create our own analogue structure here.
 *
 *  This structure will maintain the context for the drivers and will serve
 *  as the function pointer interface from the upper level protocols into
 *  the wlan component.
 *
 *****************************************************************************/

#ifndef _WLAN_NET_DEVICE_H
#define _WLAN_NET_DEVICE_H

#include "wlan.h"
//#include "ThreadX_macros.h"

//#define THREADX
//#define ETH_ALEN 6

#define	PRINTK(x...)	dprintf(x)
#define ETH_HLEN	14
#define ENOTSUPP	523


// interface to upper layer

/*void WirelessGetRxBuffer(char** rxBuff, int* buffLen, void** buffDesc);
void WirelessRxPacketRecvd(char* pktBuff, UINT32 pktLen, void* buffDesc);
void WirelessTxPktSent(char* pktBuff, UINT32 pktLen);
*/
void WlanShimDRecvPacket(UCHAR *Pkt, ULONG Len, void *buffdesc);
void WlanShimDGetRxBuf(char **rcv_buf, int *buf_len, void ** buf_desc);
void WlanShimDTxPktSent(char *pkt, UINT32 pkt_len);


#define	IFF_UP		0x1		/* interface is up		*/
#define	IFF_BROADCAST	0x2		/* broadcast address valid	*/
#define	IFF_DEBUG	0x4		/* turn on debugging		*/
#define	IFF_LOOPBACK	0x8		/* is a loopback net		*/
#define	IFF_POINTOPOINT	0x10		/* interface is has p-p link	*/
#define	IFF_NOTRAILERS	0x20		/* avoid use of trailers	*/
#define	IFF_RUNNING	0x40		/* resources allocated		*/
#define	IFF_NOARP	0x80		/* no ARP protocol		*/
#define	IFF_PROMISC	0x100		/* receive all packets		*/
#define	IFF_ALLMULTI	0x200		/* receive all multicast packets*/

#define IFF_MASTER	0x400		/* master of a load balancer 	*/
#define IFF_SLAVE	0x800		/* slave of a load balancer	*/

#define IFF_MULTICAST	0x1000		/* Supports multicast		*/

#define IFF_VOLATILE	(IFF_LOOPBACK|IFF_POINTOPOINT|IFF_BROADCAST|IFF_MASTER|IFF_SLAVE|IFF_RUNNING)

typedef struct iw_point
{
    int *pointer;		
    u16 length;
    u16 flags;
}iw_point;

/*
struct ifreq {
    u16 temp;
};

struct iw_request_info {
	u16 temp;	
};

struct pt_regs {
	u16 temp;
};
*/

struct SCAN_SSID_IE{
	UINT8 SSID[33];
	ULONG ssid_len;
	UINT8 BSSID[6];
	
	UINT16 Capability;
	UINT8	WPA_IE[32];
};

typedef struct sk_buff
{
    UCHAR* data;
    UINT32 len;
}sk_buff;

typedef struct net_device
{
    VOID* priv;     // pointer to private area
    UINT32 mc_count;
    UINT32 flags;

    BYTE dev_addr[ETH_ALEN];

//    int mc_count;	//required for set_multicast function
    UCHAR *list[6];

    // WLAN interface functions
/*  int (*wlan_open)(struct net_device *dev);
    int (*wlan_stop)(struct net_device *dev);
    struct net_device_stats* (*wlan_get_stats)(struct net_device *dev);
    int (*wlan_hard_start_xmit)(struct sk_buff *skb, struct net_device *dev);
*/

    int (*open)(struct net_device *dev);
    int (*stop)(struct net_device *dev);
    struct net_device_stats* (*get_stats)(struct net_device *dev);
    int (*hard_start_xmit)(struct sk_buff *skb, struct net_device *dev);
    int (*do_ioctl)(struct net_device *dev, struct ifreq *ifr, int cmd);
//TODO    int (*wlan_do_ioctl)(struct net_device *dev, struct ifreq *req, int i);

#ifdef	WIRELESS_EXT
//TODO    struct iw_statistics* (*wlan_get_wireless_stats)(struct net_device *dev);
//TODO    struct iw_handler_def wlan_handler_def;
#endif
	int (*set_mac_address)(struct net_device *dev, void *addr);
	void (*set_multicast_list)(struct net_device *dev);

    // BUGBUG: don't know if we need the functions:
    // wlan_tx_timeout
    // 
}net_device;

int wlan_init_module(void);
void wlan_cleanup_module(void);
void creat_del_evnt_flags(unsigned char);
struct net_device * wlan_get_net_dev(void);
void wlan_SetMulticast(struct net_device *, unsigned char, unsigned char *list[6]);
void wlan_GetMulticast(struct net_device *, unsigned char*, unsigned char *list[6]);

#ifdef MULTICAST_ENABLE
void wlan_set_multicast_list(struct net_device *dev);
#endif


struct net_device *WlanShimDLoad(void);
void WlanShimDUnload(void);
void WlanShimDSetMacAddr(struct net_device *dev, unsigned char Addr[6]);
void WlanShimDGetMacAddr(struct net_device *dev, unsigned char *Addr);
void WlanShimDSetMulticast(struct net_device *dev, unsigned char Num, unsigned char *List[6]);
void WlanShimDGetMulticast(struct net_device *dev, unsigned char *Num, unsigned char *List[6]);
int WlanShimDTxPacket(struct net_device *dev, struct sk_buff *skb);
void WlanShimDISR(struct net_device *dev);



#endif  // _WLAN_NET_DEVICE_H

