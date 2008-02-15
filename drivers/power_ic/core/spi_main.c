/*
 * /vobs/ezx_linux/code/linux/linux-2.4.17/drivers/power_ic/core/spi_main.c
 *
 * Description - This is the main file for SPI data flow management.
 *
 * Copyright (C) 2005-2006 - Motorola
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Motorola 02-Feb-2006 - Added power management interface.
 * Motorola 16-Jun-2005 - Initial Creation
 *
 */

/*!
 * @defgroup spi_management SPI data flow management
 */

/*!
 * @file spi_main.c
 *
 * @ingroup spi_management
 *
 * @brief This is the main file for SPI data flow management.
 *
 * <B>Overview:</B><BR>
 *   Provides the external interface by which data can be sent out one of the
 *   SPI buses.  This module implements the queues which are used for transferring
 *   data and is hardware independent.  It requires the external interfaces defined
 *   in below under "Low Level Driver Requirements"  to be provided externally.
 *
 *   The system works by first placing the requested data into the transfer (tx) queue.
 *   A request to transfer is then made of the low level driver.  It will start the
 *   transmission if a previous transmission is not already running.  Once it detects
 *   the completion of a transmission it will call transfer complete function
 *   (spi_tx_completed()) within this module.  This will move the data from the
 *   transmit queue to the received queue (rx) and handle waking up the transceive
 *   routine if it was blocking.
 *
 *   If the user does not wish to wait for the transfer to complete, they must set
 *   the nonblocking flag in the parameter structure.  In this case, the user must poll
 *   for the transfer to complete using spi_check_complete() or the received queue
 *   will fill up.
 *
 *   In all cases the user provided buffer will get the received data copied into it.
 *   This will be the case even if the user does not wish to receive the data.  In this
 *   case, the user still must block on the completion or poll for the completion with
 *   spi_check_complete().  The receive and transmit buffers must be DMA accessible for
 *   systems which support DMA at the low level.  See the low lever driver for details.
 *
 * <B>Low Level Driver Requirements:</B><BR>
 *   The low level driver module must be able to send the current message and then
 *   call spi_tx_completed() when the transmit was completed.  Then based upon the
 *   return value the next transmission will be started or the transmit shut down.
 *   A new transmit must be started if the return value is non NULL.
 *
 *   A pointer is provided in the queue for use by the low level driver if necessary.
 *   See low_level_data_p in SPI_QUEUE_NODE_T for more details.
 */

#include <linux/config.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/limits.h>
#include <linux/pm.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/spi_user.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <stdbool.h>
#include "os_independent.h"
#include "spi_main.h"
#include "pxa_ssp.h"

/******************************************************************************
* Local constants and macros
******************************************************************************/

/*!
 * @brief The number of queue entries which should be defined when the system
 * initializes.
 *
 * This should be as close to the number of nodes which will be used at run time.
 * The list will resize if more are needed, but this will take execution time.
 */
#define SPI_NUM_INIT_QUEUE_ENTRIES 10

/*!
 * @brief The maximum number of queue entries allowed in the transaction queue.
 *
 * Once this is reached the queue will not be expanded in size.  This should never
 * happen during normal running.  It is setup to find potential problems during
 * development.
 */
#define SPI_MAX_NUM_QUEUE_ENTRIES (SPI_NUM_INIT_QUEUE_ENTRIES*10)

/*!
 * @brief Value used for the index of unused nodes in the queue or for the head if no
 * entries are in the queue.
 */
#define SPI_QUEUE_NODE_UNUSED   ((SPI_QUEUE_INDEX_T)(-1))

/*!
 * @brief Used to reserve space in the queue for a new node which has not yet been placed
 * into one of the queues.
 */
#define SPI_QUEUE_NODE_RESERVED (SPI_QUEUE_NODE_UNUSED-1)

/*!
 * @brief Macro to return the SPI port on which a specific chip select is assigned to.
 */
#define spi_cs_to_port(cs)  ((cs) < SPI_CS__END) ? cs_to_port[(cs)]: SPI_PORT__END;

/*!
 * @brief Macros used for debugging of the code within this file (spi_main.c).
 */
#if (defined POWER_IC_SPI_MAIN_DEBUG_LEVEL) && (POWER_IC_SPI_MAIN_DEBUG_LEVEL > 1)
# define DEBUG_MSG_LEVEL2 tracemsg
#else
# define DEBUG_MSG_LEVEL2(fmt, args...)
#endif
#if (defined POWER_IC_SPI_MAIN_DEBUG_LEVEL)
# define DEBUG_MSG_LEVEL1 tracemsg
#else
# define DEBUG_MSG_LEVEL1(fmt, args...)
#endif

