/**
 * @filename    include/linux/akfb.h
 * @brief       This file provide the ioctl definitions of ak98fb driver
 *              Copyright (C) 2010, 2011 Anyka(Guangzhou) Microelectronics Technology CO.,LTD.
 * @author      Jacky Lau
 * @date        2010-11-23
 * @version     0.1
 * @ref         Please refer to ak98fb.c
 */

#ifndef __LINUX_AKFB_H__
#define __LINUX_AKFB_H__

#ifndef __KERNEL__
/**
 * as this file will be used by kernel and application,
 * when in kernel source tree, below types is defined in a mach-depended file (mach/ak98fb.h)
 * when in system include directory, we mush redefined here.
 */
#include <linux/ioctl.h>
typedef unsigned long dma_addr_t;
typedef unsigned char ak_bool;
#define true  1
#define false 0

/**
 * SHORT_RANGE means the value of y/u/v is [16, 235]
 * FULL_RANGE means the value of y/u/v is [0, 255]
 */
enum aklcd_yuv_range {
	YUV_SHORT_RANGE = 0b0,	/* [16, 235] */
	YUV_FULL_RANGE  = 0b1	/* [0, 255] */
};

/**
 * the translucence value of overlay2 channel
 * the left side is percent of translucence,
 * the right side is the value of the register
 */
enum aklcd_ov_alpha {
	OV_TRANS_100 = 0x0,
	OV_TRANS_87  = 0x1,
	OV_TRANS_75  = 0x2,
	OV_TRANS_62  = 0x3,
	OV_TRANS_50  = 0x4,
	OV_TRANS_37  = 0x5,
	OV_TRANS_25  = 0x6,
	OV_TRANS_12  = 0x7,
	OV_TRANS_0   = 0xf
};

/**
 * the translucence value of osd channel
 * the left side is percent of translucence,
 * the right side is the value of the register
 */
enum aklcd_osd_alpha {
	OSD_TRANS_100 = 0x0,
	OSD_TRANS_87  = 0x1,
	OSD_TRANS_75  = 0x2,
	OSD_TRANS_62  = 0x3,
	OSD_TRANS_50  = 0x4,
	OSD_TRANS_37  = 0x5,
	OSD_TRANS_25  = 0x6,
	OSD_TRANS_12  = 0x7,
	OSD_TRANS_0   = 0x8
};

/**
 * tvout mode:
 * TVOUT_OFF - display in LCD panel
 * PAL - PAL mode
 * NTSC - NTSC mode
 */
enum ak_tvout_mode {
	TVOUT_OFF,
	PAL,
	NTSC
};

/**
 * Overlay channel hardware setting, conform to registers setting
 */
struct aklcd_overlay_channel {
	enum aklcd_yuv_range src_range; /* only apply to overlay1 */
	dma_addr_t           y_addr;
	dma_addr_t           u_addr;
	dma_addr_t           v_addr;
	unsigned int         src_width;
	unsigned int         src_height;

	ak_bool                 use_vpage;	  /* only apply to overlay1 */
	unsigned int         vpage_width; /* width of ov1 rectangle */
	unsigned int         vpage_height; /* height of ov1 rectangle */
	unsigned int         virt_left;	/* src rect's left on ov1 */
	unsigned int         virt_top; /* src rect's top on ov1 */

	unsigned int         disp_left;
	unsigned int         disp_top;
	unsigned int         dst_width;
	unsigned int         dst_height;

	enum aklcd_ov_alpha  alpha; /* 0...7, 16, only apply to overlay2 */
};

/**
 * the color format of osd palette, rgb565
 */
typedef unsigned short int aklcd_osd_color;	/* rgb565 */

/**
 * Osd channel  hardware setting, conform to registers setting
 */
struct aklcd_osd_channel {
	dma_addr_t           data_addr; /* 4bits per pixel */
	unsigned char			*bitmap;
	
	aklcd_osd_color      palette[16]; /* palette[0] is for transparency */

	unsigned int         disp_left;
	unsigned int         disp_top;
	unsigned int         width;
	unsigned int         height;

	enum aklcd_osd_alpha alpha; /* 0...8 */
};
#endif

//LCD RGB buffer  的最大偏移值
#define OSD_MAP_OFFSET (800*480*2*3/4096+1)

/* the opcode of FBIOPUT_AKOSDOPS ioctl */
enum ak_osd_ops{
    OSD_ENABLE,
    OSD_DISABLE,
    OSD_REFRESH,
    OSD_DONOTHING
};

/* the valid value of aklcd_osd_channel::bitmap */
enum ak_osd_mapsta
{
	OSD_BIT_UNCHANGE=0,
	OSD_BIT_TRANSPARENCY=1,
	OSD_BIT_CHANGE=2
};

/**
 * overlay channel information
 */
struct aklcd_overlay_info {
	unsigned int         overlay_id; /* 0 for overlay1, 1 for overlay2 */
	ak_bool                 enable;
	struct aklcd_overlay_channel overlay_setting;
};

/**
 * osd channel information
 */
struct aklcd_osd_info {
	ak_bool                 enable;
	struct aklcd_osd_channel osd_setting;
};

/* change overlay channel settings */
#define FBIOPUT_AKOVINFO          _IOW('F', 0x80, struct aklcd_overlay_info)
/* get overlay channel information from driver */
#define FBIOGET_AKOVINFO          _IOWR('F', 0x81, struct aklcd_overlay_info)
/* change osd channel settings */
#define FBIOPUT_AKOSDINFO         _IOW('F', 0x82, struct aklcd_osd_info)
/* get osd channel information from driver */
#define FBIOGET_AKOSDINFO         _IOR('F', 0x83, struct aklcd_osd_info)
/* for android overlay hal control interface. enable is valid,
   every member in aklcd_overlay_channel except [yuv]_addr and vpage is valid */
#define FBIOPUT_AKOVPOSI          _IOW('F', 0x84, struct aklcd_overlay_info)
/* for android overlay hal data interface. enable is valid,
   [yuv]_addr in aklcd_overlay_channel is valid */
#define FBIOPUT_AKOVDATA          _IOW('F', 0x85, struct aklcd_overlay_info)
/* for anyka's android overlay hal extension.
   enable == 0 means hide overlay, enable == 1 means show overlay again */
#define FBIOPUT_AKOVSHOWING       _IOW('F', 0x86, struct aklcd_overlay_info)

/* turn off/on tvout */
#define FBIOPUT_AKTVOUT       _IOW('F', 0x87, enum ak_tvout_mode)

/* control osd channel */
#define FBIOPUT_AKOSDOPS         _IOW('F', 0x88, enum ak_osd_ops)

/* get tvout mode */
#define FBIOGET_AKTVOUT         _IOW('F', 0x89, enum ak_tvout_mode)
#endif
