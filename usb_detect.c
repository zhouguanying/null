#include  <stdio.h>
#include  <stdlib.h>
#include  <unistd.h>
#include  <sys/ioctl.h>
#include  <sys/types.h>
#include  <sys/stat.h>
#include  <fcntl.h>
#include  <termios.h>
#include  <errno.h>
#include "usb_detect.h"

#define    IOCTL_USBDET_READ                11
#define    IOCTL_USBDET_LED_DRIVE        15

#define    USBDET_DEV_NAME    "/dev/gpio"


static    int     fd = -1;

int open_usbdet(void)
{
    //fd = open(dev, O_RDWR | O_NOCTTY );

    fd = open(USBDET_DEV_NAME, O_RDWR);                //| O_NOCTTY | O_NDELAY这种方式看open函数
    if (fd == -1)
    {
        printf("Can't Open USBDET    \n");
        return -1;
    }
    else
    {
        fcntl(fd , F_SETFD , 1); //close on exec
        return    0;
    }
}

int ioctl_usbdet_read(void)
{
    int    ret;

    if (-1 == fd)
    {
        if (open_usbdet())
        {
            return -1;
        }
    }

    ret = ioctl(fd, _IOR(DEV_MAGIC,USBD,int), NULL);

    return    ret;
}

int ioctl_usbdet_led(int led)
{
    int    ret;

	return 0;

    if (-1 == fd)
    {
        if (open_usbdet())
        {
            return -1;
        }
    }

    ret = ioctl(fd, IOCTL_USBDET_LED_DRIVE, led);

    return    ret;
}

extern    int    ioctl_usbdet_led(int led);
extern    int    ioctl_usbdet_read(void);

