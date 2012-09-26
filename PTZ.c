#include     <stdio.h>      /*标准输入输出定义*/
#include     <stdlib.h>     /*标准函数库定义*/
#include     <unistd.h>     /*Unix 标准函数定义*/
#include     <sys/types.h>
#include     <sys/stat.h>
#include     <sys/ioctl.h>
#include     <fcntl.h>      /*文件控制定义*/
#include     <termios.h>    /*PPSIX 终端控制定义*/
#include     <errno.h>      /*错误号定义*/

//#include		"PTZ.h"
//******************************************************************
/****
  server.c
****/
//------------------------------------------------------------------

#define	PTZ_DEV_NAME	"/dev/ptz"
//#define	PTZ_DEV_NAME	"ptz"

static	int 	fd=-1;

static	int	open_ptz(void)
{
    //fd = open(dev, O_RDWR | O_NOCTTY );

    fd = open(PTZ_DEV_NAME, O_RDWR);				//| O_NOCTTY | O_NDELAY这种方式看open函数
    if (fd == -1)
    {
        printf("Can't Open PTZ	\n");
        return -1;
    }
    else
    {
        ioctl(fd , 15 , 0);
        if(ioctl(fd, 16 ,0)<0)
        {
            printf("###########contrl ptz speed error############\n");
        }
        else
            printf("#################contrl ptz sucess#############\n");
        return	0;
    }
}

/*
*云台转动
*
*cmd: 16 垂直; 15 水平
*
*speed 0 - 10  , 0 最快 ， 10 最慢
*/
int	speed_ptz(int cmd, int speed)
{
    if(-1==fd)
    {
        if( open_ptz() )
        {
            return -1;
        }
    }
    return	ioctl(fd, cmd ,speed);
}

int	write_ptz(char *buffer, int length)
{
    if(-1==fd)
    {
        if( open_ptz() )
        {
            return -1;
        }
    }
    return	write(fd, buffer ,length);
}

int	read_ptz(char *buffer, int length)
{
    int	nread;
    nread = read(fd,buffer,length);
    return	nread;
}
extern	int	write_ptz(char *buffer, int length);
extern	int	read_ptz(char *buffer, int length);

int	PTZ(void)
{
    printf("PTZ INIT !!! \n \n\n\n\n");
    open_ptz();			// 打开 设备,指向默认的目标设备

    return 0;
}

