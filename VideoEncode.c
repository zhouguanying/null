#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <unistd.h>
#include <pthread.h>
#include <sched.h>
#include <string.h>
#include <memory.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/shm.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <fcntl.h>
#include <linux/fs.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include "v4l2uvc.h"
#include "picture_info.h"
#include "log_dbg.h"
#include "mediaEncode.h"
#include "audio_record.h"
#include "video_stream_lib.h"
#include "akjpeg.h"

static picture_info_ex_t p_info_ex =
{
    {0, 0, 0, 1, 0xc},
};
static char* jpeg_buf = NULL;
static struct vdIn *vd = NULL;

static void sig_handler(int signum)
{
    if (signum == SIGINT || signum == SIGIO || signum == SIGSEGV){
		printf("SIGINT or SIGIO or SIGSEGV: %d,exit\n", signum);
		if( vd )
			close_v4l2(vd);
		exit(-1);
	}
    else if (signum == SIGPIPE)
        printf("SIGPIPE\n");
	else{
		printf("signal = %d\n", signum );
	}
}

int set_raw_config_value(char * buffer)
{
	return 0;
}

int set_system_time(char * time)
{
	return 0;
}

static char * gettimestamp_ex()
{
    static char timestamp[18];
    time_t t;
    struct tm *curtm;
	struct timeval now;
	int ms;
	static int last_sec = 0, last_ms = 0;
	int sec;

    if (time(&t) == -1)
    {
        printf("get time error\n");
        exit(0);
    }
    curtm = localtime(&t);
	gettimeofday(&now, NULL);
	ms = now.tv_usec / 1000;
	sec = curtm->tm_sec;
	if( ms < last_ms ){
		if( sec == last_sec ){	// ms turnround,but second may not turnround
			sec++;
		}
	}
    sprintf(timestamp, "%04d%02d%02d%02d%02d%02d%04d", curtm->tm_year + 1900, curtm->tm_mon + 1,
            curtm->tm_mday, curtm->tm_hour, curtm->tm_min, sec,ms);
	last_sec = curtm->tm_sec;
	last_ms = ms;
    return timestamp;
}

static int xioctl(int fh, int request, void *arg)
{
  int r;

  do {
    r = ioctl(fh, request, arg);
  } while (-1 == r && EINTR == errno);
  
  return r;
}

static int video_enable(struct vdIn *vd)
{
    printf("##### %s \n", __func__);
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    int ret;

    ret = xioctl(vd->fd, VIDIOC_STREAMON, &type);
    if (ret < 0)
    {
        fprintf(stderr, "Unable to %s capture: %d.\n", "start", errno);
        return ret;
    }
    vd->isstreaming = 1;
    return 0;
}


