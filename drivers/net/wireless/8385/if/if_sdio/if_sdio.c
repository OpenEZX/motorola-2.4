/**
 * File : if_sdio.c
 *
 * Motorola Confidential Proprietary
 * Copyright (C) Motorola 2005. All rights reserved.
 */

#include	"if_sdio.h"



/* 
 * Define the SD Block Size
 * for SD8381-B0/B1, SD8385/B0 chip, it could be 32,64,128,256,512
 * for all other chips including SD8381-A0, it must be 32
 */

#define SD_BLOCK_SIZE		32
#define SD_BLOCK_SIZE_512	512
#define SD_BLOCK_SIZE_320	320
#define SDIO_HEADER_LEN		4
//#define FW_DOWNLOAD_SPEED

#ifdef FW_WAKEUP_METHOD
#ifdef _MAINSTONE
#define GPIO_PORT_NUM		24
#define GPIO_PORT_INIT()	(set_GPIO_mode((GPIO_PORT_NUM) | GPIO_OUT))
#define GPIO_PORT_TO_HIGH()	(GPSR0 = 1 << GPIO_PORT_NUM)
#define GPIO_PORT_TO_LOW()	(GPCR0 = 1 << GPIO_PORT_NUM)
#else /* !_MAINSTONE */
#define GPIO_PORT_NUM		
#define GPIO_PORT_INIT()        
#define GPIO_PORT_TO_HIGH() 	wprm_trigger_host_wlan_wakeb(0) /* de-trigger HOST_WLAN_WAKEB */ 
#define GPIO_PORT_TO_LOW()	wprm_trigger_host_wlan_wakeb(1) /* trigger HOST_WLAN_WAKEB 
                                                                 * to wakeup Firmware from deep sleep mode 
                                                                 */
#endif /* _MAINSTONE */
#endif /* FW_WAKEUP_METHOD */

/*
 * Poll the Card Status register until the bits specified in the argument
 * are turned on.
 */

mmc_notifier_rec_t if_sdio_notifier;
isr_notifier_fn_t isr_function;

static wlan_private *pwlanpriv;
static wlan_private *(* wlan_add_callback)(void *dev_id);
static int (* wlan_remove_callback)(void *dev_id);

int mv_sdio_read_event_cause(wlan_private *priv);

#ifdef FW_WAKEUP_TIME
extern unsigned long wt_pwrup_sending,wt_pwrup_sent,wt_int;
#endif

void sbi_interrupt(int irq, void *dev_id, struct pt_regs *fp)
{
	struct net_device	*dev = dev_id;
#ifdef TXRX_DEBUG
	wlan_private   *priv = dev->priv;

	PRINTK("sbi_interrupt called IntCounter= %d\n",
					priv->adapter->IntCounter);
#endif

#ifdef FW_WAKEUP_TIME
	if((wt_pwrup_sending!=0L) && (wt_int==0L))
		wt_int = get_utimeofday();
#endif

	TXRX_DEBUG_GET_ALL(0x00, 0xff, 0xff);
	wlan_interrupt(dev);

	LEAVE1();
}

static inline int sbi_add_card(void *card)
{
	if (!wlan_add_callback)
		return -ENODEV;

	pwlanpriv = wlan_add_callback(card);

	if(pwlanpriv)
		return 0;
	else
		return -ENODEV;
}

static inline int sbi_remove_card(void *card)
{
	if (!wlan_remove_callback)
		return -ENODEV;
		
	pwlanpriv = NULL;
	return wlan_remove_callback(card); 
}

int *sbi_register(wlan_notifier_fn_add add, wlan_notifier_fn_remove remove, 
								void *arg)
{
	int *sdio_ret;

	wlan_add_callback = add;
	wlan_remove_callback = remove;
	
	if_sdio_notifier.add = (mmc_notifier_fn_t) sbi_add_card;
	if_sdio_notifier.remove = (mmc_notifier_fn_t) sbi_remove_card;
	isr_function = sbi_interrupt;
	sdio_ret = sdio_register(MMC_REG_TYPE_USER, 
					&if_sdio_notifier, 0);

#ifdef FW_WAKEUP_METHOD
	// init GPIO PORT for wakeup purpose
	GPIO_PORT_INIT();

	// set default value
	GPIO_PORT_TO_HIGH();
#endif
	
	return sdio_ret;
}

void sbi_unregister(void)
{
	sdio_unregister(MMC_REG_TYPE_USER, &if_sdio_notifier);
}

int sbi_read_ioreg(wlan_private *priv, u8 func, u32 reg, u8 *dat)
{
	return sdio_read_ioreg((mmc_card_t)priv->wlan_dev.card, func, reg, dat);
}

int sbi_write_ioreg(wlan_private *priv, u8 func, u32 reg, u8 dat)
{
	return sdio_write_ioreg((mmc_card_t)priv->wlan_dev.card, func, reg, dat);
}

int sbi_read_intr_reg(wlan_private *priv, u8 *ireg)
{
	return sdio_read_ioreg((mmc_card_t)priv->wlan_dev.card, FN1, 
						HOST_INTSTATUS_REG, ireg);
}

int sbi_read_card_reg(wlan_private *priv, u8 *cs)
{
	int ret;

	ret = sdio_read_ioreg((mmc_card_t)priv->wlan_dev.card, FN1, 
							CARD_STATUS_REG, cs);
	
	return ret;
}

int sbi_clear_int_status(wlan_private *priv , u8 mask)
{
	int ret;

	ret = sdio_write_ioreg((mmc_card_t)priv->wlan_dev.card, FN1, 
						HOST_INTSTATUS_REG, 0x0);

	return ret;
}

