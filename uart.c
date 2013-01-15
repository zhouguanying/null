#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>

#include "uart.h"

static int fd = -1;

static int open_ptz(void)
{
    fd = open("/dev/ptz", O_RDWR); //| O_NOCTTY | O_NDELAYÕâÖÖ·½Ê½¿´openº¯Êý
    if (fd == -1)
    {
        printf("Can't Open PTZ    \n");
        return -1;
    }
    else
    {
        ioctl(fd , 15 , 5);
        if (ioctl(fd, 16 , 5) < 0)
            printf("contrl ptz speed error");
        else
            printf("contrl ptz sucess");

        return 0;
    }
}

static int write_ptz(char *buffer, int length)
{
    if (-1 == fd)
    {
        if (open_ptz())
            return -1;
    }
    return write(fd, buffer , length);
}

int UartWrite(char *buffer, int length)
{
    return write_ptz(buffer , length);
}

