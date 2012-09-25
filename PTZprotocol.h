
#ifndef _PTZ_PROTOCOL_H_
#define _PTZ_PROTOCOL_H_
//------------------------------------------------------------------------------------------
extern	const	int		RS485_Set_Protocol[];
extern	int		RS485_Set_Protocol_Size ;
//-------------------------------------------------------------------
#define	PELCO_D	0
#define	PELCO_P	1

//-------------------------------------------------------------------
#define	UP			0
#define	DOWN		1
#define	LEFT		2
#define	RIGHT		3

#define	ZOOM_SHORT	4
#define	ZOOM_LONG		5


#define	FOCUS_NEAR	6
#define	FOCUS_FAR		7
#define	APT_NARROW	8
#define	APT_WIDE		9

#define	LIGHT_OFF		10
#define	LIGHT_ON		11

#define	GOTO_PRESET	12
#define	SET_PRESET		13
#define	DEL_PRESET		14

#define	STOP			15

#define	AUTO_ON		16
#define	AUTO_OFF		17

#define	GOTO_PRESET95		18
#define	SET_PRESET95		19
#define	DEL_PRESET95		20
//-----------------------------------------------------------------

	
extern	int	PELCO_D_frame(char *buffer,int address, int command);
extern	int	PELCO_P_frame(char *buffer,int address, int command);

//---------------------------------------------------------------------
#endif

