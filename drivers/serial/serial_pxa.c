/*
 *  linux/drivers/serial/serial_pxa.c
 *
 *  Driver for PXA26x HWUART serial port.
 *
 *  Based on drivers/serial/serial_sa1100.c, by Deep Blue Solutions Ltd.
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *
 */
#include <linux/config.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/major.h>
#include <linux/string.h>
#include <linux/fcntl.h>
#include <linux/ptrace.h>
#include <linux/ioport.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/circ_buf.h>
#include <linux/serial.h>
#include <linux/console.h>
#include <linux/pci.h>
#include <linux/sysrq.h>
#include <linux/delay.h>

#include <asm/dma.h>
#include <asm/system.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/uaccess.h>
#include <asm/bitops.h>
#include <asm/hardware.h>
#include <asm/serial.h>
#include <linux/serial_core.h>

#if defined(CONFIG_SERIAL_PXA_CONSOLE) && defined(CONFIG_MAGIC_SYSRQ)
#define SUPPORT_SYSRQ
#endif

#define FOR_MOT_ONLY
#define UART_DMA_ENABLED
//#undef UART_DMA_ENABLED
//#define ENABLE_HWFC
//#define LED_DEBUG
//#define PRINTK_DEBUG
//#define DEBUG
#undef DEBUG

#ifdef FOR_MOT_ONLY
#define SERIAL_PXA_MAJOR	4
#define CALLOUT_PXA_MAJOR	5
#define MINOR_START		64
#define HWUART_NORMAL_NAME_FS	"ttyS%d"
#define HWUART_NORMAL_NAME	"ttyS"
#else
#define SERIAL_PXA_MAJOR	206
#define CALLOUT_PXA_MAJOR	207
#define MINOR_START		0
#define HWUART_NORMAL_NAME_FS	"hwuart%d"
#define HWUART_NORMAL_NAME	"hwuart"
#endif

#ifndef MIN
#define MIN(a,b)	((a) < (b) ? (a) : (b))
#endif

#ifdef PRINTK_DEBUG
#define HDEBUG(s...)  printk("<0>[HDEBUG] Line %4i : %s\n",__LINE__ , ##s)
#else
#define HDEBUG(s...)
#endif

//#define CONFIG_SERIAL_PXA
#define NR_PORTS		1	/* only HWUART */
#define PXA_ISR_PASS_LIMIT	256	/* to avoid long time ISR */
#define FIFO_THRESHHOLD		32	/* threshhold for rx fifo */
#define UART_PORT_SIZE		0x30	/* This is the size of our serial port register set.*/

static struct tty_driver normal, callout;
static struct tty_struct *pxa_table[NR_PORTS];
static struct termios *pxa_termios[NR_PORTS], *pxa_termios_locked[NR_PORTS];
static struct uart_driver 	pxa_reg;
#ifdef SUPPORT_SYSRQ
static struct console pxa_console;
#endif

#ifdef UART_DMA_ENABLED
#define DMA_BUFF_SIZE		2048   	/* limited by UART_XMIT_SIZE (1024 bytes by default) */
#define DMA_RX_SIZE		512	/* limited by TTY_FLIPBUF_SIZE(512 bytes by default) */
static	unsigned char		*dma_rx_buff;
static	unsigned char		*dma_tx_buff;
static	dma_addr_t		dma_rx_buff_phy;
static	dma_addr_t		dma_tx_buff_phy;
static	int			txdma;
static	int			rxdma;
static  int			tx_num;
static DECLARE_MUTEX(sem_dma_tx);

static void pxa_hwuart_dma_rx_start(void)
{
	unsigned long flags;
	local_irq_save(flags);
//	HWIER |= IER_DMAE;
//	STIER |= IER_DMAE;
	consistent_sync(dma_rx_buff, DMA_BUFF_SIZE, PCI_DMA_FROMDEVICE);
	dma_rx_buff_phy = virt_to_bus(dma_rx_buff);
	DCSR(rxdma)  = DCSR_NODESC;
//	DSADR(rxdma) = __PREG(HWRBR);
	DSADR(rxdma) = __PREG(STRBR);	
	DTADR(rxdma) = dma_rx_buff_phy;
	DCMD(rxdma) = DCMD_INCTRGADDR | DCMD_FLOWSRC | DCMD_ENDIRQEN | DCMD_WIDTH1 | DCMD_BURST16 | DMA_RX_SIZE;
	DCSR(rxdma) |= DCSR_RUN;
	local_irq_restore(flags);
}
#endif

#ifdef UART_DMA_ENABLED
static void pxa_hwuart_dma_rx_irq(int channel, void *data, struct pt_regs *regs)
{
	int dcsr;
	struct uart_info* info=pxa_reg.state->info;
	struct tty_struct *tty = info->tty;
	struct uart_port *port = info->port;

//	disable_irq(IRQ_HWUART);
	disable_irq(IRQ_STUART);	
	dcsr = DCSR(channel);
	DCSR(channel) &= ~DCSR_RUN;
	DCSR(channel) |= DCSR_STARTINTR|DCSR_ENDINTR|DCSR_BUSERR;
	
	if ( dcsr & DCSR_ENDINTR )  {
//		printk("D_r\n");
		int len;
		len = MIN((TTY_FLIPBUF_SIZE - tty->flip.count), DMA_RX_SIZE);
		if (tty->flip.count >= TTY_FLIPBUF_SIZE) {
			printk("Overflow!\n");
			tty_flip_buffer_push(tty);
			goto out;
		}
#if 0
		memcpy(tty->flip.char_buf_ptr, dma_rx_buff, DMA_RX_SIZE);
		memset(tty->flip.flag_buf_ptr,TTY_NORMAL, DMA_RX_SIZE);
		tty->flip.char_buf_ptr += DMA_RX_SIZE;
		tty->flip.flag_buf_ptr += DMA_RX_SIZE;
		port->icount.rx +=DMA_RX_SIZE;
		tty->flip.count +=DMA_RX_SIZE;
#ifdef LED_DEBUG
		HEXLEDS_BASE += DMA_RX_SIZE;
#endif
#endif
		memcpy(tty->flip.char_buf_ptr, dma_rx_buff, len);
		memset(tty->flip.flag_buf_ptr,TTY_NORMAL, len);
		tty->flip.char_buf_ptr += len;
		tty->flip.flag_buf_ptr += len;
		port->icount.rx +=len;
		tty->flip.count +=len;
		tty_flip_buffer_push(tty);
	} else {
		printk(KERN_DEBUG "serial_pxa: rx dma bus error.\n");
	}
#ifdef DEBUG
	printk("rx interrupt handler\n");
#endif
out:
	pxa_hwuart_dma_rx_start();
//	enable_irq(IRQ_HWUART);
	enable_irq(IRQ_STUART);
}

