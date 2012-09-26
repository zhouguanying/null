
#ifndef _UART_H_
#define _UART_H_
//------------------------------------------------------------------------------------------



//**************************************************************************
extern	int		OpenUart(char	*dev);
extern	void		CloseUart(void);



extern	int		UartWrite(char *buffer, int length);
extern	int		UartWriteChar(char *buffer);

extern	int		UartRead(char *buffer, int length);
extern	int		UartReadCharPolling(char *buffer);

extern	int		SetUart(int speed, int databits, int stopbits, int parity);
//*****************************************************************************
extern	int		SetUartDirection(int	direction);
void SetUartSpeed(int speed);
//---------------------------------------------------------------------
#endif