int sbi_get_int_status(wlan_private *priv , u8 *ireg)
{
	int 		ret;
	u8		cs = 0;
	u8  		*cmdBuf;
	wlan_dev_t	*wlan_dev = &priv->wlan_dev;
	mmc_card_t	card = (mmc_card_t) wlan_dev->card;
	struct sk_buff	*skb;
	
	TXRX_DEBUG_GET_ALL(0x01, 0xff, 0xff);

	if ((ret = sdio_read_ioreg( card, FN1, HOST_INTSTATUS_REG, ireg)) < 0) {
		printk("sdio_read_ioreg: reading interrupt status register"
								" failed\n");
		goto end;
	}

	TXRX_DEBUG_GET_ALL(0x02, *ireg, 0xff);

	if(*ireg != 0) {/* DN_LD_HOST_INT_STATUS and/or UP_LD_HOST_INT_STATUS */
		/* Clear the interrupt status register */
		PRINTK("ireg = 0x%x, ~ireg = 0x%x\n",*ireg,~(*ireg));
		if ((ret = sdio_write_ioreg(card, FN1, HOST_INTSTATUS_REG, 
							~(*ireg) & 0x03)) < 0) {
			printk("sdio_write_ioreg: clear interrupt status" 
							" register failed\n");
			goto end;
		}
	}
	TXRX_DEBUG_GET_ALL(0x03, *ireg, 0xff);
/*
	ret = sdio_read_ioreg((mmc_card_t)priv->wlan_dev.card, FN1, 
							CARD_STATUS_REG, &cs);
	if (ret < 0 ) {
		printk("sdio_read_ioreg:reading card status register failed\n");
		goto end;
	}

	PRINTK("INT ireg=0x%x  CARD cs=0x%x\n",*ireg,cs);
*/
	TXRX_DEBUG_GET_ALL(0x04, *ireg, cs);

#ifdef TXRX_DEBUG
	if (trd_p != 0) {
		trd_p = 0;
		printk("************************************ INT ireg=0x%x"
				" CARD cs=0x%x dnld_sent=%d\n", *ireg, cs, 
						priv->wlan_dev.dnld_sent);
	}
#endif

	if ((card->hw_tx_done)
//	   && (priv->wlan_dev.dnld_sent != DNLD_RES_RECEIVED)	//waiting for tx_done INT
	   && (*ireg & DN_LD_HOST_INT_STATUS)	//tx_done INT
	   ) {
		*ireg |= HIS_TxDnLdRdy;
		if (!priv->wlan_dev.dnld_sent) { // tx_done already received
			PRINTK("warning: tx_done already received:"
					" dnld_sent=0x%x ireg=0x%x cs=0x%x\n",
					priv->wlan_dev.dnld_sent,*ireg,cs);
			TXRX_DEBUG_GET_ALL(0x05, *ireg, cs);
		} else {
			PRINTK("tx_done received: dnld_sent=0x%x ireg=0x%x"
					" cs=0x%x\n", priv->wlan_dev.dnld_sent,
					*ireg,cs);
			priv->wlan_dev.dnld_sent = DNLD_RES_RECEIVED;
			TXRX_DEBUG_GET_ALL(0x06, *ireg, cs);
		}
	}

	/* if !priv->adapter->NumTransfersRx -- To initiate the Rx Transfer */
	if (
#ifdef THROUGHPUT_TEST
	(
#endif /* THROUGHPUT_TEST */
		*ireg & UP_LD_HOST_INT_STATUS
#ifdef THROUGHPUT_TEST
		||  !priv->adapter->NumTransfersRx) && 
					(priv->adapter->ThruputTest & 0x02)
#endif /* THROUGHPUT_TEST */
	) {
		if(!(skb = dev_alloc_skb(MRVDRV_MAX_SKB_LEN))) {
			printk(KERN_ERR "No free skb\n");
			priv->stats.rx_dropped++;
			return -1;
		}

		TXRX_DEBUG_GET_ALL(0x07, *ireg, cs);
		/* Transfer data from card */
		/* TODO: Check for error return on the read */
		/* skb->tail is passed as we are calling skb_put after we
		 * are reading the data */
		if (mv_sdio_card_to_host(priv,&wlan_dev->upld_typ,
			(int *)&wlan_dev->upld_len, skb->tail,
							MRVDRV_MAX_SKB_LEN) < 0) {
			TXRX_DEBUG_GET_ALL(0x08, *ireg, cs);
			printk("<1>Card to host failed: ireg=0x%x cs=0x%x\n",
								*ireg, cs);
			if (sdio_read_ioreg(wlan_dev->card, FN1,
						CONFIGURATION_REG,&cs) < 0)
				printk(KERN_ERR "sdio_read_ioreg failed\n");

			PRINTK("Config Reg val = %d\n",cs);
			if (sdio_write_ioreg(wlan_dev->card, FN1,
					CONFIGURATION_REG, (cs | 0x04)) < 0)
				printk(KERN_ERR "write ioreg failed\n");

			PRINTK("write success\n");
			if (sdio_read_ioreg(wlan_dev->card, FN1,
					CONFIGURATION_REG, &cs) < 0 )
				printk(KERN_ERR "sdio_read_ioreg failed\n");

			printk(KERN_DEBUG "Config reg val =%x\n",cs);
			ret = -1;
			goto end;
		}
			
		TXRX_DEBUG_GET_ALL(0x09, *ireg, cs);
		if (wlan_dev->upld_typ == MVSD_DAT) {
			*ireg |= HIS_RxUpLdRdy;
			skb_put(skb, priv->wlan_dev.upld_len);
			skb_pull(skb, SDIO_HEADER_LEN);
			list_add_tail((struct list_head *) skb,
				(struct list_head *) &priv->adapter->RxSkbQ);
		} else if (wlan_dev->upld_typ == MVSD_CMD) {
			*ireg &= ~(HIS_RxUpLdRdy);
			*ireg |= HIS_CmdUpLdRdy;

			/* take care of CurCmd = NULL case by reading the 
			 * data to clear the interrupt */
			if(!priv->adapter->CurCmd) {
				cmdBuf = priv->wlan_dev.upld_buf;
				priv->adapter->HisRegCpy &= ~HIS_CmdUpLdRdy;
			} else {
				cmdBuf = priv->adapter->CurCmd->BufVirtualAddr;
			}
			
			priv->wlan_dev.upld_len -= SDIO_HEADER_LEN;
			memcpy(cmdBuf, skb->data + SDIO_HEADER_LEN,
				       		priv->wlan_dev.upld_len);
			kfree_skb(skb);
		}
		else if (wlan_dev->upld_typ == MVSD_EVENT) {
			/* Read the event cause immediately and process
			 * the dummy packet event if present to set
			 * the TxDnLdRdy interrupt bit
			 */
//			mv_sdio_read_event_cause(priv);

			if ((priv->adapter->EventCause >> SBI_EVENT_CAUSE_SHIFT)
						== MACREG_INT_CODE_DUMMY_PKT) {
				*ireg |= HIS_TxDnLdRdy;
				*ireg &= ~(HIS_RxUpLdRdy);
				priv->wlan_dev.dnld_sent = DNLD_RES_RECEIVED;
			} else {
				*ireg |= HIS_CardEvent;
			}
			kfree_skb(skb);
		}
		
		TXRX_DEBUG_GET_ALL(0x0A, *ireg, cs);
		*ireg |= HIS_CmdDnLdRdy;
//		priv->adapter->HisRegCpy |= *ireg;
		TXRX_DEBUG_GET_ALL(0x0B, *ireg, cs);
	}

	ret = 0;
end:

	return ret;
}