static void pxa_hwuart_dma_tx_irq(int channel, void *data, struct pt_regs *regs)
{
	int dcsr;
	struct uart_info* info=pxa_reg.state->info;

//	disable_irq(IRQ_HWUART);
	disable_irq(IRQ_STUART);
	dcsr = DCSR(channel);
	DCSR(channel) &= ~DCSR_RUN;
	DCSR(channel) |= DCSR_STARTINTR|DCSR_ENDINTR|DCSR_BUSERR;

	if ( dcsr & DCSR_ENDINTR )  {
//		printk("D_t\n");
//		while (!( HWLSR & LSR_TEMT ))  {
//		while (!( STLSR & LSR_TEMT ))  {
//			rmb();
//		}
		info->xmit.tail = (info->xmit.tail + tx_num) & (UART_XMIT_SIZE - 1);
//		HWIER &= ~IER_DMAE;
//		STIER &= ~IER_DMAE;
		up(&sem_dma_tx);
	} else {
		printk(KERN_DEBUG "serial_pxa: tx dma bus error.\n");
	}
#ifdef DEBUG
	printk("tx interrupt handler\n");
#endif
//	pxa_hwuart_dma_rx_start();
//	enable_irq(IRQ_HWUART);
	enable_irq(IRQ_STUART);
}
#endif

inline static int pxa_open(struct uart_port * port, struct uart_info * info)
{

	/* This enables the HWUART */
	disable_irq(IRQ_STUART);

#ifdef UART_DMA_ENABLED
//	rxdma = pxa_request_dma("HWUART_RX",DMA_PRIO_LOW, pxa_hwuart_dma_rx_irq, 0);
	rxdma = pxa_request_dma("HWUART_RX",DMA_PRIO_LOW, pxa_hwuart_dma_rx_irq, info);
	if (rxdma < 0)
		goto err_rx_dma;

//	txdma = pxa_request_dma("HWUART_TX",DMA_PRIO_LOW, pxa_hwuart_dma_tx_irq, 0);
	txdma = pxa_request_dma("HWUART_TX",DMA_PRIO_LOW, pxa_hwuart_dma_tx_irq, info);
	if (txdma < 0)
		goto err_tx_dma;

	if (! ( dma_rx_buff = kmalloc(DMA_BUFF_SIZE, GFP_KERNEL | GFP_DMA) ) ) {
		goto err_dma_rx_buff;
	}
	if (! ( dma_tx_buff = kmalloc(DMA_BUFF_SIZE, GFP_KERNEL | GFP_DMA) ) ) {
		goto err_dma_tx_buff;
	}

	DRCMR19 = rxdma | DRCMR_MAPVLD;
	DRCMR20 = txdma | DRCMR_MAPVLD;
	HDEBUG("HWUART DMA support enabled!");
//for debug
/*
	 printk("rx dma : %d, buff %x\n",rxdma,dma_rx_buff); 
	 printk("tx dma : %d, buff %x\n",txdma,dma_tx_buff); 
*/
#endif

//	CKEN |= CKEN4_HWUART;
	CKEN |= CKEN5_STUART;	

	STIER |= IER_UUE;
	
	STFCR = FCR_RESETTF | FCR_RESETRF | FCR_TRFIFOE;
	switch ( FIFO_THRESHHOLD ) {
		case 1: STFCR |= FCR_ITL_1; break;
		case 8: STFCR |= FCR_ITL_8; break;
		case 16: STFCR |= FCR_ITL_16; break;
		case 32: STFCR |= FCR_ITL_32; break;
		default: STFCR |= FCR_ITL_8;
	}

	STLCR = LCR_WLS_8;

#ifdef UART_DMA_ENABLED
//	HWIER |= IER_RTOIE | IER_RLSE ;
	STIER |= IER_RTOIE | IER_RLSE ;
#else
//	HWIER |= IER_RTOIE | IER_TIE | IER_RLSE | IER_RAVIE;
	STIER |= IER_RTOIE | IER_TIE | IER_RLSE | IER_RAVIE;
#endif

#ifdef ENABLE_HWFC
	HWMCR = MCR_OUT2 | MCR_AFE | MCR_RTS; /* Hardware flow control enabled */
	HDEBUG("Hardware Flow Control Enabled!");
#else
//	HWMCR = MCR_OUT2;
	STMCR = MCR_OUT2;
	HDEBUG("Hardware Flow Control Disabled!");
#endif
//	enable_irq(IRQ_HWUART);
	enable_irq(IRQ_STUART);

	return 0;

#ifdef UART_DMA_ENABLED
err_dma_tx_buff:
	kfree(dma_rx_buff);
err_dma_rx_buff:
	pxa_free_dma(txdma);
err_tx_dma:
	pxa_free_dma(rxdma);
err_rx_dma:
	return 1;
#endif


}

inline static void pxa_close(struct uart_port * port, struct uart_info * info)
{
//	CKEN &= ~CKEN4_HWUART;
	CKEN &= ~CKEN5_STUART;

#ifdef UART_DMA_ENABLED
	/* deinit DMA support -- begin */
	pxa_free_dma(rxdma);
	pxa_free_dma(txdma);
	if (dma_rx_buff) kfree(dma_rx_buff);
	if (dma_tx_buff) kfree(dma_tx_buff);
	/* deinit DMA support -- end */
#endif
	HDEBUG("HWUART power off");
}

/*
 * interrupts disabled on entry
 */
static void pxa_stop_tx(struct uart_port *port, u_int from_tty)
{
#ifdef UART_DMA_ENABLED
	unsigned long flags;
	HDEBUG("pxa_stop_tx");
	local_irq_save(flags);
	DCSR(txdma) &= ~DCSR_RUN;
//	HWIER &= ~IER_DMAE;
//	STIER &= ~IER_DMAE;
	up(&sem_dma_tx);
	local_irq_restore(flags);
#else
//	HWIER &= ~IER_TIE;
	STIER &= ~IER_TIE;
	port->read_status_mask &= ~LSR_TDRQ;
#endif
}

/*
 * Interrupts enabled
 */
static void pxa_stop_rx(struct uart_port *port)
{
#ifdef UART_DMA_ENABLED
	DCSR(rxdma) &= ~DCSR_RUN;
//	HWIER &= ~IER_DMAE;
//	STIER &= ~IER_DMAE;
#else
//	HWIER &= ~IER_RAVIE;
	STIER &= ~IER_RAVIE;
#endif
}