/******************************************************************************
* Local variables
******************************************************************************/
/*!
 * @brief Holds the function pointers for the low level driver.
 *
 * Each low level driver is required to provide the interface functions in
 * this structure.  A pointer to the functions will be placed in #low_level_drv_tb
 * for use by this driver.
 *
 * @note It is possible to have different low level drivers for each queue.
 */
static const struct
{
    /*!
     * Called to initialize the low level driver.
     */
    void (*spi_initialize)(void);
    /*!
     * Starts a transmit if none is currently active and returns.  It must not
     * not block to wait for the transmit to complete.  It may delay for a short
     * period of time while waiting for short transmits to complete.
     */
    int (*spi_start_tx)(SPI_QUEUE_NODE_T *node_p);
    /*!
     * Called to determine if a transmit is currently in progress.  If one is not
     * and a new node has been added to the queue spi_start_tx will be called to
     * initiate a transmit.  It must return true when a transmit is in progress and
     * false the low level driver is idle.
     */
    bool (*spi_tx_in_progress)(void);
} low_level_drv_tb[SPI_PORT__END] =
{
    /*! Define the functions for use with the main SPI port (SSP on Bulverde). */
    {
        pxa_ssp_initialize,
        pxa_ssp_start_tx,
        pxa_ssp_tx_in_progress
    }
};

/*!
 * @brief Table used to translate between chip select and port.
 *
 * Currently only one port is available for use on Bulverde.
 */
static const SPI_PORT_T cs_to_port[SPI_CS__END] =
{
    SPI_PORT_MAIN,  /* SPI_CS_PCAP   */
    SPI_PORT_MAIN   /* SPI_CS_TFLASH */
};

/*!
 * @brief The SPI transfer queue contains new items which have not yet been
 * sent out the SPI bus.
 *
 * They remain in the queue until the transfer has been completed.  Once
 * they have been transfered they are placed in the received queue.
 */
static SPI_QUEUE_T spi_tx_queue[SPI_PORT__END];

/*!
 * @brief The SPI receive queue contains the data which was received during a transmit.
 *
 * A node is only moved from the received queue after the data received has been placed
 * in the data portion of the node, and the transfer has been completed.
 */
static SPI_QUEUE_T spi_rx_queue[SPI_PORT__END];

#ifndef DOXYGEN_SHOULD_SKIP_THIS 
/*!
 * @brief Wait queue used to wait for a transfer to complete.
 *
 * Only one wait queue is used for for all transmissions, so it is necessary to check
 * to be sure the the transmit which was requested is the one which completed.
 */
DECLARE_WAIT_QUEUE_HEAD(rx_complete_wait_queue);
#endif

/*!
 * @brief Memory array which holds the queue entries.
 *
 * Elements in this array can be assigned to the Tx or Rx queues.
 */
static SPI_QUEUE_NODE_T *queue_data_p = NULL;

/*!
 * @brief The number of entries in the queue_data_p array.
 *
 * This is tracked to simplify growing the queue.
 */
static SPI_QUEUE_INDEX_T num_queue_entries;

/******************************************************************************
* Local functions
******************************************************************************/

/*!
 * @brief Inserts an existing node into a SPI transaction queue.
 *
 * Places the node in the queue based on the prioity of the device to which
 * the data will be sent.  This is done based on the entry port in the params_p
 * entry in the node.  The smaller the port the higher priority the data is
 * considered.
 *
 * @param     queue_p  Pointer to either the transmit or receive queue.
 * @param     node_i   Index of an existing node to insert onto the queue.
 *
 * @return    0 upon success<BR>
 *            -EINVAL if a bad arguement is used.<BR>
 */
static int spi_queue_node_insert(SPI_QUEUE_T *queue_p, SPI_QUEUE_INDEX_T node_i)
{
    unsigned long irq_flags;
    SPI_QUEUE_INDEX_T i;

    DEBUG_MSG_LEVEL2(_k_d("spi_queue_node_insert node = %d"), node_i);
    if ((queue_p == NULL) || (node_i >= num_queue_entries))
    {
        return (-EINVAL);
    }
    
    local_irq_save(irq_flags);
    i = queue_p->head_i;
    if (i == SPI_QUEUE_NODE_UNUSED)
    {
        queue_p->head_i = node_i;
        queue_data_p[node_i].next_i = node_i;
        queue_data_p[node_i].prev_i = node_i;
    }
    else
    {
        /*
         * Find the correct location to insert the node, based on priority.
         */
        do
        {
            if (queue_data_p[node_i].params.cs < queue_data_p[i].params.cs)
            {
                break;
            }
            i = queue_data_p[i].next_i;
        }
        while (i != queue_p->head_i);
        /*
         * Add the node to the list in the correct location.
         */
        i = queue_data_p[i].prev_i;
        queue_data_p[node_i].prev_i = i;
        queue_data_p[node_i].next_i = queue_data_p[i].next_i;
        queue_data_p[queue_data_p[i].next_i].prev_i = node_i;
        queue_data_p[i].next_i = node_i;
        /*
         * If inserting in place of the head node, reset the head to the
         * node with the highest priority.
         */
        if ((i == queue_p->head_i) && (queue_data_p[node_i].params.cs < queue_data_p[i].params.cs))
        {
            queue_p->head_i = node_i;
        }
    }
    local_irq_restore(irq_flags);
    return 0;
}

