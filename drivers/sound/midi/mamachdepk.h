/****************************************************************************
 *                                                                          *
 *      Copyright (C) 2001-2003 YAMAHA CORPORATION. All rights reserved.    *
 *                                                                          *
 *      Module      : mamachdepk.h                                          *
 *                                                                          *
 *      Description : Define machine dependent types on kernel for MA SMW   *
 *                                                                          *
 *      Version     : 1.3.15.8  2003.09.29                                  *
 *                                                                          *
 ****************************************************************************/

#ifndef __MAMACHDEPK_H__
#define __MAMACHDEPK_H__

#ifndef UINT8
typedef unsigned char   UINT8;
#endif
#ifndef UINT16
typedef unsigned short  UINT16;
#endif

#ifndef UINT32
typedef unsigned long   UINT32;
#endif

#ifndef SINT8
typedef signed char SINT8;
#endif
#ifndef SINT16
typedef signed short    SINT16;
#endif
#ifndef SINT32
typedef signed long SINT32;
#endif



#define MA3_WRB_STATUSREG(base,value)   writeb(value,(base))
#define MA3_WRB_DATAREG(base,value) writeb(value,(base)+2)
#define MA3_RDB_STATUSREG(base)     readb((base))
#define MA3_RDB_DATAREG(base)       readb((base)+2)

#endif /*__MAMACHDEPK_H__*/
