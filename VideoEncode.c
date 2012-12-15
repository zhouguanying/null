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
#include "vpu_server.h"

static picture_info_ex_t p_info_ex =
{
    {0, 0, 0, 1, 0xc},
};
static char* jpeg_buf = NULL;
static struct vdIn *vd = NULL;

static int encoder_shm_id;
static encoder_share_mem* encoder_shm_addr;

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

	printf("start encoder process\n");

#if 0
	if( argc < 2 ){
		printf("usage: encoder qvga\n");
		return 0;
	}
#endif

#ifdef WRITE_YUV_OUT
	file = fopen("/tmp/yuv", "wb");
	if( file == NULL ){
		printf("can not open temp file\n");
		return 0;
	}
#endif

    for (i = 1; i <= _NSIG; i++)
    {
        if (i == SIGIO || i == SIGINT)
            sigset(i, sig_handler);
        else
            //sigignore(i);
            sigset(i, sig_handler);
    }


    if ((encoder_shm_id = shmget(ENCODER_SHM_KEY , ENCODER_SHM_SIZE , 0666)) < 0)
    {
        perror("shmget : open");
        exit(0);
    }
    if ((encoder_shm_addr = (encoder_share_mem *)shmat(encoder_shm_id , 0 , 0)) < 0)
    {
        perror("shmat :");
        exit(0);
    }
	encoder_shm_addr->data_encoder = (char*)((int)encoder_shm_addr + sizeof(encoder_share_mem));

#if 0
	sprintf(threadcfg.record_resolution, "%s", argv[1]);
	printf("threadcfg.record_resolution = %s\n", threadcfg.record_resolution);
#endif

	width = encoder_shm_addr->width;
	height = encoder_shm_addr->height;

    threadcfg.brightness = encoder_shm_addr->brightness;
    threadcfg.contrast = encoder_shm_addr->contrast;
    threadcfg.saturation = encoder_shm_addr->saturation;
    threadcfg.gain = encoder_shm_addr->gain;
	memset(threadcfg.record_resolution,0,sizeof(threadcfg.record_resolution));
	if(encoder_shm_addr->width == 352){
		sprintf(threadcfg.record_resolution,"%s","qvga");
	}
	else if( encoder_shm_addr->width == 640 ){
		sprintf(threadcfg.record_resolution,"%s","vga");
	}
	else{
		sprintf(threadcfg.record_resolution,"%s","720p");
	}

	printf("start init\n");
	
	akjpeg_init_without_lock();
	akjpeg_set_task_func();

	if ((vd = (struct vdIn *)init_camera()) == NULL)
	{
		printf("init camera error\n");
		return -1;
	}
	clear_encode_temp_buffer();

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

		if( encoder_shm_addr->exit ){
			encoder_shm_addr->exit = 0;
			shmdt(encoder_shm_addr);
			close_video_device();
			exit(0);
		}
		
		if (count_t == 0)
		{
			time_begin = get_system_time_ms();
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
#ifdef WRITE_YUV_OUT
				fwrite(vd->buf.m.userptr, 1, width*height*3/2, file);
#endif
				time_current = get_system_time_ms();
				if( ( time_current - time_begin ) >= 10 * 1000 ){
					printf("encode speed = %d\n", ( count_t - count_last )/10 );
					time_begin = time_current;
					count_last = count_t;
				}
#if 0
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
				if( encoder_shm_addr->state == ENCODER_STATE_WAIT_FINISH){
					status = encoder_shm_addr->next_frame_type;
				}
				else{
					status = ENCODER_FRAME_TYPE_NONE;
				}
				if( status != ENCODER_FRAME_TYPE_NONE && ( time_current - time_last >= frame_interval ) ){
					time_last = time_current;
					if( status == ENCODER_FRAME_TYPE_I ){
						printf("after %d p frame, we need an I frame for some reasons\n", count_t);
						encode_need_i_frame();
						count_t = count_last = 0;
						time_begin = time_current = get_system_time_ms();
					}
		
					//time1 = get_system_time_ms();
again:
					if(-1 != encode_main((void *)vd->buf.m.userptr, vd->buf.bytesused))
					{
						//printf("encode video data count %d timestamp %lu ms\n", count_t, get_system_time_ms() - time1);
						char* buffer;
						int size;

						if( encoder_shm_addr->force_I_frame ){
							encoder_shm_addr->force_I_frame = 0;
							encode_need_i_frame();
							goto again;
						}

						if( -1 != get_temp_buffer_data(&buffer,&size) ){
							printf("get a frame,count = %d, size=%d\n",count_t, size);
							memcpy(buffer, &p_info_ex , sizeof(picture_info_ex_t));
							memcpy(encoder_shm_addr->data_encoder, buffer, size);
							encoder_shm_addr->data_size = size;
							encoder_shm_addr->state = ENCODER_STATE_FINISHED;
							count_t++;
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
					usleep(1*1000); //by chf: sleep a little, for next frame arrive 
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

#ifdef WRITE_YUV_OUT
	fclose(file);
#endif
	close_v4l2(vd);
	return 0;

err:
	printf("error exit encoder process\n");
	return -1;
}
