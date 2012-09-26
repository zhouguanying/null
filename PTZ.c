#include     <stdio.h>      /*��׼�����������*/
#include     <stdlib.h>     /*��׼�����ⶨ��*/
#include     <unistd.h>     /*Unix ��׼��������*/
#include     <sys/types.h>
#include     <sys/stat.h>
#include     <sys/ioctl.h>
#include     <fcntl.h>      /*�ļ����ƶ���*/
#include     <termios.h>    /*PPSIX �ն˿��ƶ���*/
#include     <errno.h>      /*����Ŷ���*/

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

    fd = open(PTZ_DEV_NAME, O_RDWR);				//| O_NOCTTY | O_NDELAY���ַ�ʽ��open����
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
*��̨ת��
*
*cmd: 16 ��ֱ; 15 ˮƽ
*
*speed 0 - 10  , 0 ��� �� 10 ����
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
    open_ptz();			// �� �豸,ָ��Ĭ�ϵ�Ŀ���豸

    return 0;
}

