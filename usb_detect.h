#ifndef _USB_DETECT_H_
#define _USB_DETECT_H_

extern int  open_usbdet(void);
extern void close_usbdet(void);
extern int  ioctl_usbdet_led(int led);
extern int  ioctl_usbdet_read(void);

#endif