/*
 * interrupts may not be disabled on entry
 */
static void pxa_start_tx(struct uart_port *port, u_int nonempty, u_int from_tty)
{

#ifdef UART_DMA_ENABLED
	struct uart_info* info=pxa_reg.state->info;

	if (nonempty) {
		unsigned long flags;

		down(&sem_dma_tx);
		tx_num = CIRC_CNT(info->xmit.head, info->xmit.tail, UART_XMIT_SIZE);
		port->icount.tx += tx_num;
		if ( tx_num > 0 ) {
			local_irq_save(flags);
//			pxa_stop_rx(port);

			if (info->xmit.tail > info->xmit.head) {
				memcpy(dma_tx_buff, info->xmit.buf+info->xmit.tail, UART_XMIT_SIZE - info->xmit.tail);
				memcpy(dma_tx_buff + (UART_XMIT_SIZE - info->xmit.tail), info->xmit.buf,info->xmit.head);
			} else {
				memcpy(dma_tx_buff, info->xmit.buf+info->xmit.tail, tx_num);
			}

//			HWIER |= IER_DMAE;
//			STIER |= IER_DMAE;
			consistent_sync(dma_tx_buff, DMA_BUFF_SIZE, PCI_DMA_TODEVICE);
			dma_tx_buff_phy = virt_to_bus(dma_tx_buff);
			DCSR(txdma)  = DCSR_NODESC;
			DSADR(txdma) = dma_tx_buff_phy;
//			DTADR(txdma) = __PREG(HWTHR);
			DTADR(txdma) = __PREG(STTHR);
			DCMD(txdma) = DCMD_INCSRCADDR | DCMD_FLOWTRG |  DCMD_ENDIRQEN | DCMD_WIDTH1 | DCMD_BURST32 | tx_num;
			DCSR(txdma) |= DCSR_RUN;
			local_irq_restore(flags);
		} else {
			up(&sem_dma_tx);
		}
	}
#else
	if (nonempty) {
		unsigned long flags;
		local_irq_save(flags);
		port->read_status_mask |= LSR_TDRQ;
//		HWIER |= IER_TIE;
		STIER |= IER_TIE;
		local_irq_restore(flags);
	}
#endif
}

/*
 * No modem control lines
 */
static void pxa_enable_ms(struct uart_port *port)
{
}