int sbi_poll_cmd_dnld_rdy(wlan_private *priv)
{
	return mv_sdio_poll_card_status(priv, IO_READY | UP_LD_CARD_RDY);
}

int sbi_card_to_host(wlan_private *priv, u32 type, u32 *nb, u8 *payload, 
								u16 npayload)
{
	return 0;
}

int sbi_read_event_cause(wlan_private *priv)
{
	return 0;
}

int mv_sdio_read_event_cause(wlan_private *priv)
{
	int	ret;
	u8	scr2;

	/* the SCRATCH_REG @ 0x8fc tells the cause for the Mac Event */
	if ((ret = sdio_read_ioreg(priv->wlan_dev.card, FN0, 0x80fc, &scr2)) 
									< 0) {
		PRINTK("Unable to read Event cause\n");
		return ret;
	}

	priv->adapter->EventCause = scr2;

	return 0;
}

int sbi_retrigger(wlan_private *priv)
{
	mmc_card_t card = (mmc_card_t)priv->wlan_dev.card;
	
	if (sdio_write_ioreg(card, FN1, CONFIGURATION_REG, 0x4) < 0) {
				return -1;
	}
	
	return 0;
}

int if_dnld_ready(wlan_private *priv)
{
	int rval;
	u8 cs;
	wlan_dev_t	*sdiodev = &priv->wlan_dev;
	mmc_card_t	card = (mmc_card_t) sdiodev->card;
	rval = sdio_read_ioreg(card, FN1, CARD_STATUS_REG, &cs);
	if(rval < 0)
		return -EBUSY;
	
	return (cs & DN_LD_CARD_RDY) && (cs & IO_READY); 
}

int sbi_is_tx_download_ready(wlan_private *priv)
{
	int rval;
	
	rval = if_dnld_ready(priv);
	
	return (rval < 0) ? -1 : (rval == 1) ? 0 : -EBUSY; //Check again
}

int sbi_reenable_host_interrupt(wlan_private *priv, u8 bits)
{
    	//mmc_card_t  card = (mmc_card_t) priv->wlan_dev.card;

	sdio_enable_SDIO_INT();
	return 0;

}

int sbi_disable_host_int(wlan_private *priv, u8 mask)
{
	int	ret;
	u8	host_int_mask;

	/* Read back the host_int_mask register */
	ret = sdio_read_ioreg(priv->wlan_dev.card, FN1, HOST_INT_MASK_REG, 
								&host_int_mask);
	if (ret < 0)
		goto done;

	/* Update with the mask and write back to the register */
	host_int_mask &= ~mask;
	ret = sdio_write_ioreg(priv->wlan_dev.card, FN1, HOST_INT_MASK_REG, 
								host_int_mask);
	if(ret<0)
		printk("Unable to diable the host interrupt!\n");

done:
	return ret;
}

int sbi_enable_host_int (wlan_private *priv , u8 mask)
{

	int	ret;
	u8	imask;
	mmc_card_t  card = (mmc_card_t) priv->wlan_dev.card;

	imask = UP_LD_HOST_INT_MASK;
//	if(card->hw_tx_done)
		imask |= DN_LD_HOST_INT_MASK;

	/* Simply write the mask to the register */
	ret = sdio_write_ioreg(card, FN1, HOST_INT_MASK_REG, mask);

#if DEBUG2
	PRINTK(KERN_INFO "ret = %d\n", ret);
#endif

	if(card->hw_tx_done)
		priv->adapter->HisRegCpy = 1;

	return ret;

}

/*
 * This function puts the bus into low power mode
 */
int sbi_enter_bus_powersave(wlan_private *priv)
{
    int ret;
    mmc_card_t  card = (mmc_card_t) priv->wlan_dev.card;
    ENTER();

    ret = sdio_enter_bus_powersave(card);

    LEAVE();
    return ret;
}

/*
 * This function removes the bus from low power mode
 */
int sbi_exit_bus_powersave(wlan_private *priv)
{
    int ret;
    mmc_card_t  card = (mmc_card_t) priv->wlan_dev.card;
    ENTER();

    ret = sdio_exit_bus_powersave(card);

    LEAVE();
    return ret;
}

int sbi_unregister_dev(wlan_private *priv)
{
	ENTER();

	if (priv->wlan_dev.card != NULL) {
		/* Release the SDIO IRQ */
		sdio_free_irq(priv->wlan_dev.card, priv->wlan_dev.netdev);
#if 0
		/* Stop the kernel thread */
		stop_kthread(&wlan_dev->kth_irq);
		
#endif
	PRINTK("Making the sdio dev card as NULL\n");
	//	priv->wlan_dev.card=NULL;
	}
	LEAVE();
	return 0;
}

