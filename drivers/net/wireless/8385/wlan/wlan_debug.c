/*
 *  /proc/net/wlan debug counters 
 *
 * (c) Copyright © 2003-2006, Marvell International Ltd. 
 *
 * This software file (the "File") is distributed by Marvell International 
 * Ltd. under the terms of the GNU General Public License Version 2, June 1991 
 * (the "License").  You may use, redistribute and/or modify this File in 
 * accordance with the terms and conditions of the License, a copy of which 
 * is available along with the File in the license.txt file or by writing to 
 * the Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 
 * 02111-1307 or on the worldwide web at http://www.gnu.org/licenses/gpl.txt.
 *
 * THE FILE IS DISTRIBUTED AS-IS, WITHOUT WARRANTY OF ANY KIND, AND THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE 
 * ARE EXPRESSLY DISCLAIMED.  The License provides additional details about 
 * this warranty disclaimer.
 *
 */

#include  "include.h"

struct debug_data {
  char  name[32];
  u32   size;
  u32   addr;
};

#define item_size(n) (sizeof ((wlan_adapter *)0)->n)
#define item_addr(n) ((u32) &((wlan_adapter *)0)->n)

// To debug any member of wlan_adapter, simply add one line here.

static struct debug_data items[] = {
  {"IntCounter",  item_size(IntCounter),  item_addr(IntCounter)},
  {"PSMode",      item_size(PSMode),      item_addr(PSMode)},
  {"PSState",     item_size(PSState),     item_addr(PSState)},
};

static int num_of_items = sizeof(items)/sizeof(items[0]);



static int wlan_debug_read(char *page, char **s, off_t off, int cnt, int *eof, void *data)
{
  int val = 0;
  char *p = page;
  int i;
  
  struct debug_data *d = (struct debug_data *)data;

  MOD_INC_USE_COUNT;

  for (i = 0; i < num_of_items; i++)
  {
	if (d[i].size == 1)
		val = *((u8 *)d[i].addr);
	else if (d[i].size == 2)
		val = *((u16 *)d[i].addr);
	else if (d[i].size == 4)
		val = *((u32 *)d[i].addr);

	p += sprintf(p, "%s=%d\n", d[i].name, val);

  }

   MOD_DEC_USE_COUNT;
  return p-page;
}

static int wlan_debug_write(struct file *f, const char *buf, unsigned long cnt, void *data)
{
  int r,i;
  char *pdata;
  char *p;
  char *p0;
  char *p1;
  char *p2;
  struct debug_data *d = (struct debug_data *)data;

  MOD_INC_USE_COUNT;
  
  pdata = (char *)kmalloc(cnt, GFP_KERNEL);
  if(pdata == NULL) {
	MOD_DEC_USE_COUNT;
	return 0;
  }
  copy_from_user(pdata, buf, cnt);
  p0 = pdata;
  for (i = 0; i < num_of_items; i++)
  {
	  do
	  {
		p = strstr(p0,d[i].name);
		if(p == NULL)
			break;
	    p1 = strchr(p, '\n');
		if(p1 == NULL)
			break;
		p0 = p1++;
		p2 = strchr(p, '=');
		if(!p2)
			break;
		p2++;
		r = string_to_number(p2);
		if (d[i].size == 1)
			*((u8 *)d[i].addr) = (u8)r;
		else if (d[i].size == 2)
			*((u16 *)d[i].addr) = (u16)r;
		else if (d[i].size == 4)
			*((u32 *)d[i].addr) = (u32)r;
		break;
	  }while (TRUE);
  }
  kfree(pdata);
  MOD_DEC_USE_COUNT;
  return cnt;
  
}

void wlan_debug_entry(wlan_private *priv, struct net_device *dev)
{
  int i;
  struct proc_dir_entry *r;

  if (priv->proc_entry == NULL)
    return;

  for (i = 0; i < num_of_items; i++)
  {
    items[i].addr += (u32)priv->adapter;
  }
  r = create_proc_entry("debug", 0644, priv->proc_entry);

  if (r == NULL)
      return;

    r->data       = &items[0];
    r->read_proc  = wlan_debug_read;
    r->write_proc = wlan_debug_write;
    r->owner      = THIS_MODULE;

}

void wlan_debug_remove(wlan_private *priv)
{
	remove_proc_entry("debug", priv->proc_entry);
}

// End of file