static void pxa_rx_chars1(struct uart_info *info, unsigned int status, struct pt_regs *regs)
{
	struct tty_struct *tty = info->tty;
	unsigned int ch, flg, ignored = 0;
	struct uart_port *port = info->port;

#ifdef UART_DMA_ENABLED
	unsigned int len;

	len = DTADR(rxdma) - dma_rx_buff_phy;
	len = MIN((TTY_FLIPBUF_SIZE - tty->flip.count), len);
	if (tty->flip.count >= TTY_FLIPBUF_SIZE) {
		printk("Overflow!!\n");
		goto out;
	}
	
	if (len>0) {
		memcpy(tty->flip.char_buf_ptr, dma_rx_buff, len);
		memset(tty->flip.flag_buf_ptr,TTY_NORMAL, len);
		tty->flip.char_buf_ptr += len;
		tty->flip.flag_buf_ptr += len;
		port->icount.rx +=len;
		tty->flip.count +=len;
#ifdef LED_DEBUG
		HEXLEDS_BASE += len;
#endif
		tty_flip_buffer_push(tty);
	}
#endif

	do {	
		ch = STRBR;
		if (tty->flip.count >= TTY_FLIPBUF_SIZE) {
			printk(KERN_WARNING "TTY_DONTFLIP set!!\n");	
			goto ignore_char;
//			return;
		}
		port->icount.rx++;
		flg = TTY_NORMAL;
#if 0		
		printk("%2x\n", status);
#endif	
		if (status & ( LSR_OE | LSR_PE | LSR_FE | LSR_BI ) ) {
#if 0
			printk("\n%2x", status);
#endif
			goto handle_error;
		}

		if (uart_handle_sysrq_char(info, ch, regs))
			goto ignore_char;

	error_return:
		*tty->flip.flag_buf_ptr++ = flg;
		*tty->flip.char_buf_ptr++ = ch;
		tty->flip.count++;
	ignore_char:
//		status = HWLSR;
		status = STLSR;
	} while (status & LSR_FIFOE); 
//	} while ((status & LSR_DR) && --max_count > 0); 
out:
	tty_flip_buffer_push(tty);
	return;

handle_error:
	if (status & LSR_BI){
		status &= ~( LSR_FE | LSR_PE );
		port->icount.brk++;
	} else if (status & LSR_PE)
		port->icount.parity++;
	else if (status & LSR_FE)
		port->icount.frame++;
	if (status & LSR_OE) {
		printk("Overrun!!\n");
		port->icount.overrun++;
	}

	if (status & port->ignore_status_mask) {
		if (++ignored > 100)
			goto out;
		goto ignore_char;
	}

	status &= port->read_status_mask;

	if (status & LSR_BI)
		flg = TTY_BREAK;
	else if (status & LSR_PE)
		flg = TTY_PARITY;
	else if (status & LSR_FE)
		flg = TTY_FRAME;

	if (status & LSR_OE) {
		 /* overrun does *not* affect the character  we read from the FIFO */
		*tty->flip.flag_buf_ptr++ = flg;
		*tty->flip.char_buf_ptr++ = ch;
		tty->flip.count++;
		if (tty->flip.count >= TTY_FLIPBUF_SIZE)
			goto out;
		ch = 0;
		flg = TTY_OVERRUN;
	}
#ifdef SUPPORT_SYSRQ
	info->sysrq = 0;
#endif
	goto error_return;
}
static void rx_chars(struct uart_info *info, unsigned int status, struct pt_regs *regs)
{
	struct tty_struct *tty = info->tty;
	unsigned int ch, flg, ignored = 0;
	struct uart_port *port = info->port;
	int max_count = port->fifosize;

 	STIER &= ~ IER_RTOIE;
	STIER |= IER_RAVIE; 
#ifdef UART_DMA_ENABLED
	unsigned int len;

	len = DTADR(rxdma) - dma_rx_buff_phy;
	len = MIN((TTY_FLIPBUF_SIZE - tty->flip.count), len);
	if (tty->flip.count >= TTY_FLIPBUF_SIZE) {
		printk("Overflow!!!!\n");
                goto out;
	}               
	 
	if (len>0) {
		memcpy(tty->flip.char_buf_ptr, dma_rx_buff, len);
		memset(tty->flip.flag_buf_ptr,TTY_NORMAL, len);
		tty->flip.char_buf_ptr += len;
		tty->flip.flag_buf_ptr += len;
		port->icount.rx +=len;
		tty->flip.count +=len;
#ifdef LED_DEBUG
		HEXLEDS_BASE += len;
#endif
		tty_flip_buffer_push(tty);
		pxa_hwuart_dma_rx_start();
	}
#endif
	
	do {	
		ch = STRBR;
		if (tty->flip.count >= TTY_FLIPBUF_SIZE) {
			printk(KERN_WARNING "TTY_DONTFLIP set!!\n");	
			goto ignore_char;
//			return;
		}
		port->icount.rx++;
		flg = TTY_NORMAL;
#if 0		
		printk("%2x\n", status);
#endif	
		if (status & ( LSR_OE | LSR_PE | LSR_FE | LSR_BI ) ) {
#if 0
			printk("\n%2x", status);
#endif
			goto handle_error;
		}

		if (uart_handle_sysrq_char(info, ch, regs))
			goto ignore_char;

	error_return:
		*tty->flip.flag_buf_ptr++ = flg;
		*tty->flip.char_buf_ptr++ = ch;
		tty->flip.count++;
	ignore_char:
//		status = HWLSR;
		status = STLSR;
//	} while (status & LSR_FIFOE); 
	} while ((status & LSR_DR) && (max_count-- > 0)); 
out:
	STIER &= ~IER_RAVIE;
	STIER |= IER_RTOIE;
	tty_flip_buffer_push(tty);
	return;

handle_error:
	if (status & LSR_BI){
		status &= ~( LSR_FE | LSR_PE );
		port->icount.brk++;
	} else if (status & LSR_PE)
		port->icount.parity++;
	else if (status & LSR_FE)
		port->icount.frame++;
	if (status & LSR_OE) {
		printk("Overrun!!\n");
		port->icount.overrun++;
	}

	if (status & port->ignore_status_mask) {
		if (++ignored > 100)
			goto out;
		goto ignore_char;
	}

	status &= port->read_status_mask;

	if (status & LSR_BI)
		flg = TTY_BREAK;
	else if (status & LSR_PE)
		flg = TTY_PARITY;
	else if (status & LSR_FE)
		flg = TTY_FRAME;

	if (status & LSR_OE) {
		 /* overrun does *not* affect the character  we read from the FIFO */
		*tty->flip.flag_buf_ptr++ = flg;
		*tty->flip.char_buf_ptr++ = ch;
		tty->flip.count++;
		if (tty->flip.count >= TTY_FLIPBUF_SIZE)
			goto out;
		ch = 0;
		flg = TTY_OVERRUN;
	}
#ifdef SUPPORT_SYSRQ
	info->sysrq = 0;
#endif
	goto error_return;
}
static void pxa_rx_chars(struct uart_info *info, struct pt_regs *regs)
{
	struct tty_struct *tty = info->tty;
	unsigned int status, ch, flg, ignored = 0;
	struct uart_port *port = info->port;
	int max_count = port->fifosize;

#ifdef UART_DMA_ENABLED
	unsigned int len;

	len = DTADR(rxdma) - dma_rx_buff_phy;
	len = MIN((TTY_FLIPBUF_SIZE - tty->flip.count), len);
	if (tty->flip.count >= TTY_FLIPBUF_SIZE) {
		printk("Overflow!!!!\n");
                goto out;
	}               
	 
	if (len>0) {
		memcpy(tty->flip.char_buf_ptr, dma_rx_buff, len);
		memset(tty->flip.flag_buf_ptr,TTY_NORMAL, len);
		tty->flip.char_buf_ptr += len;
		tty->flip.flag_buf_ptr += len;
		port->icount.rx +=len;
		tty->flip.count +=len;
#ifdef LED_DEBUG
		HEXLEDS_BASE += len;
#endif
		tty_flip_buffer_push(tty);
	}
#endif

//	status = HWLSR;
	status = STLSR;
	while ((status & LSR_DR) && max_count-- > 0){
//		ch = HWRBR;
		ch = STRBR;
		if (tty->flip.count >= TTY_FLIPBUF_SIZE) {
			printk(KERN_WARNING "TTY_DONTFLIP set!!!!\n");	
			goto ignore_char;
//			return;
		}
		port->icount.rx++;
#ifdef LED_DEBUG
		HEXLEDS_BASE ++;
#endif
/*
		//test begin
		{
		static unsigned int prev_ch=0;
		static long count=0;
		count++;
		printk("0x%2x ",ch);
		if(count%10 == 0)
			printk("count=%d\n", count);
//		{
//			printk("\n");
//			printk("0x%2x ",ch);
//		}
		HEXLEDS_BASE=count;
		if (prev_ch == ch) {
			HDEBUG("HWUART RECV ERROR: prev_ch=%d, ch=%d",prev_ch,ch);
		}
		prev_ch = ch;
		}
		//test end
*/
		flg = TTY_NORMAL;

		if (status & ( LSR_OE | LSR_PE | LSR_FE | LSR_BI ) ) {
//			printk("\n%2x\n", status);
			goto handle_error;
		}

		if (uart_handle_sysrq_char(info, ch, regs))
			goto ignore_char;

	error_return:
		*tty->flip.flag_buf_ptr++ = flg;
		*tty->flip.char_buf_ptr++ = ch;
		tty->flip.count++;
	ignore_char:
//		status = HWLSR;
		status = STLSR;
	}
out:
	tty_flip_buffer_push(tty);
	return;

handle_error:
	if (status & LSR_BI){
		status &= ~( LSR_FE | LSR_PE );
		port->icount.brk++;
	} else if (status & LSR_PE)
		port->icount.parity++;
	else if (status & LSR_FE)
		port->icount.frame++;
	if (status & LSR_OE) {
		printk("Overrun!!!!\n");
		port->icount.overrun++;
	}
	
	if (status & port->ignore_status_mask) {
		if (++ignored > 100)
			goto out;
		goto ignore_char;
	}

	status &= port->read_status_mask;

	if (status & LSR_BI)
		flg = TTY_BREAK;
	else if (status & LSR_PE)
		flg = TTY_PARITY;
	else if (status & LSR_FE)
		flg = TTY_FRAME;

	if (status & LSR_OE) {
		 /* overrun does *not* affect the character  we read from the FIFO */
		*tty->flip.flag_buf_ptr++ = flg;
		*tty->flip.char_buf_ptr++ = ch;
		tty->flip.count++;
		if (tty->flip.count >= TTY_FLIPBUF_SIZE)
			goto out;
		ch = 0;
		flg = TTY_OVERRUN;
	}
#ifdef SUPPORT_SYSRQ
	info->sysrq = 0;
#endif
	goto error_return;

}


