/*
 * Copyright 2004-2008 Freescale Semiconductor, Inc. All Rights Reserved.
 *
 * Copyright (c) 2006, Chips & Media.  All rights reserved.
 * 
 * Author Erik Anvik "Au-Zone Technologies, Inc."  All rights reserved.
 * based on sampled Chips & Media demo application code base
 */

/*
 * The code contained herein is licensed under the GNU Lesser General 
 * Public License.  You may obtain a copy of the GNU Lesser General 
 * Public License Version 2.1 or later at the following locations:
 *
 * http://www.opensource.org/licenses/lgpl-license.html
 * http://www.gnu.org/copyleft/lgpl.html
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>

#include <errno.h>
#include <linux/videodev.h>
#include <string.h>
#include <malloc.h>

#include "capture.h"

static int camera_fd;
static int open_count;
static int overlay_on;
static int capture_on;
static struct camera_buf capture_channel_buf[CAMERA_CHANNEL_BUFFER_NUM];
static struct camera_buf overlay_channel_buf[CAMERA_CHANNEL_BUFFER_NUM];

int camera_open()
{
//	printf("%s: enter\n", __func__);
	if(open_count == 0){
		camera_fd = open("/dev/v4l/video0", O_RDWR);
		if(camera_fd < 0){
			printf("%s: open v4l2 error\n", __func__);
		}

		overlay_on = 0;
		capture_on = 0;
	}
	open_count++;
//	printf("%s: leave\n", __func__);
	return camera_fd;
}
int camera_init_channel(camera_channel channel, int width, int height)
{
	unsigned int i;
	struct v4l2_buffer *buf;
	struct camera_buf* camera_buf;
	struct v4l2_format format;
	struct v4l2_requestbuffers request;

//	printf("%s: enter\n", __func__);

	//Set format
	if(channel == capture_channel){
		format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		format.fmt.pix.pixelformat = V4L2_PIX_FMT_YUV420;
	}
	else{
		format.type = V4L2_BUF_TYPE_VIDEO_OVERLAY;
		format.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
	}
	format.fmt.pix.width = width;
	format.fmt.pix.height = height;
	format.fmt.pix.sizeimage = 0;
	if (ioctl(camera_fd, VIDIOC_S_FMT, &format) < 0) {
		printf("%s: VIDIOC_S_FMT failed\n", __FUNCTION__);
		return -2;
	}

	//request buffers
	request.count = CAMERA_CHANNEL_BUFFER_NUM;
	if(channel == capture_channel)
		request.type  = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	else
		request.type  = V4L2_BUF_TYPE_VIDEO_OVERLAY;
	request.memory = V4L2_MEMORY_MMAP;
	if(ioctl(camera_fd, VIDIOC_REQBUFS, &request) < 0) {
		printf("CMOS:V4L2:request buf error\n");
		return -1;
	}

	if(channel == capture_channel)
		camera_buf = capture_channel_buf;
	else
		camera_buf = overlay_channel_buf;

	for (i = 0; i < CAMERA_CHANNEL_BUFFER_NUM; i++) {
		buf = &camera_buf[i].buf;
		memset(buf, 0, sizeof(buf));
		if(channel == capture_channel)
			buf->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		else
			buf->type = V4L2_BUF_TYPE_VIDEO_OVERLAY;
		buf->memory = V4L2_MEMORY_MMAP;
		buf->index = i;
		if (ioctl(camera_fd, VIDIOC_QUERYBUF, buf) < 0) {
			printf("%s: VIDIOC_QUERYBUF error\n", __FUNCTION__);
			return -1;
		}
		camera_buf[i].addr = mmap(NULL, buf->length,
					    PROT_READ | PROT_WRITE, MAP_SHARED,
					    camera_fd, buf->m.offset);
	}

	for (i = 0; i < CAMERA_CHANNEL_BUFFER_NUM; i++) {
		buf = &camera_buf[i].buf;
		if (ioctl(camera_fd, VIDIOC_QBUF, buf) < 0) {
			printf("%s: VIDIOC_QBUF error\n", __FUNCTION__);
			return -2;
		}
	}
	return 0;
//	printf("%s: leave\n", __func__);
}
int camera_start_channel(camera_channel channel)
{
//	printf("%s: enter\n", __func__);
	
	if(channel == capture_channel){
		if(capture_on == 0){
			enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			if (ioctl(camera_fd, VIDIOC_STREAMON, &type) < 0) {
				printf("%s: VIDIOC_STREAMON error\n", __FUNCTION__);
				return -3;
			}
			capture_on = 1;
		}
	}else{
		if(overlay_on == 0){
			int on = 1;
			if(ioctl(camera_fd, VIDIOC_OVERLAY, &on) < 0){
				printf("%s: VIDIOC_OVERLAY error\n", __FUNCTION__);
				return -1;
			}
			overlay_on = 1;
		}
	}

//	printf("%s: leave\n", __func__);
	return 0;
}
int camera_stop_channel(camera_channel channel)
{
	if(channel == capture_channel){
		enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		if (ioctl(camera_fd, VIDIOC_STREAMOFF, &type) < 0) {
			printf("%s: VIDIOC_STREAMOFF error\n", __FUNCTION__);
			return -3;
		}
	}else{
		int on = 0;
		if(ioctl(camera_fd, VIDIOC_OVERLAY, &on) < 0){
			printf("%s: VIDIOC_OVERLAY error\n", __FUNCTION__);
			return -1;
		}
	}

	return 0;
}
struct camera_buf* camera_get_data(camera_channel channel)
{
	struct camera_buf* camera_buf;
	struct v4l2_buffer buf;

	//printf("%s: enter\n", __func__);
	
	memset(&buf, 0, sizeof(buf));
	if(channel == capture_channel)
		buf.type= V4L2_BUF_TYPE_VIDEO_CAPTURE;
	else
		buf.type= V4L2_BUF_TYPE_VIDEO_OVERLAY;
	buf.memory = V4L2_MEMORY_MMAP;
	int ret = ioctl(camera_fd, VIDIOC_DQBUF, &buf);
	if(ret < 0){
		printf("%s:DQBUF error: channel=%d\n", __func__, channel);
		return NULL;
	}

	if(channel == capture_channel)
		camera_buf = capture_channel_buf;
	else
		camera_buf = overlay_channel_buf;

	//printf("%s: leave\n", __func__);
	
	return &camera_buf[buf.index]; 
}

int camera_finish_data(camera_channel channel, struct camera_buf *camera_buf)
{
	if(ioctl(camera_fd, VIDIOC_QBUF, &(camera_buf->buf)) < 0) {
		printf("%s:V4L2: Capture re-QBUF error\n", __func__);
		return -1;
	}
	return 0;
}

int camera_destroy_channel(camera_channel channel)
{
	struct camera_buf* camera_buf;
	int i;
	
	if(channel == capture_channel)
		camera_buf = capture_channel_buf;
	else
		camera_buf = overlay_channel_buf;
	for(i =0; i < CAMERA_CHANNEL_BUFFER_NUM; i++){
		if(munmap(camera_buf[i].addr, camera_buf[i].buf.length) < 0){
			printf("%s: munmap error\n", __func__);
		}
	}

	return 0;
}

int camera_close()
{
//	printf("%s: enter\n", __func__);
	open_count--;
	if(open_count == 0){
		//camera_stop_channel(capture_channel);
		//camera_stop_channel(overlay_channel);
		close(camera_fd);
		camera_fd = 0;
		capture_on = 0;
		overlay_on = 0;
	}
	return 0;
//	printf("%s: leave\n", __func__);
}

