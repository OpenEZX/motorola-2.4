/*
 * arch/arm/mach-ixp1200/ixp1200-pci.c: 
 *
 * Generic PCI support for IXP1200 based systems. Heavilly based on
 * DEC21285 code.  Should probably just be merged with that at some
 * point?
 *
 * Maintainer: Deepak Saxena <dsaxena@mvista.com>
 *
 * Copyright (C) 2001 MontaVista Software, Inc.
 * Copyright (C) 1998-2000 Russell King, Phil Blundell
 *
 * May-27-2000: Uday Naik
 * 	Initial port to IXP1200 based on dec21285 code.
 *
 * Sep-25-2001: dsaxena
 * 	Port to 2.4.x kernel/cleanup
 */

#include <linux/config.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/ptrace.h>
#include <linux/interrupt.h>
#include <linux/mm.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/slab.h>

#include <asm/byteorder.h>
#include <asm/hardware.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/system.h>
#include <asm/mach/pci.h>

#include <asm/arch/pci.h>
#include <asm/arch/pci-auto.h>
#include <asm/arch/pci-bridge.h>

#define MAX_SLOTS		21

static unsigned long
ixp1200_base_address(struct pci_dev *dev, int where)
{
	unsigned long addr = 0;
	unsigned int devfn = dev->devfn;

	if(dev->bus->number == 0)
	{
		if(PCI_SLOT(devfn) == 0)
			addr = ARMCSR_BASE;

		else if (devfn < PCI_DEVFN(MAX_SLOTS, 0))
			addr = PCICFG0_BASE | 0xc00000 | devfn << 8 | where;
	}
	else
	{
		addr = PCICFG1_BASE | (dev->bus->number << 16) | (devfn <<8) \
			| where;
	}

	return addr;
}

static int
ixp1200_read_config_byte(struct pci_dev *dev, int where, u8 *value)
{
	unsigned long addr = ixp1200_base_address(dev, where);

	if(!addr)
		*value = 0xff;
	else
        	*value = *(u8 *)addr;

	return PCIBIOS_SUCCESSFUL;

}

static int
ixp1200_read_config_word(struct pci_dev *dev, int where, u16 *value)
{
	unsigned long addr = ixp1200_base_address(dev, where);

	if(!addr)
		*value = 0xffff;
	else
		*value = __le16_to_cpu((*(u16 *)addr));

	return PCIBIOS_SUCCESSFUL;

}

static int
ixp1200_read_config_dword(struct pci_dev *dev, int where, u32 *value)
{
	unsigned long addr = ixp1200_base_address(dev, where);

	if(!addr)
		*value = 0xffffffff;
	else
		*value = __le32_to_cpu((*(u32 *)addr));

	return PCIBIOS_SUCCESSFUL;
}

static int
ixp1200_write_config_byte(struct pci_dev *dev, int where, u8 value)
{
	unsigned long addr = ixp1200_base_address(dev, where);

	*(u8 *)addr = value;

	return PCIBIOS_SUCCESSFUL;
}

static int
ixp1200_write_config_word(struct pci_dev *dev, int where, u16 value)
{
	unsigned long addr = ixp1200_base_address(dev, where);

        *(u16 *)addr= __cpu_to_le16(value);

	return PCIBIOS_SUCCESSFUL;
}

static int
ixp1200_write_config_dword(struct pci_dev *dev, int where, u32 value)
{
	unsigned long addr = ixp1200_base_address(dev, where);

        *(unsigned long *)addr = __cpu_to_le32(value);

	return PCIBIOS_SUCCESSFUL;
}

static struct pci_ops ixp1200_ops = {
	ixp1200_read_config_byte,
	ixp1200_read_config_word,
	ixp1200_read_config_dword,
	ixp1200_write_config_byte,
	ixp1200_write_config_word,
	ixp1200_write_config_dword,
};

void __init ixp1200_setup_resources(struct resource **resource)
{
	struct resource *busmem, *busmempf;

	busmem = kmalloc(sizeof(*busmem), GFP_KERNEL);
	busmempf = kmalloc(sizeof(*busmempf), GFP_KERNEL);
	memset(busmem, 0, sizeof(*busmem));
	memset(busmempf, 0, sizeof(*busmempf));

	busmem->flags = IORESOURCE_MEM;
	busmem->name  = "PCI non-prefetch";

	allocate_resource(&iomem_resource, busmem, 0x20000000,
			  0x60000000, 0xffffffff, 0x20000000, NULL, NULL);

	resource[0] = &ioport_resource;
	resource[1] = busmem;
}

static struct pci_controller *hose = NULL;

