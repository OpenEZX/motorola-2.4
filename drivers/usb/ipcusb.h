/**created by Levis(Levis@motorola.com) for mux driver use in 11/18/03**/

extern struct tty_driver *usb_for_mux_driver;
extern struct tty_struct *usb_for_mux_tty;
extern void (*usb_mux_dispatcher)(struct tty_struct *tty);
extern void (*usb_mux_sender)(void);
