#ifndef __AUIDO_RECORD_H
#define __AUIDO_RECORD_H

extern int openSndPcm(void);
extern int getSndPcmData(T_U8 **pAudioData, T_U32 *pulAudioSize);
extern int closeSndPcm(void);


#endif