void __init ixp1200_pci_init(struct arm_pci_sysdata *sysdata)
{
	u32 dummy_read;
	unsigned int mem_size, pci_cmd = IXP1200_PCI_COMMAND_IOSE |
                                         IXP1200_PCI_COMMAND_MSE  |
				         IXP1200_PCI_COMMAND_ME   |
                                         IXP1200_PCI_COMMAND_MWI;

        unsigned int big_endian_cntl   = SA_CNTL_BE_BEO |
					 SA_CNTL_BE_DEO |
					 SA_CNTL_BE_BEI |
                                         SA_CNTL_BE_DEI;
	int i,j,central;

	/* 
	 * Map in the whole of SDRAM (packets + linux space) into
	 * the PCI space. We just map in the max ammount of RAM.
	 * If a driver is properly written, this should not be an 
	 * issue.  The only place where it could be an issue is the
	 * IXP1200 is an agent on a PCI bus and there are a lot of
	 * devices with large memory requirements.
	 */
	mem_size = 256 * 1024 * 1024;

	*CSR_SA_CONTROL &= ~IXP1200_SA_CONTROL_PNR; /*assert PCI reset*/
	dummy_read = *CSR_SA_CONTROL;

        for(i=0;i<50;i++)
           j=i+1;

	/* Disable door bell and outbound interrupt*/
	*CSR_PCIOUTBINTMASK = 0xc;
	dummy_read = *CSR_SA_CONTROL;

	/*Disable doorbell int to PCI in doorbell mask register*/
	*CSR_DOORBELL_PCI = 0; 
	dummy_read = *CSR_SA_CONTROL;

	/*Disable doorbell int to SA-110 in Doorbell SA-110 mask register*/
        *CSR_DOORBELL_SA = 0x0;
	dummy_read = *CSR_SA_CONTROL;

	/* 
	 * Set PCI Address externsion reg to known state
	 * 
	 * We setup a 1:1 map of bus<->physical addresses
	 */
        *CSR_PCIADDR_EXTN = 0x54006000; 
	dummy_read = *CSR_SA_CONTROL;

#ifdef __ARMEB__
	*CSR_SA_CONTROL |= big_endian_cntl; /* set swap bits for big endian*/
	dummy_read = *CSR_SA_CONTROL;
#endif

        *CSR_SA_CONTROL |= IXP1200_SA_CONTROL_PNR; /*Negate PCI reset*/
	dummy_read = *CSR_SA_CONTROL;

        for(i=0; i< 45; i++)
            j=i+1;

        *CSR_SDRAMBASEMASK = mem_size - 0x40000;
	dummy_read = *CSR_SA_CONTROL;

 	*CSR_CSRBASEMASK = 0x000c0000; /*1 Mbyte*/
	dummy_read = *CSR_SA_CONTROL;

	/* Clear PCI command register*/
        *CSR_PCICMD &= 0xffff0000; 
	dummy_read = *CSR_SA_CONTROL;

        central = IXP1200_SA_CONTROL_PCF & *CSR_SA_CONTROL;

	central = 1;

        if(central)
        {
		printk("PCI: IXP1200 is system controller\n");
		*CSR_PCICSRBASE = 0x40000000;
		dummy_read = *CSR_SA_CONTROL;

		*CSR_PCICSRIOBASE = 0x0000f000;
		dummy_read = *CSR_SA_CONTROL; 

		*CSR_PCISDRAMBASE = 0; /* assume our SDRAM is at 0 */
		dummy_read = *CSR_SA_CONTROL;

		*CSR_PCICMD = (*CSR_PCICMD & 0xFFFF0000)| pci_cmd;
		dummy_read = *CSR_SA_CONTROL;

        	*CSR_SA_CONTROL |= 0x00000001;
		dummy_read = *CSR_SA_CONTROL;

		*CSR_PCICACHELINESIZE = 0x00002008;
		dummy_read = *CSR_SA_CONTROL;

		hose = pcibios_alloc_controller();
		if(!hose) panic("Could not allocate PCI hose!");

		hose->first_busno = 0;
		hose->last_busno = 0;
		hose->io_space.start = 0x54000000;
		hose->io_space.end = 0x5400ffff;
		hose->mem_space.start = 0x60000000;
		hose->mem_space.end = 0x7fffffff;

		/* Re-Enumarate the bus */
		hose->last_busno = pciauto_bus_scan(hose, 0);

		/* Scan the bus */
		pci_scan_bus(0, &ixp1200_ops, sysdata);
	}
	else 
	{
		/*
		 * In agent mode we don't have to do anything.
		 * We setup the window sizes above and it's up to
		 * the host to properly configure us.
		 */
		printk("PCI: IXP1200 is agent\n");
	}

}


#define EARLY_PCI_OP(rw, size, type)					\
int early_##rw##_config_##size(struct pci_controller *hose, int bus,	\
		int devfn, int offset, type value)	\
{									\
	return ixp1200_##rw##_config_##size(fake_pci_dev(hose, bus, devfn),	\
			offset, value);			\
}

EARLY_PCI_OP(read, byte, u8 *)
EARLY_PCI_OP(read, word, u16 *)
EARLY_PCI_OP(read, dword, u32 *)
EARLY_PCI_OP(write, byte, u8)
EARLY_PCI_OP(write, word, u16)
EARLY_PCI_OP(write, dword, u32)


