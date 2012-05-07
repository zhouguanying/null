#ifndef __RTP_H__
#define __RTP_H__
#ifdef __cplusplus
extern "C"
{
#endif
#include "inet_type.h"


#define AUDIO_SESS_PORT   			 5002
#define VIDEO_SESS_PORT 			 5004

int start_audio_and_video_session();
int videosess_add_dstaddr(uint32_t ip,uint16_t rtpport,uint16_t rtcpport);
int videosess_remove_dstaddr(uint32_t ip,uint16_t rtpport,uint16_t rtcpport);
int audiosess_add_dstaddr(uint32_t ip,uint16_t rtpport,uint16_t rtcpport);
int audiosess_remove_dstaddr(uint32_t ip,uint16_t rtpport,uint16_t rtcpport);
#ifdef __cplusplus
}
#endif
#endif
