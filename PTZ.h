
#ifndef _PTZ_H_
#define _PTZ_H_
//------------------------------------------------------

#include		"RS485.h"
#include 		"PTZprotocol.h"

//******************************************************


extern	void		ptzDest(int address, int protocol)	;	//����Ŀ��ĵ�ַ�ţ�ͨѶЭ��

extern	int		ptzOpen( void) ;
extern	void		ptzClose(void) ;
extern	void		ptzCommand(int command) ;


//--------------------------------------------------------------------------------
int		ptzGetCommand(int *com) ;		//��ETHERNET ���һ����Ч����
//********************************************************************************

#endif

