
#include 		"PTZprotocol.h"
//------------------------------------------------------
int	RS485_Set_Protocol_Size = (sizeof(int)*5*2);

const 	int	RS485_Set_Protocol[] = {
     PELCO_D,    2400 ,  8,  1,  'n' ,
     PELCO_P,     9600,  8,  1,  'n' ,
};

//------------------------------------------------------

char		PELCO_D_COM_LIST[] = {
	UP,		0x00,0x08,0x00,0xff,		//上
	DOWN,	0x00,0x10,0x00,0xff,		//下	
	LEFT,	0x00,0x04,0xff,0x00,		//左
	RIGHT,	0x00,0x02,0xff,0x00,		//右
	
	ZOOM_SHORT,	0x00,0x20,0x00,0x00,		//变倍短
	ZOOM_LONG,	0x00,0x40,0x00,0x00,		//变倍长
	
	FOCUS_NEAR,	0x00,0x80,0x00,0x00,		//聚焦近
	FOCUS_FAR,		0x01,0x00,0x00,0x00,		//聚焦远
	APT_NARROW,	0x02,0x00,0x00,0x00,		//光圈小
	APT_WIDE,		0x04,0x00,0x00,0x00,		//光圈大

	LIGHT_OFF,		0x00,0x0b,0x00,0x01,		//灯光关
	LIGHT_ON,		0x00,0x09,0x00,0x01,		//灯光开

	GOTO_PRESET,	0x00,0x07,0x00,0x01,		//转至预置点001
	SET_PRESET,	0x00,0x03,0x00,0x01,		//设置预置点001
	DEL_PRESET,	0x00,0x05,0x00,0x01,		//删除预置点001

	STOP,	0x00,0x00,0x00,0x00,		//停命令

	GOTO_PRESET95,	0x00,0x07,0x00,95,		//转至预置点95
	SET_PRESET95,	0x00,0x03,0x00,95,		//设置预置点95
	DEL_PRESET95,	0x00,0x05,0x00,95,		//删除预置点95


};
	
int	PELCO_D_frame(char *buffer,int address, int command)
{
	int	i;
	char	check = 0;
	char *pCheck = buffer;
	char *temp = PELCO_D_COM_LIST;

	int	invalidcommand = -1 ;
	
	*buffer++ = 0xff;
	*buffer++ = address;	

	for ( i= 0; i < sizeof(PELCO_D_COM_LIST) / sizeof(char)/5; i++)
	{
		if(command == *temp) 	{
			temp++;
			*buffer++ = *temp++;
			*buffer++ = *temp++;
			*buffer++ = *temp++;
			*buffer++ = *temp++;
			
			invalidcommand = 0 ;
			break;
		}else{
			temp +=5;
		}
	}
	if(invalidcommand)	return 0 ;

	pCheck += 1;
	for(i=0; i<5; i++) {
		check += *pCheck++;
	}
	*buffer++ = check ;

	return 	7;
}


char		PELCO_P_COM_LIST[] = {
	UP,		0x00,0x08,0x00,0x30,		//上
	DOWN,	0x00,0x10,0x00,0x30,		//下	
	LEFT,	0x00,0x04,0x10,0x30,		//左
	RIGHT,	0x00,0x02,0x10,0x30,		//右
	
	ZOOM_SHORT,	0x00,0x40,0x00,0x00,		//变倍短
	ZOOM_LONG,	0x00,0x20,0x00,0x00,		//变倍长
	
	FOCUS_NEAR,	0x02,0x00,0x00,0x00,		//聚焦近
	FOCUS_FAR,		0x01,0x00,0x00,0x00,		//聚焦远
	APT_NARROW,	0x08,0x00,0x00,0x00,		//光圈小
	APT_WIDE,		0x04,0x00,0x00,0x00,		//光圈大

//	LIGHT_OFF,		0x00,0x0b,0x00,0x01,		//灯光关
//	LIGHT_ON,		0x00,0x09,0x00,0x01,		//灯光开

	GOTO_PRESET,	0x00,0x07,0x00,0x01,		//转至预置点001
	SET_PRESET,	0x00,0x03,0x00,0x01,		//设置预置点001
	DEL_PRESET,	0x00,0x05,0x00,0x01,		//删除预置点001

	STOP,	0x00,0x00,0x00,0x00,			//停命令

	AUTO_ON,	0x00,0x96,0x00,0x20,		//自动巡航
	AUTO_OFF,	0x00,0x99,0x00,0x20,		//关闭自动巡航

	GOTO_PRESET95,	0x00,0x07,0x00,95,		//转至预置点95
	SET_PRESET95,	0x00,0x03,0x00,95,		//设置预置点95
	DEL_PRESET95,	0x00,0x05,0x00,95,		//删除预置点95
};


int	PELCO_P_frame(char *buffer, int address, int command)
{
	int	i;
	char	check = 0;
	char *pCheck = buffer;
	char *temp = PELCO_P_COM_LIST;
	
	int	invalidcommand = -1 ;
	
	*buffer++ = 0xA0;
	*buffer++ = address;	

	for ( i= 0; i < sizeof(PELCO_D_COM_LIST) / sizeof(char)/5; i++)
	{
		if(command == *temp) 	{
			temp++;
			*buffer++ = *temp++;
			*buffer++ = *temp++;
			*buffer++ = *temp++;
			*buffer++ = *temp++;
			
			invalidcommand = 0 ;
			break;
		}else{
			temp +=5;
		}
	}
	if(invalidcommand)	return 0 ;

	*buffer++ = 0xAF ;


	#if	0
	pCheck += 1;
	check = 0;
	for(i=0; i<5; i++) {
		check ^= *pCheck++;
	}
	*buffer++ = check ;
	
	#else
	//pCheck += 1;
	check = 0;
	for(i=0; i<7; i++) {
		check ^= *pCheck++;
	}
	*buffer++ = check ;	
	#endif


	return 	8 ;
}


