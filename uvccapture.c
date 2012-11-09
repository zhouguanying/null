#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <time.h>
#include <linux/videodev.h>
#include <sys/ioctl.h>

#include "v4l2uvc.h"

static const char version[] = "test";
int run = 1;

struct vdIn * init_camera(void)
{
    char *videodevice = "/dev/video0";
    int format = V4L2_PIX_FMT_YUYV;
    int grabmethod = 1;
    int width = 640;
    int height = 480;
    int brightness = 0, contrast = 0, saturation = 0, gain = 0;
    struct vdIn *videoIn;

    videoIn = (struct vdIn *) calloc(1, sizeof(struct vdIn));
#if defined IPED_98
    format = V4L2_PIX_FMT_YUYV;
#endif

    pthread_mutex_init(&videoIn->tmpbufflock, NULL);
    pthread_mutex_init(&videoIn->vd_data_lock, NULL);
    memset(videoIn->hrb_tid, 0, sizeof(videoIn->hrb_tid));
#if 0
    //threadcfg.qvga_flag = 0;
    if (strncmp(threadcfg.record_resolution, "vga", 3) == 0)
    {
        printf("vga \n");
        width = 640;
        height = 480;
    }
    else if (strncmp(threadcfg.record_resolution, "qvga", 4) == 0)
    {
        //threadcfg.qvga_flag = 1;
        width = 320;
        height = 240;
        printf("qvga \n");
    }
    else
    {
        width = 1280;
        height = 720;
        printf("720p\n");
    }
#endif

    width = 1280;
    height = 720;
    brightness = threadcfg.brightness;
    contrast    = threadcfg.contrast;
    saturation = threadcfg.saturation;
    gain         = threadcfg.gain;

    if (init_videoIn(videoIn, (char *) videodevice, width, height, format, grabmethod) < 0)
    {
        printf("init camera device error\n");
        return (struct vdIn *) 0;
    }
#if 0
    //query_all_ctrl(videoIn->fd);
    v4l2ResetControl(videoIn, V4L2_CID_BRIGHTNESS);
    v4l2ResetControl(videoIn, V4L2_CID_CONTRAST);
    v4l2ResetControl(videoIn, V4L2_CID_SATURATION);
    v4l2ResetControl(videoIn, V4L2_CID_GAIN);

    //Setup Camera Parameters
    /*
    #define V4L2_CID_ROTATE  (V4L2_CID_BASE + 34)
    if(v4l2SetControl(videoIn, V4L2_CID_ROTATE, 180)<0)
        printf("########################v4l2 not support rotate#####################\n");
        */
    if (brightness != 0)
    {
        //v4l2SetControl (videoIn, V4L2_CID_BRIGHTNESS, brightness);
        v4l2_contrl_brightness(videoIn, brightness);
    }
    if (contrast != 0)
    {
        //v4l2SetControl (videoIn, V4L2_CID_CONTRAST, contrast);
        v4l2_contrl_contrast(videoIn, contrast);
    }
    if (saturation != 0)
    {
        v4l2SetControl(videoIn, V4L2_CID_SATURATION, saturation);
    }
    if (gain != 0)
    {
        v4l2SetControl(videoIn, V4L2_CID_GAIN, gain);
    }
#endif

    return videoIn;
}

