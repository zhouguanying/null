
#ifndef _RS485_H_
#define _RS485_H_
//------------------------------------------------------------------------------------------

#include		"UART.h"

//**************************************************************************

#define	RS485_RECEIVE_MODE	0
#define	RS485_SEND_MODE		1

extern	int		OpenRS485(char	*dev);
extern	void		CloseRS485(void);



extern	int		RS485Write(char *buffer, int length);
extern	int		RS485WriteChar(char *buffer);

extern	int		RS485Read(char *buffer, int length);
extern	int		RS485ReadCharPolling(char *buffer);

extern	int		SetRS485(int speed, int databits, int stopbits, int parity, int direction);
//*****************************************************************************
extern	int		SetRS485Direction(int direction);

//-----------------------------------------------------------------------------

#endif