/*!
 * @brief Creates a new node for a SPI transaction queue.
 *
 * Creates a new node by searching the array of pre allocated nodes and
 * finding a node which is currently not used.  If no nodes are available the
 * array is resized and a node is assigned from the new array elements. The
 * function is safe to be called from an interrupt or bottom half context.
 *
 * @param     spi_index_p Pointer to the index to the spi node which will be created.  
 * @param     params_p    Contains the SPI bus specific parameters for the transaction. These
 *                        include items such as the chip select and the transfer speed.
 * @param     vectors     Array of blocks to be transmitted and received.
 * @param     count       The number of entries in the vectors array.
 * 
 * @return    This function returns 0 if successful.<BR>
 *            -EINVAL - Bad data contained in params_p.<BR>
 *            -ENOMEM - Unable to allocate memory for the new node.<BR>
 */
static int spi_queue_node_create
(
   SPI_QUEUE_INDEX_T *spi_index_p,
   SPI_DATA_PARAMETER_T *params_p,
   SPI_IOVEC_T *vectors,
   size_t count
)
{
    unsigned long irq_flags;
    static int sequence = 0;
    SPI_PORT_T port;
    SPI_QUEUE_INDEX_T i;
    SPI_QUEUE_NODE_T *new_queue_p;
    
    *spi_index_p = SPI_QUEUE_NODE_UNUSED;

    port = spi_cs_to_port(params_p->cs);
    /*
     * Range check the necessary elements and flag an error if out of range.
     * (The port check will check the chip select as well, since spi_cs_to_port
     * will use SPI_PORT__END if invalid cs numbers.)
     */
    if ((port >= SPI_PORT__END) || (vectors == NULL) || (count > SPI_IOVEC_MAX))
    {
        return -EINVAL;
    }

    /*
     * Find an available location for the new data.
     */
    local_irq_save(irq_flags);
    for (i = 0; i < num_queue_entries; i++)
    {
        if (queue_data_p[i].next_i == SPI_QUEUE_NODE_UNUSED)
        {
            queue_data_p[i].next_i = SPI_QUEUE_NODE_RESERVED;
            break;
        }
    }
    local_irq_restore(irq_flags);
    if (i >= num_queue_entries)
    {
        /*
         * No free element was found.  Allocate a larger buffer from which to select
         * nodes.
         */
        if (num_queue_entries >= SPI_MAX_NUM_QUEUE_ENTRIES)
        {
            tracemsg(_k_w("spi_queue_node_create: max number of queue entries reached."));
            return -ENOMEM;
        }
        /* At this point i is set to num_queue_entries from the loop above. */
        num_queue_entries += SPI_NUM_INIT_QUEUE_ENTRIES;
        /* Use GFP_ATOMIC in case this is called from an interrupt context. */
        new_queue_p = (SPI_QUEUE_NODE_T *)kmalloc(sizeof(SPI_QUEUE_NODE_T)*num_queue_entries, GFP_ATOMIC);
        if (new_queue_p == NULL)
        {
            tracemsg(_k_w("spi_queue_node_create: out of memory."));
            return -ENOMEM;
        }
        /* Copy the old queue into the new queue. */
        memcpy(new_queue_p, queue_data_p, sizeof(SPI_QUEUE_NODE_T)*i);
        kfree(queue_data_p);
        queue_data_p = new_queue_p;
        DEBUG_MSG_LEVEL2(_k_d("spi_queue_node_create: expanded queue size from %d entries to %d entries."), i, num_queue_entries);
    }
    /* Copy the existing data into the new node. */
    memcpy(&queue_data_p[i].params, params_p, sizeof(queue_data_p[i].params));
    queue_data_p[i].tx_complete = false;
    queue_data_p[i].port = port;
    queue_data_p[i].low_level_data_p = NULL;
    /* The vectors are copied from the user supplied data in case they were on the stack.  In this
       case they may not be valid by the time they need to be used. */
    memcpy(queue_data_p[i].vectors, vectors, sizeof(SPI_IOVEC_T)*count);
    queue_data_p[i].count = count;
    queue_data_p[i].error = 0;
    
    /*
     * Assign the next available sequence number to the node.  No check is made for
     * existing sequence numbers in the queue, since the transfer will in general happen
     * in order and none will ever be queue enough to end up with a duplicate.
     */
    local_irq_save(irq_flags);
    queue_data_p[i].sequence = sequence++;
    if (sequence < 0)
    {
        sequence = 0;
    }
    local_irq_restore(irq_flags);
    *spi_index_p = i;
    return 0;
}