static void pxa_tx_chars(struct uart_info *info)
{

	struct uart_port *port = info->port;

	if (port->x_char) {
//		HWTHR = port->x_char;
		STTHR = port->x_char;
		port->icount.tx++;
		port->x_char = 0;
		return;
	}
	if (info->xmit.head == info->xmit.tail
	    || info->tty->stopped
	    || info->tty->hw_stopped) {
		pxa_stop_tx(info->port, 0);
		return;
	}

	/* Tried using FIFO (not checking TNF) for fifo fill:
	 still had the '4 bytes repeated' problem. */

//	while (HWLSR & LSR_TDRQ) {
	while (STLSR & LSR_TDRQ) {
//		HWTHR = info->xmit.buf[info->xmit.tail];
		STTHR = info->xmit.buf[info->xmit.tail];

		info->xmit.tail = (info->xmit.tail + 1) & (UART_XMIT_SIZE - 1);
		port->icount.tx++;
		if (info->xmit.head == info->xmit.tail)
			break;
	}

	if (CIRC_CNT(info->xmit.head, info->xmit.tail, UART_XMIT_SIZE) <
			WAKEUP_CHARS)
		uart_event(info, EVT_WRITE_WAKEUP);

	if (info->xmit.head == info->xmit.tail)
		pxa_stop_tx(info->port, 0);
}
static void pxa_int1(int irq, void *dev_id, struct pt_regs *regs)
{

	struct uart_info *info = dev_id;
	unsigned int status, pass_counter = 0;
	unsigned int lsr;
	struct tty_struct *tty = info->tty;

#ifdef UART_DMA_ENABLED
	STIER &= ~IER_DMAE;
	pxa_stop_rx(info->port);
#endif
	do {	
		status = STLSR;
#ifdef DEBUG
		printk("LSR0x%2x\n", status);
#endif	
		if (status & LSR_DR)
			rx_chars(info, status, regs);

		if (status & LSR_TDRQ) {
			pxa_tx_chars(info);
#ifdef DEBUG
			printk("Transmit\n");
#endif	
		}

		if (pass_counter++ > PXA_ISR_PASS_LIMIT) {
			unsigned char ch;
#if 0 //def DEBUG
			printk("pxa_int loop break: IIR0x%2x, LSR0x%2x, IER0x%2x, " 
			       "LCR0x%2x, MCR0x%2x, MSR0x%2x, ISR0x%2x\n", 
			       	STIIR, STLSR, STIER, STLCR, STMCR, STMSR, STISR);
#endif	
/* This case shouldn't happen */
//			while(!(STLSR & LSR_DR) && ( STIIR & 0xc)) {
				ch = STRBR;
				printk("This case shouldn't happen!!! receive 0x%2x\n", ch);
//			}	
			break;
		}
	} while (!(STIIR & IIR_NIP));
	
#ifdef UART_DMA_ENABLED
	STIER |= IER_DMAE;
	pxa_hwuart_dma_rx_start();
//	pxa_start_tx(info->port, 1, 0);
#endif
}

static void pxa_int(int irq, void *dev_id, struct pt_regs *regs)
{

	struct uart_info *info = dev_id;
	unsigned int status, pass_counter = 0;
	unsigned int lsr;
	struct tty_struct *tty = info->tty;

//	status = HWIIR;
	status = STIIR;
	
#ifdef UART_DMA_ENABLED
	pxa_stop_rx(info->port);
#endif

	do {	
#ifdef DEBUG
		printk("IIR0x%2x\n", status);
#endif	
		if ( (status & IIR_IID) == 0x6 ) {
			lsr = STLSR;
#if 0
			printk("\nL:");
#endif
/* modified by WUL */
			pxa_rx_chars1(info, lsr, regs);
			HDEBUG("Line Status Error. HWLSR=0x%2x",lsr);

/*****			
			while (lsr & LSR_FIFOE ) {
//				HDEBUG("FIFO Error. HWLSR=0x%x, HWRBR=0x%x", lsr, HWRBR);
				HDEBUG("FIFO Error. HWLSR=0x%2x, HWRBR=0x%2x", lsr, STRBR);
#ifdef DEBUG	
				printk("\nFIFO Error. STLSR=0x%2x, STRBR=0x%2x\n", lsr, STRBR);	
#endif
				lsr = STLSR;	
			} 
*****/
#ifdef DEBUG
			printk("Line Status Error\n");
#endif			
		}

		if ( (status & IIR_TOD) || ( (status & IIR_IID) == 0x4) ) {
			/* Time Out Detected or Received Data Available */
			pxa_rx_chars(info, regs);
#ifdef DEBUG
			printk("Timeout or receive\n");
#endif			
		} 
		

		if ( (status & IIR_IID) == 0x2 ) {
			pxa_tx_chars(info);
#ifdef DEBUG
			printk("Transmit\n");
#endif	
		}

		if ( (status & IIR_IID) == 0x0 ) {
			lsr = STLSR;
/* For sometimes there are still data in FIFO. By WUL */
#if 0
			printk("\nM:");
#endif
			if (lsr & LSR_DR) {
				pxa_rx_chars1(info, lsr, regs);
#if 0
			printk("Data receive\n");
#endif	
			}	
		}	
	
		if (pass_counter++ > PXA_ISR_PASS_LIMIT) {
			lsr = STLSR;
#if 1	//def DEBUG
			printk("pxa_int loop break.IIR0x%2x, LSR0x%2x\n", status, lsr);
#endif	
/* This case shouldn't happen */
			printk("pxa_int loop break.IIR0x%2x\n", status);
			if (tty->flip.count >= TTY_FLIPBUF_SIZE) {
				unsigned char ch;
				int	max_count = 64;
				do {
					ch = STRBR;
					lsr = STLSR;
				} while ((lsr & LSR_DR) && (max_count-- > 0));
			}
			break;
		}
//		status = HWIIR;
		status = STIIR;
#ifdef DEBUG
		printk("else int status= 0x%2x\n", status);
#endif	
	} while (!(status & IIR_NIP));
	
#ifdef UART_DMA_ENABLED
	pxa_hwuart_dma_rx_start();
#endif
}

/*
 * Return TIOCSER_TEMT when transmitter is not busy.
 */
static u_int pxa_tx_empty(struct uart_port *port)
{
//	return HWLSR & LSR_TEMT ? TIOCSER_TEMT : 0;
	return STLSR & LSR_TEMT ? TIOCSER_TEMT : 0;

}

