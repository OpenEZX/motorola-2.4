#	File	: Makefile
#
# (c) Copyright © 2003-2006, Marvell International Ltd. 
#
# (c) Copyright 2006, Motorola, Inc.
#
# This software file (the "File") is distributed by Marvell International 
# Ltd. under the terms of the GNU General Public License Version 2, June 1991 
# (the "License").  You may use, redistribute and/or modify this File in 
# accordance with the terms and conditions of the License, a copy of which 
# is available along with the File in the license.txt file or by writing to 
# the Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 
# 02111-1307 or on the worldwide web at http://www.gnu.org/licenses/gpl.txt.
#
# THE FILE IS DISTRIBUTED AS-IS, WITHOUT WARRANTY OF ANY KIND, AND THE 
# IMPLIED WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE 
# ARE EXPRESSLY DISCLAIMED.  The License provides additional details about 
# this warranty disclaimer.
#
#
# 2006-Jul-07 Motorola - Add changes for Motorola specific make environment.
#

EXTRA_CFLAGS    += $(SD83XX_CFLAGS)
EXTRA_CFLAGS    += -I../../wlan -I../../os/linux -I. 

ifeq ($(CONFIG_BULVERDE),y)
    EXTRA_CFLAGS += -I../../io/sdio/pxa270
endif

obj-m := if_sdio.o

include $(TOPDIR)/Rules.make