/*!
 * @brief Removes the node passed in from the queue.
 *
 * Removes the node passed in from the queue passed in.  The node is not free to
 * be reused until spi_queue_node_destroy() is called.
 *
 * @param  queue_p Pointer to either the transmit or receive queue
 * @param  node_i  Index to the node to remove.
 *
 * @return 0 upon success<BR>
 *         -EINVAL - Null pointer in queue_p or an invalid index in node_i.<BR>
 */
static int spi_queue_node_remove(SPI_QUEUE_T *queue_p, SPI_QUEUE_INDEX_T node_i)
{
    unsigned long irq_flags;
    SPI_QUEUE_INDEX_T next_i;
    SPI_QUEUE_INDEX_T i;

    DEBUG_MSG_LEVEL2(_k_d("spi_queue_node_remove node = %d"), node_i);
    if ((queue_p == NULL) || (node_i >= num_queue_entries) || (queue_p->head_i == SPI_QUEUE_NODE_UNUSED))
    {
        return -EINVAL;
    }
    local_irq_save(irq_flags);
    
    /*
     * Make sure the node which is being removed is in the queue which was
     * passed in.
     */
    i = 0;
    next_i = queue_p->head_i;
    while (next_i != node_i)
    {
        next_i = queue_data_p[next_i].next_i;
        i++;
        if ((next_i == queue_p->head_i) || (i >= num_queue_entries))
        {
            local_irq_restore(irq_flags);
            return -EINVAL;
        }
    }
       
    if (node_i == queue_p->head_i)
    {
        /* Check and see if this is the only item in the list. */
        if (node_i == queue_data_p[node_i].next_i)
        {
            queue_p->head_i = SPI_QUEUE_NODE_UNUSED;
        }
        else
        {
            next_i = queue_data_p[node_i].next_i;
            queue_p->head_i = next_i;
            queue_data_p[next_i].prev_i = queue_data_p[node_i].prev_i;
            queue_data_p[queue_data_p[node_i].prev_i].next_i = next_i;
        }
    }
    else
    {
        next_i = queue_data_p[node_i].next_i;
        queue_data_p[next_i].prev_i = queue_data_p[node_i].prev_i;
        queue_data_p[queue_data_p[node_i].prev_i].next_i = next_i;
    }
    local_irq_restore(irq_flags);
    return 0;
}

/*!
 * @brief Destroys the SPI transaction node passed in.
 *
 * @param   node_i Index of the node which will be released for reuse.
 */
static void spi_queue_node_destroy(SPI_QUEUE_INDEX_T node_i)
{
    unsigned long irq_flags;

    DEBUG_MSG_LEVEL2(_k_d("spi_queue_node_destroy node = %d"), node_i);
    local_irq_save(irq_flags);
    queue_data_p[node_i].next_i = SPI_QUEUE_NODE_UNUSED;
    local_irq_restore(irq_flags);
}

/*!
 * @brief Find a transmit queue node with the sequence number passed in.
 *
 * @param  queue_p  Pointer to either the transmit or receive queue
 * @param  sequence The sequence number to locate in the queue.
 *
 * @return An index to the node with the sequence number or
 *         SPI_QUEUE_NODE_UNUSED if it is not found.
 */
static SPI_QUEUE_INDEX_T spi_queue_find_sequence_node(SPI_QUEUE_T *queue_p, int sequence)
{
    unsigned long irq_flags;
    SPI_QUEUE_INDEX_T i;
    SPI_QUEUE_INDEX_T end_i;

    DEBUG_MSG_LEVEL2(_k_d("spi_queue_find_sequence_node sequence = %d"), sequence);
    if ((queue_p == NULL) || (queue_p->head_i == SPI_QUEUE_NODE_UNUSED))
    {
        return SPI_QUEUE_NODE_UNUSED;
    }

    /*
     * Need to protect the search from a new node being entered on the list
     * during the search.
     */
    local_irq_save(irq_flags);
    
    /*
     * Search the list from the head, since this is called out to spi_tx_complete
     * which may be running in an interrupt.  In this case It will be looking for
     * the last node sent which will be toward the head of the list.
     */
    i = queue_p->head_i;
    end_i = i;
    do
    {
        if (queue_data_p[i].sequence == sequence)
        {
            local_irq_restore(irq_flags);
            return i;
        }
        i = queue_data_p[i].next_i;
    } while (i != end_i);
    local_irq_restore(irq_flags);
    return SPI_QUEUE_NODE_UNUSED;
}

