#include     <stdio.h>      /*标准输入输出定义*/
#include     <stdlib.h>     /*标准函数库定义*/
#include     <unistd.h>     /*Unix 标准函数定义*/
#include     <sys/types.h>  
#include     <sys/stat.h>   
#include     <fcntl.h>      /*文件控制定义*/
#include     <termios.h>    /*PPSIX 终端控制定义*/
#include     <errno.h>      /*错误号定义*/

//---------------------------------------------------------------

#define	IOCTL_USBDET_READ				11
#define	IOCTL_USBDET_LED_DRIVE		15

#define	USBDET_DEV_NAME	"/dev/usbdet"
//#define	USBDET_DEV_NAME	"usbdet"
//---------------------------------------------------------------


static	int 	fd=-1;

int	open_usbdet(void)
{
	//fd = open(dev, O_RDWR | O_NOCTTY ); 
	
	fd = open(USBDET_DEV_NAME, O_RDWR);				//| O_NOCTTY | O_NDELAY这种方式看open函数
	if (fd == -1)	{
		printf("Can't Open USBDET	\n");
		return -1;
	} else {
		return	0;
	}
}

void		close_usbdet(void)
{
	close(fd);
}

int	write_usbdet(char *buffer, int length)
{

	if(-1==fd)	{
		if( open_usbdet() )	{
			return -1;
		}
	}
	return	write(fd, buffer ,length);
}

int	read_usbdet(char *buffer, int length)
{
   	int	nread;

	if(-1==fd)	{
		if( open_usbdet() )	{
			return -1;
		}
	}	
	nread = read(fd,buffer,length);
	return	nread;
}

int	ioctl_usbdet_read(void)
{
	int	ret;

	if(-1==fd)	{
		if( open_usbdet() )	{
			return -1;
		}
	}

	ret = ioctl(fd,IOCTL_USBDET_READ,0);

	return	ret;
}

int	ioctl_usbdet_led(int led)
{
	int	ret;

	if(-1==fd)	{
		if( open_usbdet() )	{
			return -1;
		}
	}

	ret = ioctl(fd,IOCTL_USBDET_LED_DRIVE,led);

	return	ret;
}

//----------------------------------------------------------------------------

//============================================================
extern	int	write_usbdet(char *buffer, int length);
extern	int	read_usbdet(char *buffer, int length);

extern	int	ioctl_usbdet_led(int led);
extern	int	ioctl_usbdet_read(void);

//*************************************************************************************