int mv_sdio_poll_card_status(wlan_private *priv, u8 bits)
{
	int	tries;
	int	rval;
	u8	cs;

	for (tries = 0; tries < MAX_POLL_TRIES; tries++) {
		rval = sdio_read_ioreg(priv->wlan_dev.card, FN1,
							CARD_STATUS_REG, &cs);
		    //PRINTK("rval = %x\n cs&bits =%x\n",rval, (cs&bits));
		if (rval == 0 && (cs & bits) == bits) {
			return 0;
		}

		udelay(100);  
	}

	PRINTK("mv_sdio_poll_card_status: FAILED!\n");
	return -EBUSY;
}

int sbi_register_dev(wlan_private *priv)
{
	int	ret;
	u8	reg;
	mmc_card_t	card = (mmc_card_t) priv->wlan_dev.card;
	ENTER();  

	/* Initialize the private structure */
	strncpy(priv->wlan_dev.name, "sdio0", sizeof(priv->wlan_dev.name));
	priv->wlan_dev.ioport = 0;
	priv->wlan_dev.upld_rcv = 0;
	priv->wlan_dev.upld_typ = 0;
	priv->wlan_dev.upld_len = 0;

	/* Read the IO port */
	ret = sdio_read_ioreg(priv->wlan_dev.card, FN1, IO_PORT_0_REG, &reg);
	if (ret < 0)
		goto failed;
	else
		priv->wlan_dev.ioport |= reg;

	ret = sdio_read_ioreg(priv->wlan_dev.card, FN1, IO_PORT_1_REG, &reg);
	if (ret < 0)
		goto failed;
	else
		priv->wlan_dev.ioport |= (reg << 8);

	ret = sdio_read_ioreg(priv->wlan_dev.card, FN1, IO_PORT_2_REG, &reg);
	if (ret < 0)
		goto failed;
	else
		priv->wlan_dev.ioport |= (reg << 16);

	PRINTK("SDIO FUNC1 IO port: 0x%x\n", priv->wlan_dev.ioport);

	/* Disable host interrupt first. */
	if ((ret = sbi_disable_host_int(priv, 0xff)) < 0) {
		printk("Warning: unable to disable host interrupt!\n");
	}

	/* Request the SDIO IRQ */
	PRINTK("Before request_irq Address is if==>%p\n",isr_function);
	ret = sdio_request_irq(priv->wlan_dev.card,
				(handler_fn_t)isr_function, 0,
				"sdio_irq", priv->wlan_dev.netdev);

	PRINTK("IrqLine: %d\n", card->ctrlr->tmpl->irq_line);

	if (ret < 0) {
		printk("Failed to request IRQ on SDIO bus (%d)\n", ret);
		goto failed;
	}
	
	priv->wlan_dev.netdev->irq = card->ctrlr->tmpl->irq_line;
	priv->adapter->irq = priv->wlan_dev.netdev->irq;
	priv->adapter->chip_rev = card->chiprev;

	return 0;  /* success */

failed:
	priv->wlan_dev.card = NULL;

	return ret;
}

/*
 * This fuction is used for sending data to the SDIO card.
 */

int sbi_host_to_card(wlan_private *priv, u8 type, u8 *payload, u16 nb)
{
	int	ret;
	int	buf_block_len;
	int	blksz;
	mmc_card_t	card = (mmc_card_t) priv->wlan_dev.card;

	ENTER();

	TXRX_DEBUG_GET_ALL(0x80, 0xff, 0xff);
	TXRX_DEBUG_GET_ALL(0x81, 0xff, 0xff);
	
	if(card->hw_tx_done)
		priv->adapter->HisRegCpy = 0;

	/* Allocate buffer and copy payload */
	if(card->block_size_512) {
		blksz = SD_BLOCK_SIZE_320;
		buf_block_len = (nb + 4 + blksz - 1) / blksz;
	}
	else {
		blksz = SD_BLOCK_SIZE;
		buf_block_len = (nb + 4 + blksz - 1) / blksz;
	}

	priv->adapter->TmpTxBuf[0] = (nb + 4) & 0xff;
	priv->adapter->TmpTxBuf[1] = ((nb + 4) >> 8) & 0xff;
	priv->adapter->TmpTxBuf[2] = (type == MVSD_DAT) ? 0 : 1;
	priv->adapter->TmpTxBuf[3] = 0x0;

	if (payload != NULL && nb > 0) {
		memcpy(&priv->adapter->TmpTxBuf[4], payload, nb);
	}
	else {
		printk("sbi_host_to_card(): Error: payload=%p, nb=%d\n",
								payload,nb);
	}
  
	/* The host polls for the IO_READY bit */
	ret = mv_sdio_poll_card_status(priv, IO_READY);
	if (ret < 0) {
		printk("<1> Poll failed in host_to_card : %d\n", ret);
		return ret;
	}

	/* Transfer data to card */
	ret = sdio_write_iomem(priv->wlan_dev.card, FN1, priv->wlan_dev.ioport,
			BLOCK_MODE, FIXED_ADDRESS, buf_block_len,
			blksz, priv->adapter->TmpTxBuf);
	TXRX_DEBUG_GET_ALL(0x82, 0xff, 0xff);
	if(type == MVSD_DAT)
		priv->wlan_dev.dnld_sent = DNLD_DATA_SENT;
	else
		priv->wlan_dev.dnld_sent = DNLD_CMD_SENT;
	
	TXRX_DEBUG_GET_ALL(0x82, 0xff, 0xff);
#if DEBUG
	PRINTK("sdio write -dnld val =>%d\n",ret);
#endif
	
	LEAVE();
	return ret;
}

/*
 * This function is used to read data from the card.
 */