/*!
 * @brief Finds the node which needs to be transfered next.
 *
 * When the chip select is locked the oldest node with the locked chip
 * select is returned.  If the chip select is not locked then the head of
 * the queue is returned.
 *
 * @param    port The qspi port for which the search must be done.
 *
 * @return   A pointer to the node to transmit or NULL if none.
 *
 * @note No range checking is done on port. It is assumed this will be done
 *       by the caller.
 */
static SPI_QUEUE_NODE_T *spi_queue_find_next_tx(SPI_PORT_T port)
{
    unsigned long irq_flags;
    SPI_QUEUE_INDEX_T i;
    SPI_QUEUE_INDEX_T end_i;
    SPI_CS_T locked_cs;

    DEBUG_MSG_LEVEL2(_k_d("spi_queue_find_next_tx"));
    /* Do nothing if the queue is empty. */
    if (spi_tx_queue[port].head_i != SPI_QUEUE_NODE_UNUSED)
    {
        /*
         * If no lock is set up, then simply return the head of the queue.
         */
        locked_cs = spi_tx_queue[port].locked_cs;
        if (locked_cs == SPI_CS__END)
        {
            DEBUG_MSG_LEVEL2(_k_d("spi_queue_find_next_tx - Returning head of list %d (lock - %d)."),
                             spi_tx_queue[port].head_i, locked_cs);
            return &queue_data_p[spi_tx_queue[port].head_i];
        }
        else
        {
            DEBUG_MSG_LEVEL2(_k_d("spi_queue_find_next_tx - Locked searching for the next node with the lock CS (%d)."),
                             locked_cs);
            /*
             * Find the oldest node with the chip select which is locked.
             */
            local_irq_save(irq_flags);
            i = spi_tx_queue[port].head_i;
            end_i = i;
            do
            {
                if (queue_data_p[i].params.cs == locked_cs)
                {
                    local_irq_restore(irq_flags);
                    return &queue_data_p[i];
                }
                i = queue_data_p[i].next_i;
            }
            while (i != end_i);
            local_irq_restore(irq_flags);
        }
    }
    return NULL;
}

/*
 * @brief Sets up the lock status based on the node about to be transmitted
 *
 * Sets the queues lock chip select based on the node which is passed in.  Once
 * a lock is set up only nodes for the locked chip select can be sent.  The selection
 * of these nodes is done in spi_queue_find_next_tx().
 *
 * @param node_p Pointer to the node which will be transmitted next.
 *
 * @return Pointer to the node which will be transmitted next (node_p).
 */
static SPI_QUEUE_NODE_T *spi_setup_next_tx(SPI_QUEUE_NODE_T *node_p)
{
    SPI_PORT_T port;
    
    DEBUG_MSG_LEVEL2(_k_d("spi_setup_next_tx"));
    if (node_p != NULL)
    {
        DEBUG_MSG_LEVEL2(_k_d("spi_setup_next_tx: Sequence %d"), node_p->sequence);
        port = node_p->port;
        /* Set the chip select lock if the node is part of a chain of nodes. */
        if (node_p->params.lock_cs)
        {
            spi_tx_queue[port].locked_cs = node_p->params.cs;
        }
        else
        {
            spi_tx_queue[port].locked_cs = SPI_CS__END;
        }
    }
    return node_p;
}


/*!
 * @brief Indicate to the power management system if sleep can be entered
 *
 * This function is registered with the power management system and will be
 * called before the power management system tries to put the phone to sleep.
 * It must return 0 if sleep is acceptable or non 0 if sleep cannot be entered.
 * It will also be called when sleep is exited, in case anything needs to be done
 * to get things going after sleeping.  Nothing is done in this case, since
 * the phone will only be allowed to go to sleep when nothing is in the queue.
 *
 * @param pm_dev  Pointer to the power management device
 * @param req     The request to wakeup (PM_RESUME) or sleep (PM_SUSPEND)
 * @param data    unsigned long with the next power management state
 *
 * @return true if the phone cannot go to sleep false if it can.
 */
#ifdef CONFIG_PM
static int spi_pm_callback
(
    struct pm_dev *pm_dev,
    pm_request_t req,
    void *data
)
{
    unsigned int i;

    if (req == PM_SUSPEND)
    {
        /* Do not allow sleep if a queue is not empty or a transfer is in progress. */
        for (i = 0; i < SPI_PORT__END; i++)
        {
            if (spi_tx_queue[i].head_i != SPI_QUEUE_NODE_UNUSED)
            {
                return true;
            }
        }
    }
    return false;
}
#endif

