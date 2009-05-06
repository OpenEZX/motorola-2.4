/*
 * include/linux/modem/connection_monitor.h
 *
 * Copyright ©  2005, 2006 Motorola
 *
 * This code is licensed under LGPL see the GNU Lesser General Public License for full LGPL notice.
 *
 * This library is free software; you can redistribute it and/or modify it under the terms of the
 * GNU Lesser General Public License as published by the Free Software Foundation;
 * either version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License along with this library;
 * if not, write to the Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA"
 *
 */
/*
 * Motorola	Feb 24,2005	Created
 * Motorola	Oct 25,2006	Update comments
 */

#ifndef _CONNECTION_MONITOR_H
#define _CONNECTION_MONITOR_H


typedef enum {
    CONNECTION_USB = 0,
    CONNECTION_BT,
    CONNECTION_UART,
    CONNECTION_IRDA
} CONNECTION_TYPE;

typedef enum {
    CONNECTION_BROKEN = 0,
    CONNECTION_OK
} CONNECTION_STATUS;

typedef struct {
    CONNECTION_STATUS usb;
    CONNECTION_STATUS bt;
    CONNECTION_STATUS uart;
    CONNECTION_STATUS irda;
} connection_status;

/* Called in Kernel space */
extern int modem_connection_status_inform_kernel(CONNECTION_TYPE connection, CONNECTION_STATUS status); 

/* Called in User space */
extern int modem_connection_status_inform_userspace(CONNECTION_TYPE connection, CONNECTION_STATUS status); 


#define MDMCONNMONITORSETOWNER    _IO('m', 163)   
#define MDMCONNMONITORCONNSTATUS    _IOW('m', 164, int)  
#define MDMCONNMONITORBTFILENAME    _IOR('m', 165, int)  
#endif /* _CONNECTION_MONITOR_H */