int mv_sdio_card_to_host(wlan_private *priv,
			 u32 *type, int *nb, u8 *payload, int npayload)
{
	int ret = -EIO;
	u16 buf_len = 0;
	int buf_block_len;
	int blksz;
	mmc_card_t card = (mmc_card_t) priv->wlan_dev.card;

	if (!payload) {
		printk("payload NULL pointer received!\n");
		return -EINVAL;
	}

	/* Read the length of data to be transferred */
	ret = mv_sdio_read_scratch(priv, &buf_len);
	if (ret < 0)
		return ret;
#if DEBUG2
	PRINTK("Receiving %d bytes from card at scratch reg value\n", buf_len);
#endif
	if (buf_len-4 <= 0 || buf_len-4 > npayload) {
		printk("Invalid packet size from firmware, size = %d\n", 
								buf_len);
		buf_len = npayload + 4;
	}
  
	/* Allocate buffer */
	if(card->block_size_512) {
		blksz = SD_BLOCK_SIZE_320;
		buf_block_len = (buf_len + blksz - 1) / blksz;
	}
	else {
		blksz = SD_BLOCK_SIZE;
		buf_block_len = (buf_len + blksz - 1) / blksz;
	}

	/* The host polls for the IO_READY bit */
	ret = mv_sdio_poll_card_status(priv, IO_READY);
	if (ret < 0) {
		printk("<1> Poll failed in card_to_host : %d\n", ret);
		return ret;
	}

 	ret = sdio_read_iomem(priv->wlan_dev.card, FN1, priv->wlan_dev.ioport,
				BLOCK_MODE, FIXED_ADDRESS, buf_block_len,
				blksz, payload);

	if (ret < 0) {
		printk("<1>sdio_read_iomem failed - mv_sdio_card_to_host\n") ;
		return ret;
	}
//	*nb = (buf_len - 4);
	*nb = buf_len;

	if (*nb <= 0) {
		printk("Null packet recieved \n");
		return -5;
	}

	*type  = (payload[2] | (payload[3] << 8));
	
/*	if (*nb > npayload) {
		printk("*nb = %d  npayload = %d\n",*nb,npayload);
		*nb = npayload;
		return -5;
	} */

//	temp = *nb;
//	memmove(payload, (payload+4), temp);

	return *nb;
}

/*
 * Read from the special scratch 'port'.
 */

int mv_sdio_read_scratch(wlan_private *priv, u16 *dat)
{
	int	ret;
	u8	scr0;
	u8	scr1;
	u8	scr2;

	ret = sdio_read_ioreg(priv->wlan_dev.card, FN0, SCRATCH_0_REG, &scr0);
	if (ret < 0)
		return ret;
#if DEBUG2
	PRINTK("SCRATCH_0_REG = %x\n",scr0);
#endif

	ret = sdio_read_ioreg(priv->wlan_dev.card, FN0, SCRATCH_1_REG, &scr1);
	if (ret < 0)
		return ret;
#if DEBUG2
	PRINTK("SCRATCH_1_REG = %x\n",scr1);
#endif

	/* the SCRATCH_REG @ 0x8fc tells the cause for the Mac Event */
	ret = sdio_read_ioreg(priv->wlan_dev.card, FN0, 0x80fc, &scr2);
	if (ret < 0) { 
		PRINTK("Unable to read \n");
		return ret;
	}

	priv->adapter->EventCause = scr2;
	*dat = (((u16) scr1) << 8) | scr0;
	
	return 0;
}

/*
 * 	Get the CIS Table 
 */
int sbi_get_cis_info(wlan_private *priv)
{
	mmc_card_t	card = (mmc_card_t) priv->wlan_dev.card;
	wlan_adapter	*Adapter = priv->adapter;
	u8 		tupledata[255];
	char 		cisbuf[512];
	int 		ofs=0, i;
	u32 		ret = 0;

	ENTER();
	
	/* Read the Tuple Data */
  	for (i = 0; i < sizeof(tupledata); i++) {
		ret = sdio_read_ioreg(card, FN0, card->info.cisptr[FN0] + i,
								&tupledata[i]);
		if (ret < 0)
			return ret;
	}

	memset(cisbuf, 0x0, sizeof(cisbuf));
    	memcpy(cisbuf+ofs, tupledata, sizeof(cisbuf));
	
	/* Copy the CIS Table to Adapter */
	memset(Adapter->CisInfoBuf, 0x0, sizeof(cisbuf));
	memcpy(Adapter->CisInfoBuf, &cisbuf, sizeof(cisbuf));
	Adapter->CisInfoLen = sizeof(cisbuf);

	LEAVE();
	return 0;
}


/*
 * This function probes the SDIO card's CIS space to see if it is a
 * card that this driver supports and handles.
 */