/******************************************************************************
* Global functions
******************************************************************************/
/*!
 * @brief Transfers the tail of the transmit queue to the head of the receive queue.
 *
 * This function must be called by the low level driver when a transfer of a node
 * is completed.  It will transfer the head node in the transfer queue to the
 * received queue.  It then wakes up the process which is waiting for the data
 * to be received.  If the node was a null node (used for unlocking a chip select),
 * the node is deleted from the transmit queue, since no result will be expected.
 *
 * @param     node_p - Pointer to the transmit node for which the data was just
 *            sent.
 *
 * @return    A pointer to the next node to transmit or NULL if no more data
 *            is in the transmit queue.
 */
SPI_QUEUE_NODE_T *spi_tx_completed(SPI_QUEUE_NODE_T *node_p)
{
    SPI_QUEUE_INDEX_T i;
    SPI_PORT_T port;

    DEBUG_MSG_LEVEL2(_k_d("spi_tx_completed: sequence %d node_p %p"), node_p->sequence, node_p);
    port = node_p->port;
    /*
     * Move the node from the transfer to the received queue.
     */
    i = spi_queue_find_sequence_node(&spi_tx_queue[port], node_p->sequence);
    if (i != SPI_QUEUE_NODE_UNUSED)
    {
        /* Remove the node from the transmit queue. */
        (void)spi_queue_node_remove(&spi_tx_queue[port], i);
        
        /* Indicate the transmit has been completed. */
        queue_data_p[i].tx_complete = true;

        /*
         * If this was an empty node, it was used to unlock the chip select.  As a result
         * no data needs to be returned.  Simply destroy the node to free the memory.
         */
        if (queue_data_p[i].count == 0)
        {
            spi_queue_node_destroy(i); 
        }
        else
        {
            spi_queue_node_insert(&spi_rx_queue[port], i);

            /* Call the transfer complete function, if one was supplied. */
            if (node_p->params.xfer_complete_p)
            {
                (*node_p->params.xfer_complete_p)(node_p->sequence, node_p->params.xfer_complete_param_p);
            }
            /* If the blocking until the data is transfered, wake up the user thread. */
            if (!node_p->params.nonblocking)
            {
                /* Wake up the process which requested the data be sent. */
                wake_up(&rx_complete_wait_queue);
            }
        }
        return spi_setup_next_tx(spi_queue_find_next_tx(port));
    }
    return NULL;
}

/*!
 * @brief Send and receive multiple blocks (vectors) of data over the SPI
 *
 * Sends and receives data over the SPI.  The data passed in is placed in a queue and
 * will be transmitted as soon as possible.  The function may block until all of
 * the data requested has been received.  It will block if the nonblocking parameter in
 * params_p is false.  If it is true the function will not block and the data received
 * will need to be retrieved using spi_check_complete().
 *
 * IMPORTANT:
 *   -# None of the data pointers can be NULL.  The low level drivers all assume that the
 *      same amount of data which will be transmitted will be received.  As a result they
 *      need to have a location to place the receive data.  On the same note, they must
 *      transmit something to receive something, so the transmit pointers must be valid as well.
 *   -# If the nonblocking flag is set within params_p then the data pointed to by params_p must
 *      remain valid until spi_check_complete() indicates the transmission is complete.
 *
 * @param     params_p  Contains the SPI bus specific parameters for the transaction. These
 *                      include items such as the chip select and the transfer speed.
 * @param     vectors   Pointer to an array of vectors (blocks) of data to send over the
 *                      spi.  The tx and rx pointers cannot be NULL, they can point to the
 *                      same memory location or different locations.  In both cases they
 *                      must be DMA accessible.
 * @param     count     The number of vectors (blocks) in the vectors array.
 *
 * @return    A sequence number upon success (will be 0 or greater).  The sequence number
 *            is only valid if a no wait transmit was requested.<BR>
 *            -EINVAL  - Null pointer passed in or bad data contained in params_p.<BR>
 *            -ENOMEM  - Unable to allocate memory for the new node.<BR>
 */
