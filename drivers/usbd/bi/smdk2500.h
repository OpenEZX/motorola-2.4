/*
 * linux/drivers/usbd/bi/smdk2500.h
 *
 * Copyright (c) 2002, 2003 Belcarra
 *
 * By: 
 *      Stuart Lynne <sl@belcarra.com>, 
 *      Chris Lynne <cl@belcarra.com>, 
 *      Bruce Balden <balden@belcarra.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */


/*
 * smdk2500 
 */

#define EP0_PACKETSIZE  	0x40

#define UDC_MAX_ENDPOINTS       5

#define UDC_NAME        	"SMDK2500"


#define WAIT_FOR_SETUP          0
#define DATA_STATE_XMIT         1
#define DATA_STATE_NEED_ZLP     2
#define WAIT_FOR_OUT_STATUS     3
#define DATA_STATE_RECV         4

#if defined(CONFIG_USBD_SMDK2500_USE_ETH0 ) && defined(CONFIG_USBD_SMDK2500_USE_ETH1 )
	#error "Only one of ETH0 or ETH1 can be supported"
#endif

