/*
   -------------------------------------------------------------------------
   i2c-adap-ibm_ocp.c i2c-hw access for the IIC peripheral on the IBM PPC 405
   -------------------------------------------------------------------------
  
   Current Maintainers:
   Ian DaSilva and Armin Kuster, MontaVista Software, Inc.
   idasilva@mvista.com , akuster@mvista.com or source@mvista.com

   Copyright 2000-2002 MontaVista Software Inc.

   Changes made to support the IIC peripheral on the IBM PPC 405 

   ----------------------------------------------------------------------------
   This file was highly leveraged from i2c-elektor.c, which was created
   by Simon G. Vogl and Hans Berglund:

 
     Copyright (C) 1995-97 Simon G. Vogl
                   1998-99 Hans Berglund

   With some changes from Kyösti Mälkki <kmalkki@cc.hut.fi> and even
   Frodo Looijaard <frodol@dds.nl>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
   ----------------------------------------------------------------------------

   History: 01/20/12 - Armin
   	akuster@mvista.com
   	ported up to 2.4.16+	

   Version 02/03/25 - Armin
       converted to ocp format
       removed commented out or #if 0 code

   Version 1.3: 04/25/02
   	coverted c++ stlye comments to c
	added IIC_DEV macro
	the i2c_adapter->algo_data is a pointer to the iic_algo_i2c_data struct
	the i2c_adapter->data is a pointer to the ocp_struct for the i2c device
	general cleanups

    Version 1.4: 04/30/02 
	added IIC_PORT_DNFS
    Version 1.5: 05/02/02
	fixed mem leak added ocp_dev_free if ocp_reg failed

    Version: 1.6 - Armin
       no longer need extern const paddr structs

    Version: 1.8 - Armin
       name change of ocp_get_dev
       name change for *_driver to *_dev

   TODO: add PM hooks
    
*/

#include <linux/kernel.h>
#include <linux/ioport.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/init.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <linux/i2c.h>
#include <linux/i2c-id.h>
#include <asm/ocp.h>

/*
 * This next section is configurable, and it is used to set the number
 * of i2c controllers in the system.  The default number of instances is 1,
 * however, this should be changed to reflect your system's configuration.
 */

static int i2c_debug = 0;
static wait_queue_head_t iic_wait[2];
static int iic_pending;

extern int i2c_iic_add_bus(struct i2c_adapter *adap);
extern int i2c_iic_del_bus(struct i2c_adapter *adap);

/* ----- global defines -----------------------------------------------	*/
#define DEB(x)	if (i2c_debug>=1) x
#define DEB2(x) if (i2c_debug>=2) x
#define DEB3(x) if (i2c_debug>=3) x
#define DEBE(x)	x		/* error messages                               */

#define IIC_DEV(x)	((struct ocp_dev *)adap->data)->x

/* ----- local functions ----------------------------------------------	*/

/*
 * Description:  Put this process to sleep.  We will wake up when the
 * IIC controller interrupts.
 */
void
iic_ibmocp_waitforpin(void *data)
{

	int timeout = 2;
	struct ocp_dev *iic_dev;

	iic_dev = (struct ocp_dev *) data;

	/*
	 * If interrupts are enabled (which they are), then put the process to
	 * sleep.  This process will be awakened by two events -- either the
	 * the IIC peripheral interrupts or the timeout expires. 
	 */
	if (iic_dev->irq_resource[0][0].irq > 0) {
		cli();
		if (iic_pending == 0) {
			interruptible_sleep_on_timeout(&
						       (iic_wait[iic_dev->num]),
						       timeout * HZ);
		} else
			iic_pending = 0;
		sti();
	} else {
		/*
		 * If interrupts are not enabled then delay for a reasonable amount
		 * of time and return.  We expect that by time we return to the calling
		 * function that the IIC has finished our requested transaction and
		 * the status bit reflects this.
		 * 
		 * udelay is probably not the best choice for this since it is
		 * the equivalent of a busy wait
		 */
		udelay(100);
	}
}

/*
 * Description: The registered interrupt handler
 */
static void
iic_ibmocp_handler(int this_irq, void *dev_id, struct pt_regs *regs)
{
	int ret;
	struct iic_regs *iic;
	struct ocp_dev *iic_dev;

	iic_dev = (struct ocp_dev *) dev_id;
	iic = (struct iic_regs *) iic_dev->vaddr;

	iic_pending = 1;

	DEB2(printk("iic_ibmocp_handler: in interrupt handler\n"));
	/* Read status register */
	ret = readb((int) &(iic->sts));
	DEB2(printk("iic_ibmocp_handler: status = %x\n", ret));
	/* Clear status register.  See IBM PPC 405 reference manual for details */
	writeb(0x0a, (int) &(iic->sts));
	wake_up_interruptible(&(iic_wait[iic_dev->num]));
}

/*
 * Description: Release irq and memory
 */