int spi_transceivev(SPI_DATA_PARAMETER_T *params_p, SPI_IOVEC_T *vectors, size_t count)
{
    SPI_QUEUE_INDEX_T node_i;
    SPI_QUEUE_NODE_T *node_p;
    SPI_PORT_T port;
    int sequence;
    size_t i;
    int error;

    DEBUG_MSG_LEVEL1(_k_d("spi_transceivev"));
    if ((params_p == NULL) || (vectors == NULL))
    {
        tracemsg(_k_w("spi_transceivev: Parameter pointer or vectors are null."));
        return -EINVAL;
    }

    /* Return an error if any of the user specified pointers are null. */
    for (i = 0; i < count; i++)
    {
        if ((vectors[i].rx_base == NULL) || (vectors[i].tx_base == NULL))
        {
            tracemsg(_k_w("spi_transceivev: Vector base address for tx or rx null at vector %d."), i);
            return -EINVAL;
        }
    }
    /* Get the port number for the transaction and do not continue if it is invalid. */
    port = spi_cs_to_port(params_p->cs);
    if (port >= SPI_PORT__END)
    {
        tracemsg(_k_w("spi_transceivev: Invalid port number %d."), port);
        return -EINVAL;
    }

    /* Place the data to be sent and received into the queue. */
    error = spi_queue_node_create(&node_i, params_p, vectors, count);
    if (error != 0)
    {
        tracemsg(_k_w("spi_transceivev: Unable to create queue node."));
        return error;
    }
    
    error = spi_queue_node_insert(&spi_tx_queue[port], node_i);
    if (error != 0)
    {
        tracemsg(_k_w("spi_transceivev: Unable to insert queue node."));
        return error;
    }

    /*
     * Store the sequence number, in case the node is destroyed by the callback before
     * the start tx function returns.  This can only happen when the nonblock parameter
     * is true.
     */
    sequence = queue_data_p[node_i].sequence;
    
    /*
     * Start the transmit if nothing is currently being transmitted by the low level driver. 
     * No need to validate the range of port since it was done in spi_queue_node_create.
     * The transmit will be started when the last higher priority one is completed or
     * when spi_restore_transfer_priority is called.
     */
    if (!(low_level_drv_tb[port].spi_tx_in_progress()))
    {
        node_p = spi_setup_next_tx(spi_queue_find_next_tx(port));
        error = 1;  /* In case no node needs to be sent due to a lock, force blocking. */
        if (node_p != NULL)
        {
            error = low_level_drv_tb[port].spi_start_tx(node_p);
        }
    }
    /*
     * If the user requested non-blocking, return the sequence number, even if
     * the transfer is already completed or there was an error.  This is done
     * to keep the users of this function from having to check for the case of
     * the transfer already completed.  This way they can always use spi_check_complete().
     */
    if (params_p->nonblocking)
    {
        DEBUG_MSG_LEVEL1(_k_d("spi_transceivev: Returning sequence number: %d"), sequence);
        return sequence;
    }

    /*
     * Wait for the transfer to complete.  This does not use interruptable, since SPI
     * transfers will complete quickly and if interrupted orphan data will be left in
     * the received queue if the user does not call spi_check_complete().
     */
    wait_event(rx_complete_wait_queue, (queue_data_p[node_i].tx_complete != false));
    error = queue_data_p[node_i].error;
    DEBUG_MSG_LEVEL1(_k_d("spi_transceivev: Done waiting error is %d"), error);
    
    /* Remove the node from the receive queue. */
    (void)spi_queue_node_remove(&spi_rx_queue[port], node_i);
    spi_queue_node_destroy(node_i);
    return error;
}

/*!
 * @brief Send and receive data over the SPI
 *
 * Sends and receives data over the SPI.  The data passed in is placed in a queue and
 * will be transmitted as soon as possible.  The function may block until all of
 * the data requested has been received.  It will block if the nonblocking parameter in
 * params_p is false.  It is is true the function will not block and the data received
 * will need to be retrieved using spi_fetch_recieve.
 *
 * IMPORTANT:
 *   None of the data pointers can be NULL.  The low level drivers all assume that the
 *   same amount of data which will be transmitted will be received.  As a result they
 *   need to have a location to place the receive data.  On the same note, they must
 *   transmit something to receive something, so the transmit pointers must be valid as well.
 *
 * @param     params_p Contains the SPI bus specific parameters for the transaction. These
 *                     include items such as the chip select and the transfer speed.
 * @param     length   The number of bytes which need to be transfered.
 * @param     tx_p     Pointer to the data to be transmitted (must be DMA accessible).  This
 *                     pointer cannot be NULL or an error will be returned.
 * @param     rx_p     Pointer to the data to be received (must be DMA accessible).  This
 *                     pointer cannot be NULL or an error will be returned.
 *
 * @return    A sequence number upon success (will be 0 or greater).  The sequence number
 *            is only valid if a no wait transmit was requested. <BR>
 *            -ENODATA - Internal error.<BR>
 *            -EINVAL  - Null pointer passed in or bad data contained in params_p.<BR>
 *            -ENOMEM  - Unable to allocate memory for the new node.<BR>
 */
int spi_transceive(SPI_DATA_PARAMETER_T *params_p, size_t length, void *tx_p, void *rx_p)
{
    SPI_IOVEC_T spi_trans_vectors;

    spi_trans_vectors.tx_base = tx_p;
    spi_trans_vectors.rx_base = rx_p;
    spi_trans_vectors.iov_len = length;
    return spi_transceivev(params_p, &spi_trans_vectors, 1);
}

