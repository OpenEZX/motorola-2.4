/*
 * linux/drivers/CEBus_chardev/CEBus_chardev.c 
 *
 * Copyright (C) 2003 Motorola Inc.
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
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/wrapper.h>
#include <asm/hardware.h>
#include <asm/irq.h>
#include <linux/init.h>
#include <linux/interrupt.h>

//#include <asm/arch/pxa-regs.h>

#define SUCCESS 0
#define DEVICE_NAME "char_dev"
static int Major=127;
static int Device_Open=0;
static int gpio_state=0x7f;

#define GPIO6_1  (1<<6)
#define GPIO7_1  (1<<7)
#define GPIO8_1  (1<<8)
#define GPIO36_1 (1<<4)
#define GPIO37_1 (1<<5)
#define GPIO38_1 (1<<6)
#define GPIO41_1 (1<<9)
static u32 cebus_irq_is_free = 0;
extern u32 cebus_is_open;

static int device_open(struct inode *inode,struct file *file)
{
  if(Device_Open)
  {
/*    printk("The device is opened\n");*/
    return -EBUSY;
  }
  Device_Open++;
  
  GAFR0_L&=0xfffc0fff;
  GAFR1_L&=0xfff3c0ff;
  GPDR0&=~((1<<6)|(1<<7)|(1<<8));
  GPDR1&=~((1<<4)|(1<<5)|(1<<6)|(1<<9));

  MOD_INC_USE_COUNT;
  if(cebus_is_open)
  {
      cebus_irq_is_free = 1;
      cebus_is_open = 0;
      free_irq(IRQ_GPIO(6), NULL);
      free_irq(IRQ_GPIO(7), NULL);
  }
  return SUCCESS;
}
static int device_release(struct inode *inode,struct file *file)
{
  Device_Open--;
  MOD_DEC_USE_COUNT;
  if(cebus_irq_is_free)
  {
      cebus_irq_is_free = 0;
      /* here we should recovery the GPIO6 and GPIO7 interrupt handler */
      cebus_is_open = 0;
  }
  return 0;
}
static int device_read(struct file *file,char *buffer,size_t count,loff_t *ppos)
{
  int d=0,i;
  int num;
  for(i=0;i<7;i++)
  { 
    num=1<<i;
    switch(i)
    {
    case 0:
      if(GPLR0&GPIO6_1)
	d|=num;
      break;
    case 1:
      if(GPLR0&GPIO7_1)
	d|=num;
      break;
    case 2:
      if(GPLR0&GPIO8_1)
	d|=num;            
      break;
    case 3:
      if(GPLR1&GPIO36_1)
	d|=num;
      break;
    case 4:
      if(GPLR1&GPIO37_1)
	d|=num;
      break;
    case 5:
      if(GPLR1&GPIO38_1)
	d|=num;
      break;
    case 6:
      if(GPLR1&GPIO41_1)
	d|=num;
      break;
    default:
      break;
    }
  } 
  //  d&=~gpio_state;
  buffer[0]=d%256;
  buffer[1]=d/256;
  return(0);
}
static int device_write(struct file *file,const char *buffer,size_t count,loff_t *ppos)
{
  int d,i;
  int num;
  GPCR0=(1<<6)|(1<<7)|(1<<8);
  GPCR1=(1<<4)|(1<<5)|(1<<6)|(1<<9);
  d=buffer[1]*256+buffer[0];
  d&=gpio_state;
  for(i=0;i<7;i++)
  {
    num=1<<i;
    if(d&num)
    {
      switch(i)
      {
      case 0:
	GPSR0=GPIO6_1;
	break;
      case 1:
	GPSR0=GPIO7_1;
	break;
      case 2:
	GPSR0=GPIO8_1;
	break;
      case 3:
	GPSR1=GPIO36_1;
	break;
      case 4:
	GPSR1=GPIO37_1;
	break;
      case 5:
	GPSR1=GPIO38_1;
	break;
      case 6:
	GPSR1=GPIO41_1;
	break;
      default:
	break;
      }
    }
  }
  return 0;
}
static int device_ioctl(struct inode *inode,struct file *file,unsigned int cmd,unsigned long arg)
{
  int i,count;
  
  gpio_state=arg;
  GPDR0&=~((1<<6)|(1<<7)|(1<<8));
  GPDR1&=~((1<<4)|(1<<5)|(1<<6)|(1<<9));
  for(i=0;i<7;i++)
  {
    count=1<<i;
    if(arg&count)
    {
      switch(i)
      {
      case 0:
	GPDR0|=GPIO6_1;
	break;
      case 1:
	GPDR0|=GPIO7_1;
	break;
      case 2:
	GPDR0|=GPIO8_1;
	break;
      case 3:
	GPDR1|=GPIO36_1;
	break;
      case 4:
	GPDR1|=GPIO37_1;
	break;
      case 5:
	GPDR1|=GPIO38_1;
	break;
      case 6:
	GPDR1|=GPIO41_1;
	break;
      default:
	break;
	}
    }
  }
  return 0;
}

static struct file_operations Fops={
  read:   device_read,
  write:  device_write,
  ioctl:  device_ioctl,
  open:   device_open,
  release:device_release,
};

int init_CEBus_chardev()
{
  int ret;

  ret=register_chrdev(Major,DEVICE_NAME,&Fops);
  if(ret<0){
/*    printk("Sorry,registering the character device failed with %d.\n",Major);*/
    return ret;
  }
/*  printk("Registeration is a success.The major device number is %d.\n",Major);*/
  return 0;
}
void clean_CEBus_chardev()
{
  int ret;
  ret=unregister_chrdev(Major,DEVICE_NAME);
/*  if(ret<0)
    printk("Error in module_unregister_chrdev:%d.\n",ret);*/
}

module_init(init_CEBus_chardev);
module_exit(clean_CEBus_chardev);







