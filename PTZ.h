
#ifndef _PTZ_H_
#define _PTZ_H_
//------------------------------------------------------

#include		"RS485.h"
#include 		"PTZprotocol.h"

//******************************************************


extern	void		ptzDest(int address, int protocol)	;	//设置目标的地址号，通讯协议

extern	int		ptzOpen( void) ;
extern	void		ptzClose(void) ;
extern	void		ptzCommand(int command) ;


//--------------------------------------------------------------------------------
int		ptzGetCommand(int *com) ;		//从ETHERNET 获得一个有效命令
//********************************************************************************

#endif