int sbi_probe_card(void * card_p)
{
	int		ret = -ENODEV;
	mmc_card_t	card = (mmc_card_t) card_p;
#ifndef	SD8305
	u8		bic;
#endif
	ENTER();  

	if (!card)
		return 0;

	/* Check for MANFID */
	ENTER();

	ret = sdio_get_vendor_id(card);

	if (ret == 0x02df) {
		PRINTK("Marvell SDIO card detected!\n");
	} else {
		PRINTK("Ignoring a non-Marvell SDIO card...\n");
		return -ENODEV;
	}

	/* read Revision Register to get the hw revision number */
	if (sdio_read_ioreg(card, FN1, CARD_REVISION_REG, &card->chiprev) < 0) {
		printk("cannot read CARD_REVISION_REG\n");
	} else {
		PRINTK("revsion=0x%x\n",card->chiprev);
		switch(card->chiprev) {
#ifdef SD8385
		case 0x00:	/* SD8385-A0 */
			PRINTK("SD8385-A0 not supported\n");
			return -ENODEV;
		case 0x10:	/* SD8385-B0 */
		case 0x12:	/* SD8385-B1 */
		case 0x13:	/* SD8385-B2 */
		default:
			card->hw_tx_done = TRUE;
			card->block_size_512 = TRUE;
			card->async_int_mode = TRUE;

			/* enable async interrupt mode */
			sdio_read_ioreg(card, FN0, BUS_INTERFACE_CONTROL_REG, 
									&bic);
			sdio_write_ioreg(card, FN0, BUS_INTERFACE_CONTROL_REG, 
								bic | 0x20);
			break;
#endif
#ifdef SD8399
		default:
			card->hw_tx_done = TRUE;
			card->block_size_512 = TRUE;
			card->async_int_mode = TRUE;

			/* enable async interrupt mode */
			sdio_read_ioreg(card, FN0, BUS_INTERFACE_CONTROL_REG, 
									&bic);
			sdio_write_ioreg(card, FN0, BUS_INTERFACE_CONTROL_REG, 
								bic | 0x20);
			break;
#endif
#ifdef SD8388
		case 0x02:	/* SD8388-A1 */
		default:
			card->hw_tx_done = TRUE;
			card->block_size_512 = TRUE;
			card->async_int_mode = TRUE;

			/* enable async interrupt mode */
			sdio_read_ioreg(card, FN0, BUS_INTERFACE_CONTROL_REG, 
									&bic);
			sdio_write_ioreg(card, FN0, BUS_INTERFACE_CONTROL_REG, 
								bic | 0x20);
			break;
#endif
#ifdef SD8381
		case 0x05:	/* SD8381-Q0 */
		case 0x04:	/* SD8381-B3 */
		case 0x03:	/* SD8381-B2 */
		case 0x01:	/* SD8381-B1 */
		default:
			card->hw_tx_done = TRUE;
			card->block_size_512 = TRUE;
			card->async_int_mode = TRUE;

			/* enable async interrupt mode */

			sdio_read_ioreg(card, FN0, BUS_INTERFACE_CONTROL_REG, 
									&bic);
			sdio_write_ioreg(card, FN0, BUS_INTERFACE_CONTROL_REG, 
								bic | 0x20);
			break;
#endif
#ifdef SD8305
		case 0x00:
		default:
			card->hw_tx_done = FALSE;
			card->block_size_512 = FALSE;
			card->async_int_mode = FALSE;
			break;
#endif
		}
	}


	LEAVE();
	return 0;
}


#ifdef HELPER_IMAGE
static int sbi_download_wlan_fw_image(wlan_private *priv, const u8 *firmware, 
								int firmwarelen)
{
	u8	base0;
	u8	base1;
	int	ret;
	int	offset;
	int	fwblknow = 0;
	u8	fwbuf[FIRMWARE_TRANSFER_NBLOCK * 32 * 8];
	int	timeout = 5000;	// As per Hedley
	u16	len;
	int	txlen = 0;
	int	tx_len = 0;
#ifdef DEBUG_LEVEL0
	int	cnt = 0;
#endif
#ifdef FW_DOWNLOAD_SPEED
	unsigned long tv1,tv2;
#endif
	
	ENTER();

	PRINTK("WLAN_FW: Downloading firmware of size %d bytes\n", firmwarelen);
#ifdef FW_DOWNLOAD_SPEED
	tv1 = get_utimeofday();
#endif

	/* The host polls for the DN_LD_CARD_RDY and IO_READY bits */
	ret = mv_sdio_poll_card_status(priv,IO_READY|DN_LD_CARD_RDY);
	if (ret < 0) {
		printk("Firmware download died @ the end\n");
		ret = -1;
		goto done;
	}

	/* Wait initially for the first non-zero value */
	do {
		if((ret = sdio_read_ioreg(priv->wlan_dev.card, FN1, 
					HOST_F1_RD_BASE_0, &base0)) < 0) {
			if(timeout>4995 || timeout<5)
				printk("Dev BASE0 register read failed:"
						" base0=0x%04X(%d)\n",
						base0, base0);
			ret = -1;
			goto done;
		}
		if((ret = sdio_read_ioreg(priv->wlan_dev.card, FN1, 
					HOST_F1_RD_BASE_1, &base1)) < 0) {
			if(timeout>4995 || timeout<5)
				printk("Dev BASE1 register read failed:"
						" base1=0x%04X(%d)\n", 
						base1, base1);
			ret = -1;
			goto done;
		}
		len = (((u16) base1) << 8) | base0;
	} while (!len && --timeout);
		
	if (!timeout) {
		printk("Timeout waiting for initial non-zero length\n");
		ret = -1;
		goto done;
	}
	
	PRINTK(KERN_DEBUG "WLAN_FW: Len got from firmware = 0x%04X(%d)\n", 
								len, len);
	len &= ~B_BIT_0;

	/* Perform firmware data transfer */
	for (offset = 0; offset < firmwarelen; offset += txlen) {
		txlen = len;
		
		/* Set blocksize to transfer - checking for last block */
		if (firmwarelen - offset < txlen) {
			txlen = firmwarelen - offset;
		}
		PRINTK(KERN_DEBUG "WLAN_FW: offset=%d, txlen = 0x%04X(%d)\n", 
							offset,txlen,txlen);
	
 	    tx_len = (FIRMWARE_TRANSFER_NBLOCK * 32 * 8);
	    for (fwblknow = 0; fwblknow < txlen; fwblknow += tx_len) {
		/* The host polls for the DN_LD_CARD_RDY and IO_READY bits */
		ret = mv_sdio_poll_card_status(priv, IO_READY|DN_LD_CARD_RDY);
		if (ret < 0) {
			printk("Firmware download died @ %d\n", fwblknow);
			goto done;
		}
		if (txlen - fwblknow < tx_len)
			tx_len = txlen - fwblknow;
		PRINTK("WLAN_FW:     fwblknow=%d, tx_len = 0x%04X(%d)\n", 
							offset,tx_len,tx_len);
		/* Copy payload to buffer */
		memcpy(fwbuf, &firmware[offset+fwblknow], tx_len);

		/* Send data */
		ret = sdio_write_iomem(priv->wlan_dev.card, FN1,
			priv->wlan_dev.ioport, BLOCK_MODE, FIXED_ADDRESS,
				FIRMWARE_TRANSFER_NBLOCK*8, 32, fwbuf);

		if (ret < 0) {
			printk("IO error:transferring @ %d\n", offset+fwblknow);
			goto done;
		}
	    }
		do {
			mdelay(1);
			if((ret = sdio_read_ioreg(priv->wlan_dev.card, FN1, 
					HOST_F1_CARD_RDY, &base0)) < 0) {
				printk("Dev CARD_RDY register read failed:"
					" base0=0x%04X(%d)\n",base0,base0);
				ret = -1;
				goto done;
			}
			PRINTK("offset=0x%08X len=0x%04X: FN1,"
					"HOST_F1_CARD_RDY: 0x%04X\n",
					offset, txlen, base0);
		} while(!(base0 & 0x08) || !(base0 & 0x01));
		
		if((ret = sdio_read_ioreg(priv->wlan_dev.card, FN1, 
					HOST_F1_RD_BASE_0, &base0)) < 0) {
			printk("Dev BASE0 register read failed:"
					" base0=0x%04X(%d)\n",base0,base0);
			ret = -1;
			goto done;
		}
		if((ret = sdio_read_ioreg(priv->wlan_dev.card, FN1, 
					HOST_F1_RD_BASE_1, &base1)) < 0) {
			printk("Dev BASE1 register read failed:"
					" base1=0x%04X(%d)\n",base1,base1);
			ret = -1;
			goto done;
		}
		len = (((u16) base1) << 8) | base0;

		if (!len) {
			PRINTK("WLAN Firmware Download Over\n");
			break;
		}
		
		if (len & B_BIT_0) {
			printk("CRC32 Error indicated by the helper:"
					" len=0x%04X(%d)\n",len,len);
			len &= ~B_BIT_0;
			/* Setting this to 0 to resend from same offset */
			txlen = 0;
		} else {
			PRINTK("%d: %d,%d bytes block of firmware downloaded\n",
							++cnt, offset, txlen);
		}
  	}

	printk("Firmware Image of Size %d bytes downloaded\n", firmwarelen);
  
	ret = 0;
done:
#ifdef FW_DOWNLOAD_SPEED
	tv2 = get_utimeofday();
	printk("firmware: %ld.%03ld.%03ld ", tv1 / 1000000, 
						(tv1 % 1000000) / 1000,
						tv1 % 1000);
	printk(" -> %ld.%03ld.%03ld ", tv2 / 1000000, 
						(tv2 % 1000000) / 1000,
						tv2%1000);
	tv2 -= tv1;
	printk(" == %ld.%03ld.%03ld\n", tv2 / 1000000,
						(tv2 % 1000000) / 1000,
						tv2 % 1000);
#endif
	LEAVE();
	return ret;
}