/*!
 * @brief Check to see if the transmit and receive are completed.
 *
 * Searches the received queue for the sequence number which is provided.  If
 * it is found the user is informed and the node is destroyed.  The user must
 * call this if they have set up the nonblocking parameter of the receive queue will
 * fill up.  No data is copied, since the user specified a receive pointer to
 * which the data will already be copied.
 *
 * @param   params_p  Contains the SPI bus specific parameters for the transaction. These
 *                    include items such as the chip select and the transfer speed.
 * @param   sequence  The sequence number from the receive or transceive for
 *                    which the data must be received.
 *
 * @return  0 upon successfully finding the node and copying data to data_p.<BR>
 *          1 if the transcation is still in progress (node is in the transmit queue).<BR>
 *          -ENODATA - Internal error.<BR>
 *          -EINVAL  - Bad data contained in params_p or the node is not in either
 *                     queue.<BR>
 */
int spi_check_complete(SPI_DATA_PARAMETER_T *params_p, int sequence)
{
    SPI_QUEUE_INDEX_T node_i;
    SPI_PORT_T port;

    DEBUG_MSG_LEVEL1(_k_d("spi_check_complete = %d"), sequence);
    if (params_p == NULL)
    {
        return -EINVAL;
    }
    
    /* Get the port number for the transaction and do not continue if it is invalid. */
    port = spi_cs_to_port(params_p->cs);
    if (port >= SPI_PORT__END)
    {
        return -EINVAL;
    }
    
    node_i = spi_queue_find_sequence_node(&spi_rx_queue[port], sequence);
    if (node_i == SPI_QUEUE_NODE_UNUSED)
    {
        /* Return an error if the node is not in the transmit queue. */
        if (spi_queue_find_sequence_node(&spi_tx_queue[port], sequence) == SPI_QUEUE_NODE_UNUSED)
        {
            return -EINVAL;
        }
        return 1;
    }
    if (spi_queue_node_remove(&spi_rx_queue[port], node_i) != 0)
    {
        return -ENODATA;
    }
    spi_queue_node_destroy(node_i);
    return 0;
}

/*!
 * @brief Releases a chip select lock
 *
 * Releases a chip select lock if it is currently active.  The lock is released by
 * placing a node with no data within the transmit queue.  When the node is executed
 * no data will be sent, but the chip select lock will be released, and the chip select
 * will be disabled.
 *
 * @param   params_p  Contains the SPI bus specific parameters for the transaction. These
 *                    include items such as the chip select and the transfer speed.
 *
 * @return 0 upon success
 */
int spi_release_cs_lock(SPI_DATA_PARAMETER_T *params_p)
{
    SPI_PORT_T port;
    static const SPI_IOVEC_T spi_null_trans_vectors =
        {
            .tx_base = NULL,
            .rx_base = NULL,
            .iov_len = 0
        };

    if (params_p == NULL)
    {
        return -EINVAL;
    }

    /* Get the port number for the transaction and do not continue if it is invalid. */
    port = spi_cs_to_port(params_p->cs);
    if (port >= SPI_PORT__END)
    {
        return -EINVAL;
    }
    
    if (spi_tx_queue[port].locked_cs != SPI_CS__END)
    {
        return spi_transceivev(params_p, (SPI_IOVEC_T *)&spi_null_trans_vectors, 0);
    }
    return 0;
}

/*!
 * @brief Initialize the SPI queues at power up.
 */
void __init spi_initialize(void)
{
    unsigned int i;

    DEBUG_MSG_LEVEL1(_k_d("spi_initialize"));
    for (i = 0; i < SPI_PORT__END; i++)
    {
        spi_tx_queue[i].head_i = SPI_QUEUE_NODE_UNUSED;
        spi_tx_queue[i].locked_cs = SPI_CS__END;
        spi_rx_queue[i].head_i = SPI_QUEUE_NODE_UNUSED;
        spi_rx_queue[i].locked_cs = SPI_CS__END;
        low_level_drv_tb[i].spi_initialize();
    }
    num_queue_entries = SPI_NUM_INIT_QUEUE_ENTRIES;
    queue_data_p = (SPI_QUEUE_NODE_T *)kmalloc(sizeof(SPI_QUEUE_NODE_T)*SPI_NUM_INIT_QUEUE_ENTRIES, GFP_KERNEL);
    for (i = 0; i < SPI_NUM_INIT_QUEUE_ENTRIES; i++)
    {
        queue_data_p[i].next_i = SPI_QUEUE_NODE_UNUSED;
    }
    /* Initialize the user space interface. */
    moto_spi_init();
    
#ifdef CONFIG_PM
    /* Ignore the response, since we do not use it at this time.  */
    (void)pm_register(PM_UNKNOWN_DEV, PM_SYS_UNKNOWN, spi_pm_callback);
#endif
}