static u_int pxa_get_mctrl(struct uart_port *port)
{
	return TIOCM_CTS | TIOCM_DSR | TIOCM_CAR;
}

static void pxa_set_mctrl(struct uart_port *port, u_int mctrl)
{
}

/*
 * break disabled.
 */
static void pxa_break_ctl(struct uart_port *port, int break_state)
{
	HDEBUG("pxa_break_ctl");
}

static int pxa_startup(struct uart_port *port, struct uart_info *info)
{

	int retval;
	
	set_GPIO_mode(GPIO46_STRXD_MD);
	set_GPIO_mode(GPIO47_STTXD_MD);

	/* Allocate the IRQ */
//	retval = request_irq(port->irq, pxa_int, 0, "serial_pxa", info);
	retval = request_irq(port->irq, pxa_int1, 0, "serial_pxa", info);
	if (retval)
		return retval;

	/* If there is a specific "open" function (to register control line interrupts) */

	if (pxa_open) {
		retval = pxa_open(port, info);
		if (retval) {
			free_irq(port->irq, info);
			return retval;
		}
	}

	return 0;
}

static void pxa_shutdown(struct uart_port *port, struct uart_info *info)
{

	/* Free the interrupt */
	free_irq(port->irq, info);

	/* If there is a specific "close" function (to unregister control line interrupts) */
	if (pxa_close)
		pxa_close(port, info);

	/* Disable all interrupts, port and break condition. */
//	HWIER &= ~(IER_RTOIE | IER_TIE | IER_RLSE | IER_RAVIE);
	STIER &= ~(IER_RTOIE | IER_TIE | IER_RLSE | IER_RAVIE | IER_DMAE);
}

static void pxa_change_speed(struct uart_port *port, u_int cflag, u_int iflag, u_int quot)
{

//	HWIER &= ~IER_UUE;
	STIER &= ~IER_UUE;

#ifdef UART_DMA_ENABLED
	pxa_stop_rx(port);
#endif
	/* byte size and parity */
	switch (cflag & CSIZE) {
//	case CS7:	HWLCR |= LCR_WLS_7;		break;
//	default:	HWLCR |= LCR_WLS_8;		break;
	case CS7:	STLCR |= LCR_WLS_7;		break;
	default:	STLCR |= LCR_WLS_8;		break;
	}
	if (cflag & CSTOPB)
//		HWLCR |= LCR_STB;
		STLCR |= LCR_STB;
	if (cflag & PARENB) {
//		HWLCR |= LCR_PEN;
		STLCR |= LCR_PEN;
		if (!(cflag & PARODD))
//			HWLCR |= LCR_EPS;
			STLCR |= LCR_EPS;
	}


	port->read_status_mask = LSR_OE;
	if (iflag & INPCK)
		port->read_status_mask |= LSR_FE | LSR_PE;
	if (iflag & (BRKINT | PARMRK))
		port->read_status_mask |= LSR_BI;

	/* Characters to ignore */
	port->ignore_status_mask = 0;
	if (iflag & IGNPAR)
		port->ignore_status_mask |= LSR_FE | LSR_PE;
	if (iflag & IGNBRK) {
		port->ignore_status_mask |= LSR_BI;

		 /* If we're ignoring parity and break indicators,
		    ignore overruns too (for real raw support). */

		if (iflag & IGNPAR)
			port->ignore_status_mask |= LSR_OE;
	}

//	HWFCR |= FCR_RESETTF | FCR_RESETRF | FCR_TRFIFOE;
	STFCR |= FCR_RESETTF | FCR_RESETRF | FCR_TRFIFOE;

	/* set the baud rate */
/*
	HWLCR |= LCR_DLAB;
	HWDLL = quot;
	HWDLH = 0;
	HWLCR &= ~LCR_DLAB;

	HWIER |= IER_UUE;
*/
#ifdef DEBUG
	printk("speed DLL=%d\n", quot);
#endif
	STLCR |= LCR_DLAB;
	STDLL = quot;
	STDLH = 0;
	STLCR &= ~LCR_DLAB;

	STIER |= IER_UUE;

#ifdef UART_DMA_ENABLED
//      HWIER |= IER_RTOIE | IER_RLSE;
	STIER |= IER_RTOIE | IER_RLSE | IER_DMAE;
	pxa_hwuart_dma_rx_start();
#else
//	HWIER |= IER_RTOIE | IER_TIE | IER_RLSE | IER_RAVIE;
	STIER |= IER_RTOIE | IER_TIE | IER_RLSE | IER_RAVIE;
#endif

}

static const char *pxa_type(struct uart_port *port)
{
//	return port->type == PORT_PXA ? "PXA HWUART" : NULL;
	return port->type == PORT_PXA ? "PXA UART" : NULL;

}

/*
 * Release the memory region(s) being used by 'port'.
 */
static void pxa_release_port(struct uart_port *port)
{
	release_mem_region(port->mapbase, UART_PORT_SIZE);
}

/*
 * Request the memory region(s) being used by 'port'.
 */
static int pxa_request_port(struct uart_port *port)
{
	return request_mem_region(port->mapbase, UART_PORT_SIZE,"serial_pxa") != NULL ? 0 : -EBUSY;
}

/*
 * Configure/autoconfigure the port.
 */
static void pxa_config_port(struct uart_port *port, int flags)
{
	if (flags & UART_CONFIG_TYPE && pxa_request_port(port) == 0) port->type = PORT_PXA;
}

/*
 * Verify the new serial_struct (for TIOCSSERIAL).
 * The only change we allow are to the flags and type, and
 * even then only between PORT_PXA and PORT_UNKNOWN
 */
static int pxa_verify_port(struct uart_port *port, struct serial_struct *ser)
{

	int ret = 0;

	if (ser->type != PORT_UNKNOWN && ser->type != PORT_PXA)
		ret = -EINVAL;
	if (port->irq != ser->irq)
		ret = -EINVAL;
	if (ser->io_type != SERIAL_IO_MEM)
		ret = -EINVAL;
	if (port->uartclk / 16 != ser->baud_base)
		ret = -EINVAL;
	if ((void *)port->mapbase != ser->iomem_base)
		ret = -EINVAL;
	if (port->iobase != ser->port)
		ret = -EINVAL;
	if (ser->hub6 != 0)
		ret = -EINVAL;
	return ret;

}