int sbi_download_wlan_fw(wlan_private *priv)
{
#ifdef READ_HELPER_FW
	wlan_adapter	*Adapter = priv->adapter;
	if ( Adapter->fmimage != NULL ) {
		return sbi_download_wlan_fw_image(priv,
				Adapter->fmimage, Adapter->fmimage_len);
	}
	else
#endif
	{
		printk("No external firmware image\n");
		return -1;
	}
}
#endif

static int sbi_prog_firmware_image(wlan_private *priv, const u8 *firmware, 
								int firmwarelen)
{
	int	ret;
	u16	firmwarestat;
	int	tries;
	u8	fwbuf[FIRMWARE_TRANSFER_NBLOCK * SD_BLOCK_SIZE];
	int	fwblknow;
	u32	tx_len;
#ifdef FW_DOWNLOAD_SPEED
	unsigned long tv1,tv2;
#endif

	ENTER();

	/* Check if the firmware is already downloaded */
	if ((ret = mv_sdio_read_scratch(priv, &firmwarestat)) < 0) {
		printk("read scratch returned <0\n");
		goto done;
	}

	if (firmwarestat == FIRMWARE_READY) {
		printk("Firmware already downloaded!\n");
		/* TODO: We should be returning success over here */
		ret = 0;
		goto done;
  	}

	printk("Downloading "
#ifdef HELPER_IMAGE
		"helper"
#else
		"firmware"
#endif
		" image (%d bytes), block size %d bytes ", firmwarelen, 
								SD_BLOCK_SIZE);
#ifdef FW_DOWNLOAD_SPEED
	tv1 = get_utimeofday();
#endif
	/* Perform firmware data transfer */
	tx_len = (FIRMWARE_TRANSFER_NBLOCK * SD_BLOCK_SIZE) - 4;
	for (fwblknow = 0; fwblknow < firmwarelen; fwblknow += tx_len) {

		/* The host polls for the DN_LD_CARD_RDY and IO_READY bits */
		ret = mv_sdio_poll_card_status(priv, IO_READY|DN_LD_CARD_RDY);
		if (ret < 0) {
			printk("Firmware download died @ %d\n", fwblknow);
			goto done;
		}

		/* Set blocksize to transfer - checking for last block */
		if (firmwarelen - fwblknow < tx_len)
			tx_len = firmwarelen - fwblknow;

		fwbuf[0] = ((tx_len & 0x000000ff) >>  0);  /* Little-endian */
		fwbuf[1] = ((tx_len & 0x0000ff00) >>  8);
		fwbuf[2] = ((tx_len & 0x00ff0000) >> 16);
		fwbuf[3] = ((tx_len & 0xff000000) >> 24);

		/* Copy payload to buffer */
		memcpy(&fwbuf[4], &firmware[fwblknow], tx_len);

		PRINTK(".");

		/* Send data */
		ret = sdio_write_iomem(priv->wlan_dev.card, FN1,
					priv->wlan_dev.ioport, BLOCK_MODE, 
					FIXED_ADDRESS, FIRMWARE_TRANSFER_NBLOCK,
					SD_BLOCK_SIZE, fwbuf);

		if (ret < 0) {
			printk("IO error: transferring block @ %d\n", fwblknow);
			goto done;
		}
  	}

	printk("\ndone (%d/%d bytes)\n", fwblknow, firmwarelen);
#ifdef FW_DOWNLOAD_SPEED
	tv2 = get_utimeofday();
	printk("helper: %ld.%03ld.%03ld ",tv1 / 1000000, (tv1 % 1000000) / 1000,
								tv1 % 1000);
	printk(" -> %ld.%03ld.%03ld ", tv2 / 1000000, (tv2 % 1000000)/1000,
								tv2 % 1000);
	tv2 -= tv1;
	printk(" == %ld.%03ld.%03ld\n", tv2 / 1000000, (tv2 % 1000000) / 1000,
								tv2 % 1000);
#endif
  
	/* Write last EOF data */
	PRINTK("Transferring EOF block\n");
	memset(fwbuf, 0x0, sizeof(fwbuf));
	ret = sdio_write_iomem((mmc_card_t) priv->wlan_dev.card, FN1, 
				priv->wlan_dev.ioport, BLOCK_MODE, 
				FIXED_ADDRESS, 1, SD_BLOCK_SIZE, fwbuf);

	if (ret < 0) {
		printk("IO error in writing EOF firmware block\n");
		goto done;
	}

	/* Wait for firmware initialization event */
	for (tries = 0; tries < MAX_FIRMWARE_POLL_TRIES; tries++)
		mdelay(10);

	ret = 0;

done:
	return ret;
}

