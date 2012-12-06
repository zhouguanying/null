#ifndef _H_MEDIA_ENCODE_H__
#define _H_MEDIA_ENCODE_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdarg.h>

#include <fcntl.h>              /* low-level i/o */
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include <linux/videodev2.h>

#include <akuio/akuio.h>

#include <akmedialib/anyka_types.h>
#include <akmedialib/media_demuxer_lib.h>
#include <akmedialib/medialib_global.h>
#include <akmedialib/sdfilter.h>
#include <akmedialib/video_stream_lib.h>

#include <akmedialib/media_recorder_lib.h>
#include <akmedialib/medialib_struct.h>
#include <akmedialib/sdcodec.h>
#include <akmedialib/media_player_lib.h>

#define ENCODE_USING_MEDIA_LIB	0

int MediaDestroy();
int processVideoData(T_U8* dataPtr, int datasize,int32_t timeStamp);
int processAudioData(T_U8* dataPtr, int datasize);
unsigned long get_system_time_ms(void);
int MediaEncodeMain(T_U32 nvbps, int w, int h);
int get_encode_video_buffer(unsigned char *buffer, int size);
int write_encode_video_buffer(unsigned char *buffer, int size);
void clear_encode_video_buffer(void);
int get_encode_video_buffer_valid_size(void);
int MediaRestart(unsigned int nvbps, int w, int h);
int MediaRestartFast(void);
void encode_need_i_frame(void);
int encode_main(char* yuv_buf, int size);

//by chf: temp_buffer store the encoded-one-frame-date at once. should be clear before encoding every time.
void clear_encode_temp_buffer(void);
int get_temp_buffer_data(char** buffer, int* size);
int closeMedia(void);


#endif
