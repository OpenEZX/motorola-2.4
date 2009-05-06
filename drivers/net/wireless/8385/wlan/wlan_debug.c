/*
 *  /proc/net/wlan debug counters 
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

int string_to_number(char *s)
{
  int r = 0;
  int base = 0;

  if ((strncmp(s, "0x", 2) == 0) || (strncmp(s, "0X", 2) == 0))
    base = 16;
  else
    base = 10;

  if (base == 16)
    s += 2;

  for (s = s; *s !=0; s++)
  {
    if ((*s >= 48) && (*s <= 57))
      r = (r * base) + (*s - 48);
    else if ((*s >= 65) && (*s <= 70))
      r = (r * base) + (*s - 55);
    else if ((*s >= 97) && (*s <= 102))
      r = (r * base) + (*s - 87);
    else
      break;
  }

	return r;
}

int wlan_debug_read(char *page, char **s, off_t off, int cnt, int *eof, void *data)
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

int wlan_debug_write(struct file *f, const char *buf, unsigned long cnt, void *data)
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
  if(pdata == NULL)
	  return 0;
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

  if (priv->proc_entry == NULL)
    return;

  for (i = 0; i < num_of_items; i++)
  {
    items[i].addr += (u32)priv->adapter;
  }
  struct proc_dir_entry *r;
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
