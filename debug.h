#ifndef __DEBUG_H
//#define DEBUG

#ifdef  DEBUG
#define debug(stuff...) printf(stuff)
#else
#define debug(fmt, args...) do{}while(0)
#endif 

#endif
