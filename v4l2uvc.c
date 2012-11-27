#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/videodev.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include "v4l2uvc.h"
#include "picture_info.h"
#include "log_dbg.h"
#include "mediaEncode.h"
#include "audio_record.h"
#include "video_stream_lib.h"
#include "akjpeg.h"

#if defined IPED_98
#include <akuio/akuio.h>
#endif
static int debug = 0;
#define dbg(fmt, args...)  \
    do { \
        printf(__FILE__ ": %s: " fmt, __func__, ## args); \
    } while (0)

#define CLEAR(x) memset(&(x), 0, sizeof(x))
#define DEV_NAME      "/dev/video0"

#define VIDEO_BUFFER_BLOCK_SIZE 100*1024
#define CAMERA_REAL_FRAME_RATE		25

static void  *lock;
static int init_device(struct vdIn *vd);
static void uninit_device(struct vdIn *vd);

static int xioctl(int fh, int request, void *arg)
{
  int r;

  do {
    r = ioctl(fh, request, arg);
  } while (-1 == r && EINTR == errno);
  
  return r;
}

static unsigned char dht_data[DHT_SIZE] =
{
    0xff, 0xc4, 0x01, 0xa2, 0x00, 0x00, 0x01, 0x05, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02,
    0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x01, 0x00, 0x03,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09,
    0x0a, 0x0b, 0x10, 0x00, 0x02, 0x01, 0x03, 0x03, 0x02, 0x04, 0x03, 0x05,
    0x05, 0x04, 0x04, 0x00, 0x00, 0x01, 0x7d, 0x01, 0x02, 0x03, 0x00, 0x04,
    0x11, 0x05, 0x12, 0x21, 0x31, 0x41, 0x06, 0x13, 0x51, 0x61, 0x07, 0x22,
    0x71, 0x14, 0x32, 0x81, 0x91, 0xa1, 0x08, 0x23, 0x42, 0xb1, 0xc1, 0x15,
    0x52, 0xd1, 0xf0, 0x24, 0x33, 0x62, 0x72, 0x82, 0x09, 0x0a, 0x16, 0x17,
    0x18, 0x19, 0x1a, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x34, 0x35, 0x36,
    0x37, 0x38, 0x39, 0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a,
    0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x63, 0x64, 0x65, 0x66,
    0x67, 0x68, 0x69, 0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a,
    0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8a, 0x92, 0x93, 0x94, 0x95,
    0x96, 0x97, 0x98, 0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8,
    0xa9, 0xaa, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xc2,
    0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2, 0xd3, 0xd4, 0xd5,
    0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7,
    0xe8, 0xe9, 0xea, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9,
    0xfa, 0x11, 0x00, 0x02, 0x01, 0x02, 0x04, 0x04, 0x03, 0x04, 0x07, 0x05,
    0x04, 0x04, 0x00, 0x01, 0x02, 0x77, 0x00, 0x01, 0x02, 0x03, 0x11, 0x04,
    0x05, 0x21, 0x31, 0x06, 0x12, 0x41, 0x51, 0x07, 0x61, 0x71, 0x13, 0x22,
    0x32, 0x81, 0x08, 0x14, 0x42, 0x91, 0xa1, 0xb1, 0xc1, 0x09, 0x23, 0x33,
    0x52, 0xf0, 0x15, 0x62, 0x72, 0xd1, 0x0a, 0x16, 0x24, 0x34, 0xe1, 0x25,
    0xf1, 0x17, 0x18, 0x19, 0x1a, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x35, 0x36,
    0x37, 0x38, 0x39, 0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a,
    0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x63, 0x64, 0x65, 0x66,
    0x67, 0x68, 0x69, 0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a,
    0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8a, 0x92, 0x93, 0x94,
    0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
    0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba,
    0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2, 0xd3, 0xd4,
    0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7,
    0xe8, 0xe9, 0xea, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa
};

static int init_v4l2(struct vdIn *vd);

int
init_videoIn(struct vdIn *vd, char *device, int width, int height,
             int format, int grabmethod)
{

    if (vd == NULL || device == NULL)
        return -1;
    if (width == 0 || height == 0)
        return -1;
    if (grabmethod < 0 || grabmethod > 1)
        grabmethod = 1;        //mmap by default;
    vd->videodevice = NULL;
    vd->status = NULL;
    vd->pictName = NULL;
    vd->videodevice = (char *) calloc(1, 16 * sizeof(char));
    vd->status = (char *) calloc(1, 100 * sizeof(char));
    vd->pictName = (char *) calloc(1, 80 * sizeof(char));
    snprintf(vd->videodevice, 12, "%s", device);
    vd->toggleAvi = 0;
    vd->getPict = 0;
    vd->signalquit = 1;
    vd->width = width;
    vd->height = height;
    vd->formatIn = format;
    vd->grabmethod = grabmethod;
    if (init_v4l2(vd) < 0)
    {
        log_warning(" Init v4L2 failed !! exit fatal \n");
        goto error;;
    }
    /* alloc a temp buffer to reconstruct the pict */
#ifdef IPED_98
    vd->framesizeIn = vd->fmt.fmt.pix.sizeimage;
#else
    vd->framesizeIn = (vd->width * vd->height);
#endif
    switch (vd->formatIn)
    {
    case V4L2_PIX_FMT_MJPEG:
        vd->tmpbuffer = (unsigned char *) calloc(1, (size_t) vd->framesizeIn);
        if (!vd->tmpbuffer)
            goto error;
        vd->framebuffer =
            (unsigned char *) calloc(1, (size_t) vd->width * (vd->height + 8) * 2);
        break;
    case V4L2_PIX_FMT_YUYV:
        fprintf(stderr, "use V4L2_PIX_FMT_YUYV format framesize %d\n", vd->framesizeIn);
        vd->framebuffer = (unsigned char *) calloc(1, (size_t) vd->framesizeIn);
        break;
    default:
        fprintf(stderr, " should never arrive exit fatal !!\n");
        goto error;
        break;
    }
    if (!vd->framebuffer)
        goto error;
    return 0;
error:
    //sleep(20);
    free(vd->videodevice);
    free(vd->status);
    free(vd->pictName);
    close(vd->fd);
    return -1;
}

static int
init_v4l2(struct vdIn *vd)
{
    printf("#### %s use v4l2 format %s : %d, dev <%s>\n",
            __func__, "V4L2_PIX_FMT_YUYV", vd->formatIn, vd->videodevice);
    if ((vd->fd = open(vd->videodevice, O_RDWR)) == -1)
    {
        perror("ERROR opening V4L interface \n");
        exit(1);
    }
    printf("**********open video fd==%d**************\n", vd->fd);
 //   fcntl(vd->fd , F_SETFD , 1) ; //close on exec

#ifdef IPED_98 
    return init_device(vd);
#else
    int i;
    int ret = 0;


    memset(&vd->cap, 0, sizeof(struct v4l2_capability));
    ret = xioctl(vd->fd, VIDIOC_QUERYCAP, &vd->cap);
    if (ret < 0)
    {
        fprintf(stderr, "Error unable to query device %s. %s\n",
                vd->videodevice, strerror(errno));
        goto fatal;
    }
    printf("querycap sucess\n");
    if ((vd->cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) == 0)
    {
        fprintf(stderr, "Error video capture not supported.%s %s\n",
                vd->videodevice, strerror(errno));
        goto fatal;;
    }
    if (vd->grabmethod)
    {
        if (!(vd->cap.capabilities & V4L2_CAP_STREAMING))
        {
            fprintf(stderr, "%s does not support streaming i/o\n",
                    vd->videodevice);
            goto fatal;
        }
    }
    else
    {
        if (!(vd->cap.capabilities & V4L2_CAP_READWRITE))
        {
            fprintf(stderr, "%s does not support read i/o\n", vd->videodevice);
            goto fatal;
        }
    }
    /* set format in */
    memset(&vd->fmt, 0, sizeof(struct v4l2_format));
    vd->fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    vd->fmt.fmt.pix.width = vd->width;
    vd->fmt.fmt.pix.height = vd->height;
    vd->fmt.fmt.pix.pixelformat = vd->formatIn;
#if defined IPED_98
    vd->fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;
#else
    vd->fmt.fmt.pix.field = V4L2_FIELD_ANY;
#endif
    printf ("fmt pix w %d h %d\n", vd->width, vd->height);
    ret = xioctl(vd->fd, VIDIOC_S_FMT, &vd->fmt);
    if (ret < 0)
    {
        fprintf(stderr, "Unable to set format: %d %s\n" ,
                errno, strerror(errno));
        goto fatal;
    }
    printf("set formate sucess\n");
    if ((vd->fmt.fmt.pix.width != vd->width) ||
            (vd->fmt.fmt.pix.height != vd->height))
    {
        fprintf(stderr,
               "format asked unavailable get width %d height %d \n",
                vd->fmt.fmt.pix.width, vd->fmt.fmt.pix.height);
        vd->width = vd->fmt.fmt.pix.width;
        vd->height = vd->fmt.fmt.pix.height;
        /* look the format is not part of the deal ??? */
        //vd->formatIn = vd->fmt.fmt.pix.pixelformat;
    }
    /* request buffers */
    memset(&vd->rb, 0, sizeof(struct v4l2_requestbuffers));
    vd->rb.count = NB_BUFFER;
    vd->rb.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    vd->rb.memory = V4L2_MEMORY_MMAP;

    ret = xioctl(vd->fd, VIDIOC_REQBUFS, &vd->rb);
    if (ret < 0)
    {
        fprintf(stderr, "Unable to allocate buffers: %d.\n", errno);
        goto fatal;
    }
    printf("reqbufs sucess\n");
    /* map the buffers */
    for (i = 0; i < NB_BUFFER; i++)
    {
        memset(&vd->buf, 0, sizeof(struct v4l2_buffer));
        vd->buf.index = i;
        vd->buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        vd->buf.memory = V4L2_MEMORY_MMAP;
        ret = xioctl(vd->fd, VIDIOC_QUERYBUF, &vd->buf);
        if (ret < 0)
        {
            fprintf(stderr, "Unable to query buffer (%d).\n", errno);
            goto fatal;
        }
        if (debug)
            fprintf(stderr, "length: %u offset: %u\n", vd->buf.length,
                    vd->buf.m.offset);
        printf ("### v4l2 query buf %u length %d offset %u\n",
                              i, vd->buf.length, vd->buf.m.offset);
        vd->mem[i] = mmap(0 /* start anywhere */ ,
                          vd->buf.length, PROT_READ, MAP_SHARED, vd->fd,
                          vd->buf.m.offset);
        if (vd->mem[i] == MAP_FAILED)
        {
            fprintf(stderr, "Unable to map %d buffer (%d) %s\n",
                    i, errno, strerror(errno));
            goto fatal;
        }
        if (debug)
            fprintf(stderr, "Buffer mapped at address %p.\n", vd->mem[i]);
    }
    /* Queue the buffers. */
    for (i = 0; i < NB_BUFFER; ++i)
    {
        memset(&vd->buf, 0, sizeof(struct v4l2_buffer));
        vd->buf.index = i;
        vd->buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        vd->buf.memory = V4L2_MEMORY_MMAP;
        ret = xioctl(vd->fd, VIDIOC_QBUF, &vd->buf);
        if (ret < 0)
        {
            fprintf(stderr, "Unable to queue buffer (%d).\n", errno);
            goto fatal;;
        }
    }
    return 0;
fatal:
    return -1;
#endif

}

static int
video_enable(struct vdIn *vd)
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

static int
video_disable(struct vdIn *vd)
{
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    int ret;

    ret = xioctl(vd->fd, VIDIOC_STREAMOFF, &type);
    if (ret < 0)
    {
        fprintf(stderr, "Unable to %s capture: %d.\n", "stop", errno);
        return ret;
    }
    vd->isstreaming = 0;
    return 0;
}

static inline char * gettimestamp()
{
    static char timestamp[15];
    time_t t;
    struct tm *curtm;
    if (time(&t) == -1)
    {
        printf("get time error\n");
        exit(0);
    }
    curtm = localtime(&t);
    sprintf(timestamp, "%04d%02d%02d%02d%02d%02d", curtm->tm_year + 1900, curtm->tm_mon + 1,
            curtm->tm_mday, curtm->tm_hour, curtm->tm_min, curtm->tm_sec);
    return timestamp;
}

static picture_info_t p_info =
{
    {0, 0, 0, 1, 0xc},
};

static char* jpeg_buf = NULL;

int uvcGrab(struct vdIn *vd)
{
#define HEADERFRAME1 0xaf

    static int count_t = 0;
	static int count_last = 0;
    static unsigned long time_begin, time_current, time_last = 0;
	int frame_interval = ( 1000-100 ) / threadcfg.framerate;
	int time1;
	static int time2 = 0;
    if (count_t == 0)
    {
        time_begin = get_system_time_ms();
    }
#if 0
	static unsigned long time_stamp = 0;
    unsigned long now = get_system_time_ms();
    time_stamp += now - begin_t;
    begin_t = now;
	if (time_stamp < 10)
		time_stamp = 10;
#endif
    int ret;
    char *time;
	int status;
	unsigned int  psize;

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
    //printf("###############after xioctl VIDEOC_DQBUF#############\n");
    switch (vd->formatIn)
    {
    case V4L2_PIX_FMT_MJPEG:
        time = gettimestamp();
        memcpy(p_info.TimeStamp , time , sizeof(p_info.TimeStamp));
        memcpy(vd->tmpbuffer , &p_info , sizeof(picture_info_t));
        memcpy(vd->tmpbuffer + sizeof(picture_info_t), vd->mem[vd->buf.index], HEADERFRAME1);
        memcpy(vd->tmpbuffer + sizeof(picture_info_t) + HEADERFRAME1, dht_data, DHT_SIZE);
        memcpy(vd->tmpbuffer + sizeof(picture_info_t) + HEADERFRAME1 + DHT_SIZE,
               vd->mem[vd->buf.index] + HEADERFRAME1,
               (vd->buf.bytesused - HEADERFRAME1));
        if (debug)
            fprintf(stderr, "bytes in used %d \n", vd->buf.bytesused);
        break;
    case V4L2_PIX_FMT_YUYV:
//        printf("############ try to encode data bytesused %lu buffer index %d\n", vd->buf.bytesused, vd->buf.index);
		time_current = get_system_time_ms();
		if( ( time_current - time_begin ) >= 10 * 1000 ){
			printf("encode speed = %d, total_time=%d\n", ( count_t - count_last )/10,time2 );
			time_begin = time_current;
			count_last = count_t;
			time2 = 0;
		}
		status = check_monitor_queue_status();
		if( status != MONITOR_STATUS_NEED_NOTHING && ( time_current - time_last >= frame_interval ) ){
			time_last = time_current;
			clear_encode_temp_buffer(); //by chf: after encoding one frame, compressed data are stored in static temp buffer, we should take them later
			if( status == MONITOR_STATUS_NEED_I_FRAME || count_t  >= 10000 ){
				printf("after %d p frame, we need an I frame for some reasons\n", count_t);
				if( status == MONITOR_STATUS_NEED_I_FRAME ){
					//MediaRestartFast();
				}
				else{
					//MediaRestart(threadcfg.bitrate, 1280,720);
					//MediaRestartFast();
				}
				count_t = count_last = 0;
				time_begin = time_current = get_system_time_ms();
			}

//			if( -1 != processVideoData((void *)vd->buf.m.userptr, vd->buf.bytesused, 100 * count_t	/*time_stamp*/)){
			time1 = get_system_time_ms();
			if(-1 != encode_main((void *)vd->buf.m.userptr, vd->buf.bytesused)){
				time2+= get_system_time_ms() - time1;
				printf("encode video data count %d timestamp %lu ms\n", count_t, get_system_time_ms() - time1);
				char* buffer;
				int size;
				if( -1 != get_temp_buffer_data(&buffer,&size) ){
					printf("get a frame,count = %d, size=%d\n",count_t, size);
				//	if( write_monitor_packet_queue(buffer,size) == 0 ){
						count_t++;
					//}
					//else{
				//		printf("so strange, write_monitor_packet_queue error, what happened?????\n");
				//	}
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
			usleep(1000 / CAMERA_REAL_FRAME_RATE *1000);	//by chf: we have to sleep 1000/fps ms, to wait for next frame 
		}
		//printf("####### get capture :size %u framesize %u pointer %p\n",
							//vd->buf.bytesused, vd->framesizeIn, vd->mem[vd->buf.index]);
#if 0
        if (vd->buf.bytesused > vd->framesizeIn)
            memcpy(vd->framebuffer, vd->mem[vd->buf.index],
                   (size_t) vd->framesizeIn);
        else
            memcpy(vd->framebuffer, vd->mem[vd->buf.index],
                   (size_t) vd->buf.bytesused);
#endif
#if 0
		psize=1280*720*3/2;
		if( jpeg_buf == NULL ){
			jpeg_buf = malloc( 1280 * 720 * 3 / 2 + 8192 );
			time = gettimestamp();
			memcpy(p_info.TimeStamp , time , sizeof(p_info.TimeStamp));
			memcpy(jpeg_buf, &p_info , sizeof(picture_info_t));
		}
		status = check_monitor_queue_status();
		if( status != MONITOR_STATUS_NEED_NOTHING ){
			if( akjpeg_encode_yuv420((void *)vd->buf.m.userptr, jpeg_buf+sizeof(picture_info_t), (void *)&psize,1280, 720 ,60) != AK_FALSE ){
				printf("encode an jpeg file, size = %d\n", psize);
#if 0
				if( write_monitor_packet_queue(jpeg_buf,psize+sizeof(picture_info_t)) == 0 ){
					count_t++;
				}
				else{
					printf("so strange, write_monitor_packet_queue error, what happened?????\n");
				}
#endif
			}
		}

#endif

        break;
    default:
        goto err;
        break;
    }
    //printf("################before ioctl VIDEOC_QBUF 2###########\n");
    ret = xioctl(vd->fd, VIDIOC_QBUF, &vd->buf);
    if (ret < 0)
    {
        fprintf(stderr, "Unable to requeue buffer (%d).\n", errno);
        goto err;
    }
    //printf("################after ioctl VIDEOC_QBUF 2###########\n");
    memset(vd->hrb_tid, 0, sizeof(vd->hrb_tid));
    return 0;
err:
    vd->signalquit = 0;
    return -1;
}

int
close_v4l2(struct vdIn *vd)
{
    if (vd->isstreaming)
        video_disable(vd);

    /* If the memory maps are not released the device will remain opened even
       after a call to close(); */
#if 0
    int i;

    for (i = 0; i < NB_BUFFER; i++)
    {
        if (munmap(vd->mem[i], vd->buf.length) != 0)
        {
            log_warning("############error v4l2 munmap: %d#############\n" , errno);
        }
    }
#else
    uninit_device(vd);
#endif

    if (vd->tmpbuffer)
        free(vd->tmpbuffer);
    vd->tmpbuffer = NULL;
    free(vd->framebuffer);
    vd->framebuffer = NULL;
    free(vd->videodevice);
    free(vd->status);
    free(vd->pictName);
    vd->videodevice = NULL;
    vd->status = NULL;
    vd->pictName = NULL;
    if (close(vd->fd) != 0)
    {
        log_warning("###############error close v4l2:%d############## \n" , errno);
    }
    return 0;
}

/* return >= 0 ok otherwhise -1 */
static int
isv4l2Control(struct vdIn *vd, int control, struct v4l2_queryctrl *queryctrl)
{
    int err = 0;

    queryctrl->id = control;
    if ((err = xioctl(vd->fd, VIDIOC_QUERYCTRL, queryctrl)) < 0)
    {
        fprintf(stderr, "ioctl querycontrol error %d \n", errno);
    }
    else if (queryctrl->flags & V4L2_CTRL_FLAG_DISABLED)
    {
        fprintf(stderr, "control %s disabled \n", (char *) queryctrl->name);
    }
    else if (queryctrl->flags & V4L2_CTRL_TYPE_BOOLEAN)
    {
        return 1;
    }
    else if (queryctrl->type & V4L2_CTRL_TYPE_INTEGER)
    {
        return 0;
    }
    else
    {
        fprintf(stderr, "contol %s unsupported  \n", (char *) queryctrl->name);
    }
    return -1;
}

int
v4l2GetControl(struct vdIn *vd, int control)
{
    log_warning("control %d\n", control);
    struct v4l2_queryctrl queryctrl;
    struct v4l2_control control_s;
    int err;

    if (isv4l2Control(vd, control, &queryctrl) < 0)
        return -1;
    control_s.id = control;
    if ((err = xioctl(vd->fd, VIDIOC_G_CTRL, &control_s)) < 0)
    {
        fprintf(stderr, "ioctl get control error\n");
        return -1;
    }
    return control_s.value;
}

int
v4l2SetControl(struct vdIn *vd, int control, int value)
{
    struct v4l2_control control_s;
    struct v4l2_queryctrl queryctrl;
    int min, max, step, val_def;
    int err;
    log_warning("control %d\n", control);

    if (isv4l2Control(vd, control, &queryctrl) < 0)
        return -1;
    min = queryctrl.minimum;
    max = queryctrl.maximum;
    step = queryctrl.step;
    val_def = queryctrl.default_value;
    if (value < min)
        value = min;
    else if (value > max)
        value = max;
    if ((value >= min) && (value <= max))
    {
        control_s.id = control;
        control_s.value = value;
        if ((err = xioctl(vd->fd, VIDIOC_S_CTRL, &control_s)) < 0)
        {
            fprintf(stderr, "ioctl set control error\n");
            return -1;
        }
    }
    return 0;
}

int
v4l2ResetControl(struct vdIn *vd, int control)
{
    struct v4l2_control control_s;
    struct v4l2_queryctrl queryctrl;
    int val_def;
    int err;

    if (isv4l2Control(vd, control, &queryctrl) < 0)
        return -1;
    val_def = queryctrl.default_value;
    control_s.id = control;
    control_s.value = val_def;
    if ((err = xioctl(vd->fd, VIDIOC_S_CTRL, &control_s)) < 0)
    {
        fprintf(stderr, "ioctl reset control error\n");
        return -1;
    }

    return 0;
}

int v4l2_contrl_brightness(struct vdIn *vd, int brightness)
{
    struct v4l2_control control_s;
    struct v4l2_queryctrl queryctrl;
    int min, max, step, val_def;
    int err , value;

    if (brightness < 0 || brightness > 100)
        return -1;
    if (isv4l2Control(vd, V4L2_CID_BRIGHTNESS, &queryctrl) < 0)
        return -1;
    min = queryctrl.minimum;
    max = queryctrl.maximum;
    step = queryctrl.step;
    val_def = queryctrl.default_value;
    value = (int)(max * brightness / 100);
    if (value < min)
        value = min;
    else if (value > max)
        value = max;
    control_s.id = V4L2_CID_BRIGHTNESS;
    control_s.value = value;
    if ((err = xioctl(vd->fd, VIDIOC_S_CTRL, &control_s)) < 0)
    {
        fprintf(stderr, "ioctl set control error\n");
        return -1;
    }
    dbg("set brightness sucess\n");
    return 0;
}

int v4l2_contrl_contrast(struct vdIn *vd, int contrast)
{
    struct v4l2_control control_s;
    struct v4l2_queryctrl queryctrl;
    int min, max, step, val_def;
    int err , value;

    if (contrast < 0 || contrast > 100)
        return -1;
    if (isv4l2Control(vd, V4L2_CID_CONTRAST, &queryctrl) < 0)
        return -1;
    min = queryctrl.minimum;
    max = queryctrl.maximum;
    step = queryctrl.step;
    val_def = queryctrl.default_value;
    value = (int)(max * contrast / 100);
    if (value < min)
        value = min;
    else if (value > max)
        value = max;
    control_s.id = V4L2_CID_CONTRAST;
    control_s.value = value;
    if ((err = xioctl(vd->fd, VIDIOC_S_CTRL, &control_s)) < 0)
    {
        fprintf(stderr, "ioctl set control error\n");
        return -1;
    }
    dbg("set contrast sucess\n");
    return 0;
}

extern struct vdIn *vdin_camera;
void restart_v4l2(int width , int height)
{
    log_debug("cemera restart \n");
    const char *videodevice = "/dev/video0";
    int format = V4L2_PIX_FMT_MJPEG;
    int grabmethod = 1;
    int trygrab = 5;
    pthread_mutex_lock(&vdin_camera->tmpbufflock);
    close_v4l2(vdin_camera);
    if (init_videoIn(vdin_camera, (char *) videodevice, width, height, format, grabmethod) < 0)
    {
        printf("init camera device error\n");
        exit(0);
    }
    while (uvcGrab(vdin_camera) < 0)
    {
        trygrab--;
        if (trygrab <= 0)
            exit(0);
        printf("Error grabbing\n");
        usleep(100000);
    }
    pthread_mutex_unlock(&vdin_camera->tmpbufflock);
}

static int init_userp(struct vdIn *vd, unsigned int buffer_size)
{
	struct v4l2_requestbuffers req;
    int camera = vd->fd;
    int n_buffers;

	CLEAR(req);
	req.count  = NB_BUFFER;
	req.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory = V4L2_MEMORY_USERPTR;
	
	if (-1 == xioctl(camera, VIDIOC_REQBUFS, &req))
    {
		log_warning("xioctl(camera, VIDIOC_REQBUFS, &req) failed \n");
        return -1;
    }

	lock = akuio_lock_block_compatible(0);
	for (n_buffers = 0; n_buffers < NB_BUFFER; ++n_buffers) {
	    vd->mem[n_buffers] = akuio_alloc_pmem(buffer_size);

		if (!vd->mem[n_buffers]) {
      		log_warning("##### Out of memory\n");
            return -1;
    	}
  	}

    int i;
    for (i = 0; i < NB_BUFFER; ++i) {
        struct v4l2_buffer buf;

        CLEAR(buf);
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_USERPTR;
        buf.index = i;
        buf.m.userptr = (unsigned long)vd->mem[i];
        buf.length = buffer_size;

        if (-1 == xioctl(camera, VIDIOC_QBUF, &buf))
        {
            log_warning("xioctl(camera, VIDIOC_QBUF, &buf) failed\n");
            return -1;
        }
    }
    MediaEncodeMain(threadcfg.bitrate,
                    vd->width,
                    vd->height);
    return 0;
}

static int init_device(struct vdIn *vd)
{
	struct v4l2_capability cap;
	struct v4l2_cropcap cropcap;
	struct v4l2_crop crop;
	struct v4l2_format fmt;
	struct v4l2_control m_ctrl; 
	struct v4l2_streamparm parm;

	unsigned int min;
    int camera = vd->fd;

	printf("xioctl(camera, VIDIOC_QUERYCAP, &cap)!\n"); 
	if (-1 == xioctl(camera, VIDIOC_QUERYCAP, &cap)) {
      	log_warning("%s is no V4L2 device\n", DEV_NAME);
        return -1;
  	}
	printf("xioctl(camera, VIDIOC_QUERYCAP, &cap) succeedded!\n");
	
	if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
		log_warning("%s is no video capture device\n", DEV_NAME);
        return -1;
	}
	printf("\tQUERYCAP: Device is V4L2_CAP_VIDEO_CAPTURE\n");

	if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
		log_warning("%s does not support streaming i/o\n", DEV_NAME);
        return -1;
	}
	dbg("\tQUERYCAP: Device support V4L2_CAP_STREAMING\n");		
  /* Select video input, video standard and tune here. */
	CLEAR(cropcap);
	cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	dbg("xioctl(camera, VIDIOC_CROPCAP, &cropcap)!\n");
	if (0 == ioctl(camera, VIDIOC_CROPCAP, &cropcap)) {
		dbg("xioctl(camera, VIDIOC_CROPCAP, &cropcap) succeedded!\n");
		dbg("\tCROPCAP: bounds = %d\t%d\t%d\t%d\n", cropcap.bounds.left, 
						cropcap.bounds.top, cropcap.bounds.width, cropcap.bounds.height);	
		dbg("\tCROPCAP: defrect = %d\t%d\t%d\t%d\n", cropcap.defrect.left, 
						cropcap.defrect.top, cropcap.defrect.width, cropcap.defrect.height);				
    	crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		printf("#############################################################\n"
			   "################  widht %d height %d ###############\n", vd->width, vd->height);
    	crop.c.width = vd->width; /* reset to default */
    	crop.c.height= vd->height;
		dbg("xioctl(camera, VIDIOC_S_CROP, &crop)!\n");
		if (-1 == ioctl(camera, VIDIOC_S_CROP, &crop)) 
			log_warning("xioctl(camera, VIDIOC_S_CROP, &crop) failed\n");
  	}

  	CLEAR(fmt);
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width       = vd->width;
    fmt.fmt.pix.height      = vd->height;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    fmt.fmt.pix.field       = V4L2_FIELD_INTERLACED;
    dbg("xioctl(camera, VIDIOC_S_FMT, &fmt)!\n");

    if (-1 == xioctl(camera, VIDIOC_S_FMT, &fmt))
    {
        log_warning("xioctl(camera, VIDIOC_S_FMT, &fmt) failed\n");
        return -1;
    }
    printf("xioctl(camera, VIDIOC_S_FMT, &fmt) succeedded\n");

	vd->width = fmt.fmt.pix.width;
	vd->height = fmt.fmt.pix.height;
    fmt.fmt.pix.bytesperline = fmt.fmt.pix.width * 2;
  	min = fmt.fmt.pix.bytesperline * fmt.fmt.pix.height;
    printf("fmt.fmt.pix.bytesperline = %d\n", fmt.fmt.pix.bytesperline);

	/*set parm*/
	CLEAR(parm);
	parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	parm.parm.capture.timeperframe.numerator = 1; 
	parm.parm.capture.timeperframe.denominator = 10; 
	parm.parm.capture.capturemode = 0; 
	if (-1 == xioctl(camera, VIDIOC_S_PARM, &parm))
		printf("xioctl(fd, VIDIOC_S_PARM, &parm) failed\n");

#if 0
	/*set control parm*/		
	m_ctrl.id = V4L2_CID_BRIGHTNESS;
	m_ctrl.value = 100;
	if (-1 == xioctl(camera, VIDIOC_S_CTRL, &m_ctrl))
		printf("xioctl(fd, VIDIOC_S_CTRL, &parm) failed\n");
		
	m_ctrl.id = V4L2_CID_CONTRAST;
	m_ctrl.value = 21;
	if (-1 == xioctl(camera, VIDIOC_S_CTRL, &m_ctrl))
		printf("xioctl(fd, VIDIOC_S_CTRL, &parm) failed\n");
#endif

    fmt.fmt.pix.sizeimage = fmt.fmt.pix.bytesperline * fmt.fmt.pix.height;
	fmt.fmt.pix.sizeimage += 8192;
	fmt.fmt.pix.sizeimage = fmt.fmt.pix.sizeimage & (~(0xFFF));
	printf("fmt.fmt.pix.sizeimage = %u\n", fmt.fmt.pix.sizeimage);
    vd->fmt = fmt;
	return init_userp(vd, fmt.fmt.pix.sizeimage);
}

static void uninit_device(struct vdIn *vd)
{
	unsigned int i;
	for (i = 0; i < NB_BUFFER; ++i)
    {
		akuio_free_pmem(vd->mem[i]);
        vd->mem[i] = NULL;
    }
	akuio_unlock(lock);
}

int close_video_device()
{
	return close_v4l2(vdin_camera);	
}