int sbi_prog_firmware(wlan_private *priv)
{
#ifdef READ_HELPER_FW
	wlan_adapter	*Adapter = priv->adapter;
	if ( Adapter->fmimage != NULL ) {
		return sbi_prog_firmware_image(priv,
				Adapter->fmimage, Adapter->fmimage_len);
	}
	else
#endif
	{
		printk("no external firmware image!!!\n");
		return -1;
	}
}

int sbi_verify_fw_download(wlan_private *priv)
{
	int	ret;
	u16	firmwarestat;
	int	tries;
	u8	rsr;

	/* Wait for firmware initialization event */
	for (tries = 0; tries < MAX_FIRMWARE_POLL_TRIES; tries++) {
		if ((ret = mv_sdio_read_scratch(priv, &firmwarestat)) < 0)
			continue;

		if (firmwarestat == FIRMWARE_READY) {
			ret = 0;
			printk("Firmware successfully downloaded\n");
			break;
		} else {
			mdelay(10);
			ret = -ETIMEDOUT;
		}
	}

	if (ret < 0) {
		printk("Timeout waiting for firmware to become active\n");
		goto done;
	}

	ret = sdio_read_ioreg(priv->wlan_dev.card, FN1, HOST_INT_RSR_REG, &rsr);
	if (ret < 0) {
		printk("sdio_read_ioreg: reading INT RSR register failed\n");
		return -1;
	} else
		PRINTK("sdio_read_ioreg: RSR register 0x%x\n",rsr);
	
	ret = 0;
done:
	return ret;
}

#ifdef HELPER_IMAGE
int sbi_prog_helper(wlan_private *priv)
{
#ifdef READ_HELPER_FW
	wlan_adapter	*Adapter = priv->adapter;
	if ( Adapter->helper != NULL ) {
		return sbi_prog_firmware_image(priv, 
				Adapter->helper, Adapter->helper_len);
	}
	else
#endif
	{
		printk("no external helper image\n");
		return -1;
	}
}
#endif

#ifdef DEEP_SLEEP

extern int start_bus_clock(mmc_controller_t);
extern int stop_bus_clock_2(mmc_controller_t);

inline int sbi_enter_deep_sleep(wlan_private *priv)
{
	int ret;

	sdio_write_ioreg(priv->wlan_dev.card, FN1, CONFIGURATION_REG, 0);
	mdelay(2);
	ret = sdio_write_ioreg(priv->wlan_dev.card, FN1, CONFIGURATION_REG, 
							HOST_POWER_DOWN);

	stop_bus_clock_2(((mmc_card_t)((priv->wlan_dev).card))->ctrlr);
	mdelay(2);

	return ret;
}

inline int sbi_exit_deep_sleep(wlan_private *priv)
{
	int ret = 0;

	printk("Trying to wakeup device... Conn=%d IntC=%d PS_Mode=%d PS_State=%d\n",
		priv->adapter->MediaConnectStatus, priv->adapter->IntCounter,
		priv->adapter->PSMode, priv->adapter->PSState);			
	
	start_bus_clock(((mmc_card_t)((priv->wlan_dev).card))->ctrlr);

#ifdef FW_WAKEUP_METHOD
	if (priv->adapter->fwWakeupMethod == WAKEUP_FW_THRU_GPIO) {
		GPIO_PORT_TO_LOW();
	}
	else // SDIO method 
#endif
	ret = sdio_write_ioreg(priv->wlan_dev.card, FN1, CONFIGURATION_REG, 
								HOST_POWER_UP);
#ifdef FW_WAKEUP_TIME
	wt_pwrup_sent = get_utimeofday();
#endif

	return ret;
}

inline int sbi_reset_deepsleep_wakeup(wlan_private *priv)
{
	ENTER();

	int ret = 0;

#ifdef FW_WAKEUP_METHOD
	if (priv->adapter->fwWakeupMethod == WAKEUP_FW_THRU_GPIO) {
		GPIO_PORT_TO_HIGH();
	}
	else // SDIO method 
#endif
	ret = sdio_write_ioreg(priv->wlan_dev.card, FN1, CONFIGURATION_REG, 0);

	LEAVE();

	return ret;
}
#endif  /* DEEP_SLEEP */

#ifdef CONFIG_PM
inline int sbi_suspend(wlan_private *priv)
{
	int ret;
	
	ENTER();

	ret = sdio_suspend(priv->wlan_dev.card);

	LEAVE();
	return ret;
}

inline int sbi_resume(wlan_private *priv)
{
	int ret;
	
	ENTER();

	ret = sdio_resume(priv->wlan_dev.card);

	LEAVE();
	return ret;
}
#endif

