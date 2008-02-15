/*
 * Copyright 2004 Freescale Semiconductor, Inc.
 * Copyright (C) 2005-2006  Motorola, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as 
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 *
 * Motorola 2005-Dec-06 - File Creation.
 *
 */

/*!
 * @file backup_mem.c
 *
 * @ingroup poweric_core
 *
 * @brief The kernel-level interface to access backup memory registers
 *
 * The functions in this file implement the kernel-level interface for reading
 * and writing the backup memory registers.  The register access functions in 
 * external.c are accessed from this file in order to simplify reading/writing
 * to the actual registers.
 *
 * Currently, this API supports only the ATLAS backup memory registers.
 */

#include <linux/errno.h>
#include <linux/module.h>
#include <linux/power_ic.h>

#ifndef DOXYGEN_SHOULD_SKIP_THIS
EXPORT_SYMBOL(power_ic_backup_memory_read);
EXPORT_SYMBOL(power_ic_backup_memory_write);
#endif


/******************************************************************************
* Constants
******************************************************************************/

/* Backup Memory API location definitions per power ic. */
#if defined(CONFIG_MOT_POWER_IC_ATLAS)
#define  POWER_IC_BACKUP_MEMORY_A  POWER_IC_REG_ATLAS_MEMORY_A
#define  POWER_IC_BACKUP_MEMORY_B  POWER_IC_REG_ATLAS_MEMORY_B
#else
#define  POWER_IC_BACKUP_MEMORY_A  -1
#define  POWER_IC_BACKUP_MEMORY_B  -1
#endif


/******************************************************************************
* Local Structures
******************************************************************************/

/* Backup Memory API structure for lookup table. */
typedef struct
{
    POWER_IC_REGISTER_T  location; /* Memory register element is stored in. */
    unsigned short       offset; /* Element offset from LSB of register. */
    unsigned short       length; /* Length of element in bits. */
} POWER_IC_BACKUP_MEMORY_LOOKUP_T;


/******************************************************************************
* Local Variables
******************************************************************************/

/* Lookup table for Backup Memory API based on Backup Memory ID. */
const POWER_IC_BACKUP_MEMORY_LOOKUP_T backup_memory_table[POWER_IC_BACKUP_MEMORY_ID_NUM_ELEMENTS] =
{
    { /* POWER_IC_BACKUP_MEMORY_ID_MEMA */
        POWER_IC_BACKUP_MEMORY_A, /* memory register location */
        0x00, /* offset */
        0x10 /* length */
    },

    { /* POWER_IC_BACKUP_MEMORY_ID_MEMB */
        POWER_IC_BACKUP_MEMORY_B, /* memory register location */
        0x00, /* offset */
        0x10 /* length */
    },

    { /* POWER_IC_BACKUP_MEMORY_ID_ATLAS_BACKUP_FLASH_MODE */
        POWER_IC_BACKUP_MEMORY_A, /* memory register location */
        0x00, /* offset */
        0x01 /* length */
    },

    { /* POWER_IC_BACKUP_MEMORY_ID_ATLAS_BACKUP_PANIC */
        POWER_IC_BACKUP_MEMORY_A, /* memory register location */
        0x01, /* offset */
        0x01 /* length */
    },

    { /* POWER_IC_BACKUP_MEMORY_ID_ATLAS_BACKUP_FOTA_MODE */
        POWER_IC_BACKUP_MEMORY_A, /* memory register location */
        0x02, /* offset */
        0x01 /* length */
    },

    /* ------ Insert new elements above this line ------ */
};


/******************************************************************************
* Global Functions
******************************************************************************/

/*!
 * @brief Get the value of a bit in the backup memory registers.
 *
 * This function is used to read a contiguous range of bits in a Backup Memory register.
 * The read value is passed back through the value parameter.
 *
 * @param        id         identifier for what bit(s) to read
 * @param        value      value to store the read bit(s) to
 *
 * @return 0 if successful
 */
int power_ic_backup_memory_read(POWER_IC_BACKUP_MEMORY_ID_T id, int* value)
{
    /* First, check to make sure this is a valid backup memory element. */
    if ((id < POWER_IC_BACKUP_MEMORY_ID_NUM_ELEMENTS) && (backup_memory_table[id].location >= 0))
    {
        /* Get only this value from the register. */
        return power_ic_get_reg_value(backup_memory_table[id].location,
                                      backup_memory_table[id].offset,
                                      value, backup_memory_table[id].length);
    }
    /* Return invalid if ID is not recognized, or location is not valid. */
    else
    {
        return -EINVAL;
    }
}


/*!
 * @brief Set the value of a bit in the backup memory registers.
 *
 * This function is used to overwrite a contiguous range of bits in a Backup Memory register.
 * 
 *
 * @param        id         identifier for what bit(s) to write
 * @param        value      value to write to the register bit(s)
 *
 * @return 0 if successful
 */
int power_ic_backup_memory_write(POWER_IC_BACKUP_MEMORY_ID_T id, int value)
{
    /* First, check to make sure this is a valid backup memory element. */
    if ((id < POWER_IC_BACKUP_MEMORY_ID_NUM_ELEMENTS) && (backup_memory_table[id].location >= 0))
    {
        /* Write the new register value. */
        return power_ic_set_reg_value(backup_memory_table[id].location,
                                      backup_memory_table[id].offset,
                                      value, backup_memory_table[id].length);
    }
    /* Return invalid if ID is not recognized, or location is not valid. */
    else
    {
        return -EINVAL;
    }
}