static void
iic_ibmocp_release(void)
{
	int i;
	struct ocp_dev *iic_drv;

	for (i = 0; i < ocp_get_max(IIC); i++) {
		iic_drv = ocp_get_dev(IIC, i);
		if (iic_drv->irq_resource[0][0].irq > 0) {
			disable_irq(iic_drv->irq_resource[0][0].irq);
			free_irq(iic_drv->irq_resource[0][0].irq, 0);
		}
	}

}

/*
 * Description: Does nothing
 */
static int
iic_ibmocp_reg(struct i2c_client *client)
{
	return 0;
}

/*
 * Description: Does nothing
 */
static int
iic_ibmocp_unreg(struct i2c_client *client)
{
	return 0;
}

/*
 * Description: If this compiled as a module, then increment the count
 */
static void
iic_ibmocp_inc_use(struct i2c_adapter *adap)
{
#ifdef MODULE
	MOD_INC_USE_COUNT;
#endif
}

/*
 * Description: If this is a module, then decrement the count
 */
static void
iic_ibmocp_dec_use(struct i2c_adapter *adap)
{
#ifdef MODULE
	MOD_DEC_USE_COUNT;
#endif
}

/*
 * Description: Called when the module is loaded.  This function starts the
 * cascade of calls up through the heirarchy of i2c modules (i.e. up to the
 *  algorithm layer and into to the core layer)
 */
static int __init
iic_ibmocp_init(void)
{
	struct ocp_dev *iic_drv;
	struct i2c_adapter *adap;
	int curr_iic = 0;

	printk(KERN_INFO "iic_ibmocp_init: IBM on-chip iic adapter module\n");
	
	while (curr_iic != -ENXIO) {
		if (!(iic_drv = ocp_alloc_dev(sizeof (struct i2c_adapter))))
			return -ENOMEM;
		iic_drv->type = IIC;
		if ((curr_iic = ocp_register(iic_drv)) != -ENXIO) {
			adap = (struct i2c_adapter *) iic_drv->ocpdev;
			iic_drv->vaddr = ioremap(iic_drv->paddr,17);
			DEB(printk
			    ("iic_hw_resrc_init: Physical Base address: 0x%lx\n",
			     iic_drv->paddr));
			DEB(printk
			    ("iic_hw_resrc_init: ioremapped base address: 0x%lx\n",
			     iic_drv->vaddr));
			DEB(printk
			    ("Adapter irq %x\n",
			     iic_drv->irq_resource[0][0].irq));

			strcpy(adap->name, "IBM OCP IIC adapter");
			adap->data = (void *) iic_drv;
			adap->id = I2C_HW_OCP;
			adap->algo = NULL;
			adap->inc_use = iic_ibmocp_inc_use;
			adap->dec_use = iic_ibmocp_dec_use;
			adap->client_register = iic_ibmocp_reg;
			adap->client_unregister = iic_ibmocp_unreg;

			init_waitqueue_head(&(iic_wait[curr_iic]));

			if (iic_drv->irq_resource[0][0].irq > 0) {
				if (request_irq
				    (iic_drv->irq_resource[0][0].irq,
				     iic_ibmocp_handler, 0, "IBM OCP IIC",
				     iic_drv)) {
					printk(KERN_ERR "iic_hw_resrc_init: Request irq%d 
				failed\n", iic_drv->irq_resource[0][0].
					       irq);
					iic_drv->irq_resource[0][0].irq = 0;
				} else {
					DEB3(printk
					     ("iic_hw_resrc_init: Enabled interrupt\n"));
				}
			}

			if (i2c_iic_add_bus(adap) < 0)
				return -ENODEV;

			DEB(printk
			    (KERN_INFO
			     "iic_ibmocp_init: found device at %#lx.\n\n",
			     iic_drv->vaddr));
		} else {
			ocp_free_dev(iic_drv);
			break;
		}		/* end if */
	}			/* end while */
	return (curr_iic == -ENXIO) ? 0 : curr_iic;
}

static void
iic_ibmocp_exit(void)
{
	int i;
	struct ocp_dev *iic_drv;
	struct i2c_adapter *adap;

	for (i = 0; i < ocp_get_max(IIC); i++) {
		iic_drv = ocp_get_dev(IIC, i);
		adap = (struct i2c_adapter *) iic_drv->ocpdev;
		i2c_iic_del_bus(adap);
	}

	iic_ibmocp_release();
}

EXPORT_SYMBOL(iic_ibmocp_waitforpin);
/*
 * If modules is NOT defined when this file is compiled, then the MODULE_*
 * macros will resolve to nothing
 */
MODULE_AUTHOR("MontaVista Software <www.mvista.com>");
MODULE_DESCRIPTION("I2C-Bus adapter routines for IBM OCP IIC bus adapter");
MODULE_PARM(i2c_debug, "i");
MODULE_LICENSE("GPL");

module_init(iic_ibmocp_init);
module_exit(iic_ibmocp_exit);
