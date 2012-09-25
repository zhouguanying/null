#ifndef ENCODER_H
#define ENCODER_H

#include "vpu_io.h"
#include "vpu_lib.h"
#include "includes.h"
#include "defines.h"
#include "config.h"

#define	STREAM_BUF_SIZE 0xA0000L
#define NUM_FRAME_BUF   (1+17+2)
#define MAX_FRAME       (16+2+4)

typedef struct {
	int Index;
#if 1
	int AddrY;
	int AddrCb;
	int AddrCr;
	int StrideY;
	int StrideC; /* Stride Cb/Cr */
	int DispY; /* DispY is the page aligned AddrY */
	int DispCb; /* DispCb is the page aligned AddrCb */
	int DispCr;
#endif
	vpu_mem_desc CurrImage;	/* Current memory descriptor for user space */
} FRAME_BUF;

struct vpu_config {
	u32 index;
	u32 src;
	char src_name[80];
	u32 dst;
	u32 dst_name[80];
	u32 enc_flag;
	u32 fps;
	u32 bps;
	u32 mode;
	u32 width;
	u32 height;
	u32 gop;
	u32 frame_count;
	u32 rot_angle;
	u32 out_ratio;
	u32 mirror_angle;
};

/**
 * vpu_SystemInit - Init VPU hardware
 * Returns 0 on success or -1 on error
 */
int vpu_SystemInit(void);

/**
 * vpu_StartSession - Starts new video capture session
 * @params: VPU configuration parameters
 * Returns 0 on success or -1 on error
 */
int vpu_StartSession(struct video_config_params *params);

/**
 * vpu_ExitSession - Terminates video session
 * Returns 0 always
 */
int vpu_ExitSession(void);

/**
 * vpu_RemoveCallback - Removes video handler
 * @sess: cookie passed back when callback is executed
 * @cb: callback method
 * Returns 0 on success or -1 on error
 */
int vpu_RemoveCallback(void *sess, void *cb);

/**
 * vpu_AddCallback - Installs video handler for further data processing
 * @sess: cookie passed back when callback is executed
 * @cb: callback method
 * Returns 0 on success or -1 on error
 */
int vpu_AddCallback(void *sess, void *cb);

/**
 * vpu_ResyncVideo - Force I-Frame to be sent 
 * @sess: unused
 * Returns 0 on success or -1 on error
 */
int vpu_ResyncVideo(void *sess);

/**
 * vpu_EnableImageRotation - Enable image rotation
 * @sess: unused
 * Returns 0 on success or -1 on error
 */
int vpu_EnableImageRotation(void *sess);

/**
 * vpu_DisableImageRotation - Disable image rotation
 * @sess: unused
 * Returns 0 on success or -1 on error
 */
int vpu_DisableImageRotation(void *sess);

/**
 * vpu_SetImageRotationAngle - Set image rotation angle
 * @sess: unused
 * @angle: rotation angle
 * Returns 0 on success or -1 on error
 */
int vpu_SetImageRotationAngle(void *sess, int angle);

/**
 * vpu_EnableImageMirroring - Enable image mirroring
 * @sess: unused
 * Returns 0 on success or -1 on error
 */
int vpu_EnableImageMirroring(void *sess);

/**
 * vpu_DisableImageMirroring - Disable image mirroring
 * @sess: unused
 * Returns 0 on success or -1 on error
 */
int vpu_DisableImageMirroring(void *sess);

/**
 * vpu_SetImageMirrorDirection - Set image mirror direction
 * @sess: unused
 * Returns 0 on success or -1 on error
 */
int vpu_SetImageMirrorDirection(void *sess, int direction);

/**
 * vpu_EnableMotionDetection - Enable motion detection
 * @sess: unused
 * Returns 0 on success or -1 on error
 */
int vpu_EnableMotionDetection(void *sess);

/**
 * vpu_DisableMotionDetection - Disable motion detection
 * @sess: unused
 * Returns 0 on success or -1 on error
 */
int vpu_DisableMotionDetection(void *sess);

/**
 * vpu_GetMotionDetection - Get motion detection state
 * @sess: unused
 * Returns 0 on success or -1 on error
 */
int vpu_GetMotionDetection(void *sess);

#endif /* ENCODER_H */
