/*
 * Copyright ©  2004, 2006 Motorola
 *
 * This code is licensed under LGPL 
 * see the GNU Lesser General Public License for full LGPL notice.
 *
 * This library is free software; you can redistribute it and/or modify it 
 * under the terms of the GNU Lesser General Public License as published 
 * by the Free Software Foundation; either version 2.1 of the License, 
 * or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, 
 * but WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 * See the GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License 
 * along with this library; if not, write to the Free Software Foundation, Inc., 
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

/* 
 * History:
 * Author	Date		Description of Changes
 * ---------	-----------	--------------------------
 * Motorola	2004/12/14	Originally created
 * Motorola	2006/10/25	Update comments	
 */

#ifndef __PCAP_RTC_H__
#define __PCAP_RTC_H__

#include <linux/ioctl.h>

#define PCAP_RTC_NAME   "pcap_rtc"

/* pcap_rtc get time (seconds since 2000) */
#define PCAP_RTC_IOC_GET_TIME           _IOR('p', 1, struct timeval *)
/* pcap_rtc set time (seconds since 2000) */
#define PCAP_RTC_IOC_SET_TIME           _IOW('p', 1, struct timeval *)
/* pcap_rtc get alarm time (seconds since 2000) */
#define PCAP_RTC_IOC_GET_ALARM_TIME     _IOR('p', 2, struct timeval *)
/* pcap_rtc set alarm time (seconds since 2000) */
#define PCAP_RTC_IOC_SET_ALARM_TIME     _IOW('p', 2, struct timeval *)

#endif  /* __PCAP_RTC_H__ */