int main(int argc, char* argv[])
{
	int width, height;
	int i;
	FILE* file;

	if( argc < 2 ){
		printf("usage: encoder qvga\n");
		return 0;
	}

	file = fopen("/tmp/yuv", "wb");
	if( file == NULL ){
		printf("can not open temp file\n");
		return 0;
	}

    for (i = 1; i <= _NSIG; i++)
    {
        if (i == SIGIO || i == SIGINT)
            sigset(i, sig_handler);
        else
            //sigignore(i);
            sigset(i, sig_handler);
    }

	sprintf(threadcfg.record_resolution, "%s", argv[1]);
	printf("threadcfg.record_resolution = %s\n", threadcfg.record_resolution);
	
	threadcfg.brightness = 
	threadcfg.contrast = 
	threadcfg.saturation =
	threadcfg.gain = 50;

	if( !strncmp( argv[1], "qvga", 4 )){
		width = 352;
		height = 288;
	}
	else if( !strncmp( argv[1],"vga", 3 )){
		width = 640;
		height = 480;
	}
	else{		//default 720p
		width = 1280;
		height = 720;
	}

	printf("start init\n");
	
	akjpeg_init_without_lock();
	akjpeg_set_task_func();
	
	if ((vd = (struct vdIn *)init_camera()) == NULL)
	{
		printf("init camera error\n");
		return -1;
	}
	printf("start encoder\n");
	while(1)
	{
		static int count_t = 0;
		static int count_last = 0;
		static unsigned long time_begin, time_current, time_last = 0;
		int frame_interval = ( 1000-100 ) / 10;
		int ret;
		char *time;
		int status;
		unsigned int  psize;
		
		if (count_t == 0)
		{
			time_begin = get_system_time_ms();
		}

		if( count_t >= 6 ){
			break;
		}
	
		if (!vd->isstreaming)
			if (video_enable(vd))
				goto err;
		memset(&vd->buf, 0, sizeof(struct v4l2_buffer));
		vd->buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		vd->buf.memory = V4L2_MEMORY_USERPTR;
		//printf("###############before ioctl VIDEOC_DQBUF#############\n");
		ret = ioctl(vd->fd, VIDIOC_DQBUF, &vd->buf);
		if (ret < 0)
		{
			log_warning("Unable to dequeue buffer (%d). %s\n", errno, strerror(errno));
			goto err;
		}
		time = gettimestamp_ex();
		memcpy(p_info_ex.TimeStamp , time , sizeof(p_info_ex.TimeStamp));
		//printf("###############after xioctl VIDEOC_DQBUF#############\n");
		switch (vd->formatIn)
		{
			case V4L2_PIX_FMT_MJPEG:
				printf("*************************************unsupported format*************************************\n");
				break;
			case V4L2_PIX_FMT_YUYV:
		  	  //printf("############ try to encode data bytesused %lu buffer index %d\n", vd->buf.bytesused, vd->buf.index);
			  fwrite(vd->buf.m.userptr, 1, width*height*3/2, file);
				time_current = get_system_time_ms();
				if( ( time_current - time_begin ) >= 10 * 1000 ){
					printf("encode speed = %d\n", ( count_t - count_last )/10 );
					time_begin = time_current;
					count_last = count_t;
				}
#if 1
				psize=1280*720*3/2;
				if( jpeg_buf == NULL ){
					jpeg_buf = malloc( 1280 * 720 * 3 / 2 + 8192 );
				}
				if( count_t % 12 == 0 ){
					//status = check_monitor_queue_status();
					status = MONITOR_STATUS_NEED_ANY;
					if( status == MONITOR_STATUS_NEED_ANY && ( time_current - time_last >= frame_interval )){
						time_last = time_current;
						if( akjpeg_encode_yuv420((void *)vd->buf.m.userptr, jpeg_buf+sizeof(picture_info_ex_t), (void *)&psize,vd->width, vd->height,60) != AK_FALSE ){
							printf("encode an jpeg file, size = %d\n", psize);
							memcpy(jpeg_buf, &p_info_ex , sizeof(picture_info_ex_t));
#if 0
							if( write_monitor_packet_queue(jpeg_buf,psize+sizeof(picture_info_ex_t)) == 0 ){
								count_t++;
							}
							else{
								printf("so strange, write_monitor_packet_queue error, what happened?????\n");
							}
#else
							count_t++;
#endif
						}
						break;
					}
				}
#endif
		
				//status = check_monitor_queue_status();
				status = MONITOR_STATUS_NEED_ANY;
				if( status != MONITOR_STATUS_NEED_NOTHING && ( time_current - time_last >= frame_interval ) ){
					time_last = time_current;
					clear_encode_temp_buffer(); //by chf: after encoding one frame, compressed data are stored in static temp buffer, we should take them later
					if( status == MONITOR_STATUS_NEED_I_FRAME ){
						printf("after %d p frame, we need an I frame for some reasons\n", count_t);
						encode_need_i_frame();
						count_t = count_last = 0;
						time_begin = time_current = get_system_time_ms();
					}
		
					//time1 = get_system_time_ms();
					if(-1 != encode_main((void *)vd->buf.m.userptr, vd->buf.bytesused))
					{
						//printf("encode video data count %d timestamp %lu ms\n", count_t, get_system_time_ms() - time1);
						char* buffer;
						int size;
						if( -1 != get_temp_buffer_data(&buffer,&size) ){
							printf("get a frame,count = %d, size=%d\n",count_t, size);
							memcpy(buffer, &p_info_ex , sizeof(picture_info_ex_t));
#if 0
							if( write_monitor_packet_queue(buffer,size) == 0 ){
								count_t++;
							}
							else{
								printf("so strange, write_monitor_packet_queue error, what happened?????\n");
							}
#else
							count_t++;
#endif
						}
						else{
							printf("get temp buffer data error, so strange\n");
						}
					}
					else{
						printf("encode error\n");
					}
				}
				else{
					usleep(10); //by chf: sleep a little, for next frame arrive 
				}
				//printf("####### get capture :size %u framesize %u pointer %p\n",
									//vd->buf.bytesused, vd->framesizeIn, vd->mem[vd->buf.index]);
				break;
			default:
				goto err;
				break;
		}
		ret = xioctl(vd->fd, VIDIOC_QBUF, &vd->buf);
		if (ret < 0)
		{
			fprintf(stderr, "Unable to requeue buffer (%d).\n", errno);
			goto err;
		}
	}

	fclose(file);
	close_v4l2(vd);
	return 0;

err:
	printf("error exit encoder process\n");
	return -1;
}
