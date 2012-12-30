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

struct vdIn * init_camera(void);

static void sig_handler_encoder(int signum)
{
    if (signum == SIGINT || signum == SIGIO || signum == SIGSEGV){
		printf("encoder got a SIGINT or SIGIO or SIGSEGV: %d,exit\n", signum);
		if( vd )
			close_v4l2(vd);
		system("touch /tmp/encoder_exited");
		exit(-1);
	}
    else if (signum == SIGPIPE)
        printf("encoder got SIGPIPE\n");
	else{
		printf("encoder got signal = %d\n", signum );
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

	printf("************************start encoder process************************\n");

    for (i = 1; i <= _NSIG; i++)
    {
        if (i == SIGIO || i == SIGINT)
            sigset(i, sig_handler_encoder);
        else
            //sigignore(i);
            sigset(i, sig_handler_encoder);
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

	width = encoder_shm_addr->width;
	height = encoder_shm_addr->height;

    threadcfg.brightness = encoder_shm_addr->brightness;
    threadcfg.contrast = encoder_shm_addr->contrast;
    threadcfg.saturation = encoder_shm_addr->saturation;
    threadcfg.gain = encoder_shm_addr->gain;
    threadcfg.record_quality = encoder_shm_addr->record_quality;
	
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

	akjpeg_init_without_lock();
	akjpeg_set_task_func();

	if ((vd = (struct vdIn *)init_camera()) == NULL)
	{
		printf("init camera error\n");
		return -1;
	}
	clear_encode_temp_buffer();

	v4l2_contrl_exposure(vd, encoder_shm_addr->exposure);

	while(1)
	{
		static int count_t = 0;
		static int count_last = 0;
		static int count_frame = 0;
		static int count_last_frame = 0;
		static long long time_begin, time_current, time_last = 0;
		int frame_interval = ( 1000-100 ) / encoder_shm_addr->frame_rate;
		int ret;
		char *time;
		int status;

		if( encoder_shm_addr->exit ){
			encoder_shm_addr->exit = 0;
			shmdt(encoder_shm_addr);
			close_video_device();
			printf("encoder exit for main process's cmd\n");
			system("touch /tmp/encoder_exited");
			exit(0);
		}

		if( encoder_shm_addr->para_changed ){
			encoder_shm_addr->para_changed = 0;
			threadcfg.record_quality = encoder_shm_addr->record_quality;
			if( threadcfg.brightness != encoder_shm_addr->brightness ){
				threadcfg.brightness = encoder_shm_addr->brightness;
				v4l2_contrl_brightness(vd, threadcfg.brightness);
			}
			if( threadcfg.contrast != encoder_shm_addr->contrast ){
				threadcfg.contrast = encoder_shm_addr->contrast;
				v4l2_contrl_contrast(vd, threadcfg.contrast);
			}
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
		count_frame++;
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
			case V4L2_PIX_FMT_YUYV:
		  	  //printf("############ try to encode data bytesused %lu buffer index %d\n", vd->buf.bytesused, vd->buf.index);
				time_current = get_system_time_ms();
				if( ( time_current - time_begin ) >= 10 * 1000 ){
					printf("encode speed = %d, frame_rate=%d\n", ( count_t - count_last )/10, (count_frame-count_last_frame)/10);
					time_begin = time_current;
					count_last = count_t;
					count_last_frame = count_frame;
				}
				
				if( encoder_shm_addr->state == ENCODER_STATE_WAIT_FINISH){
					status = encoder_shm_addr->next_frame_type;
				}
				else{
					status = ENCODER_FRAME_TYPE_NONE;
				}

				if( status == ENCODER_FRAME_TYPE_JPEG ){
					int psize = 1280*720*3/2;
					time_last = time_current;
					if( jpeg_buf == NULL ){
						jpeg_buf = malloc( 1280 * 720 * 3 / 2 + 8192 );
					}
					if( akjpeg_encode_yuv420((void *)vd->buf.m.userptr, jpeg_buf+sizeof(picture_info_ex_t), (void *)&psize,
							encoder_shm_addr->width, encoder_shm_addr->height,60) != AK_FALSE )
					{
						printf("encode a jpeg file, size = %d\n", psize);
						if( psize > ENCODER_SHM_SIZE - sizeof(encoder_share_mem)){
							printf("***************************jpeg size is too large***************************\n");
							encoder_shm_addr->data_size = 0;
							encoder_shm_addr->state = ENCODER_STATE_FINISHED;
						}
						else{
							memcpy(jpeg_buf, &p_info_ex , sizeof(picture_info_ex_t));
							memcpy(encoder_shm_addr->data_encoder, jpeg_buf, psize + sizeof(picture_info_ex_t));
							count_t++;
							encoder_shm_addr->data_size = psize + sizeof(picture_info_ex_t);
							encoder_shm_addr->state = ENCODER_STATE_FINISHED;
						}
					}
				}
				else if( status != ENCODER_FRAME_TYPE_NONE && ( time_current - time_last >= frame_interval ) ){
					time_last = time_current;
					if( status == ENCODER_FRAME_TYPE_I ){
						//printf("after %d p frame, we need an I frame for some reasons\n", count_t);
						encode_need_i_frame();
						//count_t = count_last = 0;
						//time_begin = time_current = get_system_time_ms();
					}
		
again:
					if(-1 != encode_main((void *)vd->buf.m.userptr, vd->buf.bytesused))
					{
						//printf("encode video data count %d timestamp %lu ms\n", count_t, get_system_time_ms() - time1);
						char* buffer;
						int size;

						if( encoder_shm_addr->force_I_frame ){
							encoder_shm_addr->force_I_frame = 0;
							encoder_shm_addr->next_frame_type = ENCODER_FRAME_TYPE_I;
							printf("encoder force an I frame by main process\n");
							encode_need_i_frame();
							goto again;
						}

						if( -1 != get_temp_buffer_data(&buffer,&size) ){
							if( encoder_shm_addr->next_frame_type == ENCODER_FRAME_TYPE_I )
								printf("*******************************get an I frame,count = %d, size=%d\n",count_t, size);
							else 
								printf("get a %d frame,count = %d, size=%d\n",encoder_shm_addr->next_frame_type, count_t, size);
								
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

	close_v4l2(vd);
	return 0;

err:
	printf("error exit encoder process\n");
	return -1;
}
