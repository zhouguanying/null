/*
 * Copyright 2004-2008 Freescale Semiconductor, Inc. All Rights Reserved.
 * 
 * Author Erik Anvik "Au-Zone Technologies, Inc."  All rights reserved.
 */

/*
 * The code contained herein is licensed under the GNU Lesser General 
 * Public License.  You may obtain a copy of the GNU Lesser General 
 * Public License Version 2.1 or later at the following locations:
 *
 * http://www.opensource.org/licenses/lgpl-license.html
 * http://www.gnu.org/copyleft/lgpl.html
 */

#ifndef __VPU__CAPTURE__H
#define __VPU__CAPTURE__H

#include <linux/types.h>
#include <linux/videodev2.h>

typedef enum {
	overlay_channel,
	capture_channel
}camera_channel;

struct camera_buf{
	struct v4l2_buffer buf;
	unsigned char* addr;
};


#define CAMERA_CHANNEL_BUFFER_NUM 5
int camera_open();
int camera_init_channel(camera_channel channel, int width, int height);
int camera_start_channel(camera_channel channel);
int camera_stop_channel(camera_channel channel);
struct camera_buf* camera_get_data(camera_channel channel);
int camera_finish_data(camera_channel channel, struct camera_buf *buf);
#endif				/* __VPU__CAPTURE__H */
