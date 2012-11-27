/**
 * @FILENAME: Akjpeg.h
 * @BRIEF define some jpeg funtions
 *        This file provides some definitions of jpeg utils.
 *        Copyright (C) 2011 Anyka (Guang zhou) Software Technology Co., LTD
 * @AUTHOR Xie GuangYong
 * @DATA 2011-11-20
 * @VERSION 1.0
 * @REF please refer to...
 **/

#ifndef _AK_JPEG_CALLBACK_H__
#define _AK_JPEG_CALLBACK_H__

#ifdef __cplusplus
extern "C"{
#endif

#include "anyka_types.h"

/*
 * @brief		set callbacks of akjpeg, without hw_lock & hw_unlock
 * @param	NONE
 * @return	void
 * @retval	NONE
 */
extern void akjpeg_init_without_lock();

/*
 * @brief		set callback of akjpeg,with hw_lock & hw_unlock
 * @param	NONE
 * @return	void
 * @retval	NONE
 */
extern void akjpeg_init_with_lock();

/*
 * @brief		set task func for async encode task
 * @param	NONE
 * @return	void
 * @retval	NONE
 */
extern void akjpeg_set_task_func();


/*
 * @brief		encode yuv to jpeg
 * @param	srcYUV[in], dstStream[in], pSize[in], pic_width[in], pic_height[in], quality[in]
 * @return	unsigned char
 * @retval	0 for error,other for success.
 */
extern unsigned char akjpeg_encode_yuv420( T_U8 *srcYUV, T_U8 *dstStream, T_U32 *pSize,
												 T_U32 pic_width, T_U32 pic_height, T_U32 quality );


/*
 * @brief		decode jpeg to YUV420
 * @param	srcJpeg[in], jpeg_size[in], pSize[in], dstYUV[out], pic_height[out], quality[out]
 * @return	unsigned char
 * @retval	0 for error,other for success.
 */
extern unsigned char akjpeg_decode_jpeg( T_U8 *srcJpeg, T_U32 jpeg_size, T_U8 *dstYUV, 
											  T_U32 * pic_width, T_U32 * pic_height );


#ifdef __cplusplus
}
#endif



#endif

