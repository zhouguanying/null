#include <linux/videodev2.h>
#include "cli.h"

#define NB_BUFFER 4
#define DHT_SIZE 420

#if defined IPED_233
#define V4L2_CID_BACKLIGHT_COMPENSATION    (V4L2_CID_PRIVATE_BASE+0)
#define V4L2_CID_POWER_LINE_FREQUENCY    (V4L2_CID_PRIVATE_BASE+1)
#define V4L2_CID_SHARPNESS        (V4L2_CID_PRIVATE_BASE+2)
#define V4L2_CID_HUE_AUTO        (V4L2_CID_PRIVATE_BASE+3)
#define V4L2_CID_FOCUS_AUTO        (V4L2_CID_PRIVATE_BASE+4)
#define V4L2_CID_FOCUS_ABSOLUTE        (V4L2_CID_PRIVATE_BASE+5)
#define V4L2_CID_FOCUS_RELATIVE        (V4L2_CID_PRIVATE_BASE+6)

#define V4L2_CID_PANTILT_RELATIVE    (V4L2_CID_PRIVATE_BASE+7)
#define V4L2_CID_PANTILT_RESET        (V4L2_CID_PRIVATE_BASE+8)
#endif

struct vdIn
{
    pthread_mutex_t vd_data_lock;
    int fd;
    char *videodevice;
    char *status;
    char *pictName;
    struct v4l2_capability cap;
    struct v4l2_format fmt;
    struct v4l2_buffer buf;
    struct v4l2_requestbuffers rb;
    void *mem[NB_BUFFER];
    pthread_mutex_t tmpbufflock;
    pthread_t hrb_tid[MAX_CONNECTIONS];
    unsigned char *tmpbuffer;
    unsigned char *framebuffer;
    int isstreaming;
    int grabmethod;
    int width;
    int height;
    int formatIn;
    int formatOut;
    int framesizeIn;
    int signalquit;
    int toggleAvi;
    int getPict;

};

int init_videoIn(struct vdIn *vd, char *device, int width, int height,
             int format, int grabmethod);
int uvcGrab(struct vdIn *vd);
int close_v4l2(struct vdIn *vd);

int v4l2GetControl(struct vdIn *vd, int control);
int v4l2SetControl(struct vdIn *vd, int control, int value);
int v4l2ResetControl(struct vdIn *vd, int control);
int v4l2_contrl_brightness(struct vdIn *vd, int brightness);
int v4l2_contrl_contrast(struct vdIn *vd, int contrast);
int v4l2_contrl_exposure(struct vdIn *vd, int exposure);
void start_monitor_capture(void);
void stop_monitor_capture(void);
int close_video_device(void);