static void pxa_pm(struct uart_port* port, u_int state, u_int oldstate)
{

	if (state) {
//		CKEN &= ~CKEN4_HWUART;
		CKEN &= ~CKEN5_STUART;	
		HDEBUG("hwuart suspend");
	} else {
//		CKEN |= CKEN4_HWUART;
		CKEN |= CKEN5_STUART;
		HDEBUG("hwuart resume");
	}
}


static struct uart_ops pxa_pops = {
	tx_empty:	pxa_tx_empty,
	set_mctrl:	pxa_set_mctrl,
	get_mctrl:	pxa_get_mctrl,
	stop_tx:	pxa_stop_tx,
	start_tx:	pxa_start_tx,
	stop_rx:	pxa_stop_rx,
	enable_ms:	pxa_enable_ms,
	break_ctl:	pxa_break_ctl,
	startup:	pxa_startup,
	shutdown:	pxa_shutdown,
	change_speed:	pxa_change_speed,
	pm:		pxa_pm,
	type:		pxa_type,
	release_port:	pxa_release_port,
	request_port:	pxa_request_port,
	config_port:	pxa_config_port,
	verify_port:	pxa_verify_port
};

static struct uart_port pxa_ports[NR_PORTS];

/*
 * Setup the PXA serial ports. Only HWUART is enabled in this driver.
 */
static void pxa_init_ports(void)
{

	static int first = 1;
	int i;

	if (!first)
		return;
	first = 0;

	for (i = 0; i < NR_PORTS; i++) {
		/* Intel PXA26x Processor Family Developer's Manual p3-6 */
		pxa_ports[i].uartclk  = 14745600;
		pxa_ports[i].ops      = &pxa_pops;
		pxa_ports[i].fifosize = FIFO_THRESHHOLD;
		pxa_ports[i].flags = ASYNC_BOOT_AUTOCONF | ASYNC_LOW_LATENCY;

//		pxa_ports[i].membase =(void*)&HWUART;
		pxa_ports[i].membase =(void*)&STUART;
//		pxa_ports[i].mapbase = __PREG(HWUART);
		pxa_ports[i].mapbase = __PREG(STUART);				
//		pxa_ports[i].irq     = IRQ_HWUART;
		pxa_ports[i].irq     = IRQ_STUART;
		pxa_ports[i].iotype  = SERIAL_IO_MEM;
	}

	/* This enables the HWUART */
/*It shouldn't been initialized here to enable the HWUART */  
/*
	CKEN |= CKEN4_HWUART;
	CKEN |= CKEN5_STUART;	

//	set_GPIO_mode(GPIO42_HWRXD_MD);
//	set_GPIO_mode(GPIO43_HWTXD_MD);
//	set_GPIO_mode(GPIO44_HWCTS_MD);
//	set_GPIO_mode(GPIO45_HWRTS_MD);

	set_GPIO_mode(GPIO46_STRXD_MD);
	set_GPIO_mode(GPIO47_STTXD_MD);

//	HWIER |= IER_UUE;
//	HWFCR = FCR_RESETTF | FCR_RESETRF | FCR_TRFIFOE;
//	switch ( FIFO_THRESHHOLD ) {
//		case 1: HWFCR |= FCR_ITL_1; break;
//		case 8: HWFCR |= FCR_ITL_8; break;
//		case 16: HWFCR |= FCR_ITL_16; break;
//		case 32: HWFCR |= FCR_ITL_32; break;
//		default: HWFCR |= FCR_ITL_8;
//	}

//	HWLCR = LCR_WLS_8;

	STIER |= IER_UUE;
	STFCR = FCR_RESETTF | FCR_RESETRF | FCR_TRFIFOE;
	switch ( FIFO_THRESHHOLD ) {
		case 1: STFCR |= FCR_ITL_1; break;
		case 8: STFCR |= FCR_ITL_8; break;
		case 16: STFCR |= FCR_ITL_16; break;
		case 32: STFCR |= FCR_ITL_32; break;
		default: STFCR |= FCR_ITL_8;
	}

	STLCR = LCR_WLS_8;

#ifdef UART_DMA_ENABLED
//	HWIER |= IER_RTOIE | IER_RLSE ;
	STIER |= IER_RTOIE | IER_RLSE ;
#else
//	HWIER |= IER_RTOIE | IER_TIE | IER_RLSE | IER_RAVIE;
	STIER |= IER_RTOIE | IER_TIE | IER_RLSE | IER_RAVIE;
#endif

#ifdef ENABLE_HWFC
	HWMCR = MCR_OUT2 | MCR_AFE | MCR_RTS; //// Hardware flow control enabled 
	HDEBUG("Hardware Flow Control Enabled!");
#else
//	HWMCR = MCR_OUT2;
	STMCR = MCR_OUT2;
	HDEBUG("Hardware Flow Control Disabled!");
#endif
//	enable_irq(IRQ_HWUART);
	enable_irq(IRQ_STUART);
*/
}

#ifdef CONFIG_SERIAL_PXA_CONSOLE

#define BOTH_EMPTY (LSR_TDRQ | LSR_TEMT)

/*
 * Interrupts are disabled on entering
 */

static void pxa_console_write(struct console *co, const char *s, u_int count)
{
//        struct uart_port *port = pxa_ports + co->index;
        unsigned int old_ier, status, i;

        /*
         *      First, save LER and then disable interrupts
         */
        old_ier = STIER;
        STIER = 0x40;

        /*
         *      Now, do each character
         */
        for (i = 0; i < count; i++) {
                do {
                        status = STLSR;
                } while ((status & BOTH_EMPTY) !=  BOTH_EMPTY);
                STTHR = s[i];
                if (s[i] == '\n') {
                        do {
                                status = STLSR;
                        } while ((status & BOTH_EMPTY) !=  BOTH_EMPTY);
                        STTHR =  '\r';
                }
        }

        /*
         *      Finally, wait for transmitter to become empty
         *      and restore IER
         */
        do {
                status = STLSR;
        } while ((status & BOTH_EMPTY) !=  BOTH_EMPTY);
        STIER = old_ier;
}


static kdev_t pxa_console_device(struct console *co)
{
        return MKDEV(SERIAL_PXA_MAJOR, MINOR_START + 3);
}

static int pxa_console_wait_key(struct console *co)
{
//        struct uart_port *port = pxa_ports + co->index;
        unsigned long flags;
        unsigned int old_ier, status, ch;

        /*
         * Save IER and disable interrupts
         */
        save_flags(flags);
        cli();
        old_ier = STIER;
       	STIER &= ~(IER_TIE | IER_RAVIE);
        restore_flags(flags);

        /*
         * Wait for a character
         */
        do {
                status = STLSR;
        } while (!(status & LSR_DR));
        ch = STRBR;

        /*
         * Restore IER
         */
	save_flags(flags);
        cli();
        STIER = old_ier;
	 restore_flags(flags);

        return ch;
}

