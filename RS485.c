

#include		"RS485.h"
//******************************************************************

int		SetRS485Direction(int direction)
{
	return	SetUartDirection(direction);
}
//***************************************************************************


int		OpenRS485(char	*dev)
{
	return	OpenUart(dev);
}
void		CloseRS485(void)
{
	CloseUart();
}


int	RS485Write(char *buffer, int length)
{
	return	UartWrite(buffer ,length);
}
int	RS485WriteChar(char *buffer)
{
	return	UartWriteChar(buffer);
}
int	RS485Read(char *buffer, int length)
{
	return	UartRead(buffer,length);
}

int	RS485ReadCharPolling(char *buffer)
{
	return	 UartReadCharPolling(buffer);
}
//------------------------------------------------------

int		SetRS485(int speed, int databits, int stopbits, int parity, int direction)
{
	SetRS485Direction(direction);
	return	SetUart(speed, databits, stopbits,parity);
}
//***********************************************************************************


