
#include		"PTZ.h"
//******************************************************************

#ifndef	_UARTDEV_ARRAY
static	char 	*UARTDEV[] = {
"/dev/ttymxc0",		// 串口0     "/dev/ttyS0"; 
"/dev/ttymxc1",		// 串口1
"/dev/ttymxc2",		// 串口2
"/dev/ttymxc3",		// 串口3
};
#define	_UARTDEV_ARRAY
#endif

//------------------------------------------------------------------

static	int		ptzRS485_Mode = -1;
	
static	void		ptzRS485_Init(int protocol)
{
	int	i;
	int	speed,databits,stopbits,parity;
	
	int	*temp = RS485_Set_Protocol;
	int	invalidprotocol = -1 ;
		
	for( i=0; i<(RS485_Set_Protocol_Size)/sizeof(int)/5; i++ ){
		if( protocol == *temp) {
			temp++ ;
			speed = *temp++ ;
			databits = *temp++ ;
			stopbits = *temp++ ;
			parity = *temp++ ;
			
			invalidprotocol = 0 ;
				 printf("\n ptzRS485_Init valid  protocol \n");
			break;
		}else{
			temp += 5 ;
		}
	}
	if(invalidprotocol)	return ;

	SetRS485(speed,  databits, stopbits,  parity, RS485_RECEIVE_MODE);
	ptzRS485_Mode = protocol;
				// printf("\n ptzRS485_Init protocol: %d \n",protocol);
	return ;
}

//******** 


static	void		ptzRS485Open(int protocol)
{
	int	UartPort= 2;						// UART 端口号
	
//   	 char *dev ="/dev/ttyS2"; ///dev/ttyS2对应串口com3 /dev/ttyS0对应com1 
	char		*dev = UARTDEV[UartPort]; 
		// printf("\n OpenRS485: %s \n",dev);
		
	OpenRS485(dev);
		
	ptzRS485_Init(protocol);
}
static	void		ptzRS485Close(void)
{
	CloseRS485();
}

static	void		ptzRS485Transister(char *buffer, int length, int protocol)
{
	if(ptzRS485_Mode != protocol)	ptzRS485_Init(protocol);
	
	SetRS485Direction(RS485_SEND_MODE);
	RS485Write(buffer, length);
	SetRS485Direction(RS485_RECEIVE_MODE);
}
//---------------------------------------------------------------------------------------------------

static	void		ptzSend(int	 address, int protocol, int command)	//以指定的协议向目标发送命令
{
	char 	buffer[100];
	int		length;
	
	switch(protocol) {
		case PELCO_D:
			length = PELCO_D_frame(buffer,address, command);
			break;
		case PELCO_P:
			length = PELCO_P_frame(buffer,address, command);
			break;
		default:
			length = 0 ;		// 无效通讯协议
			break;
			
			//length = PELCO_P_frame(buffer,address, command);	
			//break;
	}
	if(length == 0)	return ;		// 无效通讯协议或无效命令
	
	ptzRS485Transister(buffer,length,protocol);
	return ;
}

//*************************************************************************************
typedef	struct{
	int	address;
	int	protocol;
}ptzDESTmode;

static	ptzDESTmode		ptzDEST;

void		ptzDest(int address, int protocol)		//设置目标的地址号，通讯协议
{
	ptzDEST.address = address ;
	ptzDEST.protocol = protocol ;
}


//*************************************************************************
#define	RS485_ADDRESS		10

int		ptzOpen( void)
{
	ptzDest(RS485_ADDRESS, PELCO_P);
//-----------------------------------
	ptzRS485Open(ptzDEST.protocol );

	return	0;
}

void		ptzClose(void)
{
	ptzRS485Close();
}

void		ptzCommand(int command)
{
	ptzSend(ptzDEST.address, ptzDEST.protocol, command) ;
}

//-------------------------------------------------------------------------
int		ptzGetCommand(int *com)	//从ETHERNET 获得一个有效命令
{
	return	-1 ;		// 收到一个有效命令，返回-1
}
//*************************************************************************



#include     <stdio.h>      /*标准输入输出定义*/
#include     <stdlib.h>     /*标准函数库定义*/
#include     <unistd.h>     /*Unix 标准函数定义*/
#include     <sys/types.h>  
#include     <sys/stat.h>   
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
	if (fd == -1)	{
		printf("Can't Open PTZ	\n");
		return -1;
	} else {
		ioctl(fd , 15 , 0);
		if(ioctl(fd, 16 ,0)<0)
		{
			printf("###########contrl ptz speed error############\n");
		}else
			printf("#################contrl ptz sucess#############\n");
		return	0;
	}
}

static	void		close_ptz(void)
{
	close(fd);
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
	if(-1==fd)	{
		if( open_ptz() )	{
			return -1;
		}
	}
	return	ioctl(fd, cmd ,speed);
}

int	write_ptz(char *buffer, int length)
{
	if(-1==fd)	{
		if( open_ptz() )	{
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

#if	0
int	UartWrite(char *buffer, int length)
{
	return	write_ptz( buffer ,length);

}
extern	int	UartWrite(char *buffer, int length);
#endif


//*************************************************************************************

int	PTZ(void)
{
	printf("PTZ INIT !!! \n \n\n\n\n");
	open_ptz();			// 打开 设备,指向默认的目标设备

	return 0;
}

//****************************************************************************