/*
 * If the port was already initialised (eg, by a boot loader), try to determine
 * the current setup.
 */
static void __init
pxa_console_get_options(struct uart_port *port, int *baud, int *parity, int *bits)
{
        unsigned int lcr, quot;

        lcr = STLCR;
       
        *parity = 'n';
        if (lcr & LCR_PEN) {
               if (lcr & LCR_EPS)
               		*parity = 'e';
               else
               		*parity = 'o';
        }

        switch (lcr & LCR_WLS) {
        	case LCR_WLS_8:
	        	*bits = 8;
			break;
                case LCR_WLS_7:
                        *bits = 7;
			break;
		case LCR_WLS_6:
                        *bits = 6;
			break;
		case LCR_WLS_5:
                        *bits = 5;
			break;
	}
        quot = STDLL | STDLH << 8;
        *baud = port->uartclk / (16 * (quot + 1));    
}

static int __init
pxa_console_setup(struct console *co, char *options)
{
        struct uart_port *port;
        int baud = CONFIG_PXA_DEFAULT_BAUDRATE;
        int bits = 8;
        int parity = 'n';
        int flow = 'n';

	/*
         * Check whether an invalid uart number has been specified, and
         * if so, search for the first available port that does have
         * console support.
         */
        port = uart_get_console(pxa_ports, NR_PORTS, co);

        if (options)
                uart_parse_options(options, &baud, &parity, &bits, &flow);
        else
                pxa_console_get_options(port, &baud, &parity, &bits);

        return uart_set_options(port, co, baud, parity, bits, flow);
}

static struct console pxa_console = {
        name:           "ttyS",
        write:          pxa_console_write,
        device:         pxa_console_device,
        wait_key:       pxa_console_wait_key,
        setup:          pxa_console_setup,
        flags:          CON_PRINTBUFFER,
        index:          -1,
};

void __init pxa_rs_console_init(void)
{
	pxa_init_ports();
	
	// disable IRDA
	GPSR0 = 0x00200000;
		
	//standby DC
	// enable DC chipset; GPIO35 as high output
	GPSR1 = 0x00000008;

	// delay 200ms
	mdelay(200);
		
	// disable IO of DC
	GPCR0 = 0x00100000;
	
	// turn off 3.6M clock
	set_GPIO_mode(GPIO11_INPUT_MD);	
		
	// turn on STUART
	set_GPIO_mode(GPIO46_STRXD_MD);
	set_GPIO_mode(GPIO47_STTXD_MD);	
	CKEN |= CKEN5_STUART;

	register_console(&pxa_console);
}

#define PXA_CONSOLE  &pxa_console
#else
#define PXA_CONSOLE  NULL
#endif

static struct uart_driver pxa_reg = {
	owner:			THIS_MODULE,
	normal_major:		SERIAL_PXA_MAJOR,
#ifdef CONFIG_DEVFS_FS
	normal_name:		HWUART_NORMAL_NAME_FS,
	callout_name:		"cuhw%d",
#else
	normal_name:		HWUART_NORMAL_NAME,
	callout_name:		"cuhw",
#endif
	normal_driver:		&normal,
	callout_major:		CALLOUT_PXA_MAJOR,
	callout_driver:		&callout,
	table:			pxa_table,
	termios:		pxa_termios,
	termios_locked:		pxa_termios_locked,
	minor:			MINOR_START + 3,
	nr:			NR_PORTS,
	port:			pxa_ports,
	cons:			PXA_CONSOLE,	
//	cons:			NULL,
};


static int __init pxa_serial_init(void)
{

	pxa_init_ports();

/*
#ifdef UART_DMA_ENABLED
	// init DMA support -- begin 
//	disable_irq(IRQ_HWUART);
//	disable_irq(IRQ_STUART);
	rxdma = pxa_request_dma("HWUART_RX",DMA_PRIO_LOW, pxa_hwuart_dma_rx_irq, 0);
	if (rxdma < 0)
		goto err_rx_dma;

	txdma = pxa_request_dma("HWUART_TX",DMA_PRIO_LOW, pxa_hwuart_dma_tx_irq, 0);
	if (txdma < 0)
		goto err_tx_dma;

	if (! ( dma_rx_buff = kmalloc(DMA_BUFF_SIZE, GFP_KERNEL | GFP_DMA) ) ) {
		goto err_dma_rx_buff;
	}
	if (! ( dma_tx_buff = kmalloc(DMA_BUFF_SIZE, GFP_KERNEL | GFP_DMA) ) ) {
		goto err_dma_tx_buff;
	}

//	DRCMR29 = rxdma | DRCMR_MAPVLD;
	DRCMR19 = rxdma | DRCMR_MAPVLD;
//	DRCMR34 = txdma | DRCMR_MAPVLD;
	DRCMR20 = txdma | DRCMR_MAPVLD;
//	enable_irq(IRQ_HWUART);
//	enable_irq(IRQ_STUART);
	HDEBUG("HWUART DMA support enabled!");
//for debug
	 printk("rx dma : %d, buff %x\n",rxdma,dma_rx_buff); 
	 printk("tx dma : %d, buff %x\n",txdma,dma_tx_buff); 
	// init DMA support -- end 
#endif
*/
	return uart_register_driver(&pxa_reg);
/*
#ifdef UART_DMA_ENABLED
err_dma_tx_buff:
	kfree(dma_rx_buff);
err_dma_rx_buff:
	pxa_free_dma(txdma);
err_tx_dma:
	pxa_free_dma(rxdma);
err_rx_dma:
//	enable_irq(IRQ_HWUART);
//	enable_irq(IRQ_STUART);
	return 1;
#endif
*/
}

static void __exit pxa_serial_exit(void)
{
/*
#ifdef UART_DMA_ENABLED
	// deinit DMA support -- begin 
	pxa_free_dma(rxdma);
	pxa_free_dma(txdma);
	if (dma_rx_buff) kfree(dma_rx_buff);
	if (dma_tx_buff) kfree(dma_tx_buff);
	// deinit DMA support -- end 
#endif
*/
	uart_unregister_driver(&pxa_reg);
}

module_init(pxa_serial_init);
module_exit(pxa_serial_exit);

EXPORT_NO_SYMBOLS;

MODULE_AUTHOR("Hao Wu");
MODULE_DESCRIPTION("PXA26x HWUART Serial Port Driver");
MODULE_LICENSE("GPL");


















































