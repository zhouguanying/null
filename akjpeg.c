/**
 * @FILENAME: Akjpeg.c
 * @BRIEF implement some jpeg funtions
 *        This file provides the implementation of jpeg utils.
 *        Copyright (C) 2011 Anyka (Guang zhou) Software Technology Co., LTD
 * @AUTHOR Xie GuangYong
 * @DATA 2011-11-20
 * @VERSION 1.0
 * @REF please refer to...
 **/

//#define LOG_NDEBUG 0
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <akuio/akuio.h>
#include <akimagelib/image_api.h>
//#include <akimagelib/akbasetype.h>

#include "akjpeg.h"
#include "headers.h"

//#define SHOW_ENCODE_TIME
//#define SHOW_DECODE_TIME
#define LOG_BUF_SIZE		1024


//#define DEBUG 
#if defined(DEBUG)
#define DPRINTF(args...) printf(args)
#else
#define DPRINTF(args...)
#endif
/*
 * @brief		allocate memory
 * @param	size[in] ,filename[in], line[in]
 * @return	T_pVOID
 * @retval	NULL for error,the handle of memory allocated if succeed.
 */
static T_pVOID jpeg_malloc(IMG_T_U32 size, T_pSTR filename, IMG_T_U32 line)
{
	DPRINTF("jpeg_malloc size(%ld) filename(%s) line(%ld)\n",size,filename,line);
	return malloc(size);
}

/*
 * @brief		free memory
 * @param	mem[in]
 * @return	T_pVOID
 * @retval	the handle of the freed memory
 */
static T_pVOID jpeg_free(T_pVOID mem)
{
	DPRINTF("jpeg_free (%p)\n",mem);
	free(mem);
	return mem;
}

/*
 * @brief		print some message
 * @param	format[in] , ...[in]
 * @return	IMG_T_S32
 * @retval	legth of printed messages
 */
static IMG_T_S32 jpeg_printf(IMG_T_pCSTR format, ...)
{
#ifdef DEBUG
	va_list ap;
    T_CHR buf[LOG_BUF_SIZE];    

    va_start(ap, format);
    vsnprintf( buf, LOG_BUF_SIZE, format, ap );
    va_end(ap);

	fprintf( stderr, "::" );
	fprintf( stderr, buf );
	fprintf( stderr, "\n" );
#endif

	return 1;
}

/*
 * @brief		allocate physic memory
 * @param	size[in]
 * @return	IMG_T_pVOID
 * @retval	NULL for error,the handle of memory allocated if succeed.
 */
static IMG_T_pVOID jpeg_akuio_alloc_pmem(IMG_T_U32 size)
{
	DPRINTF(" jpeg_akuio_alloc_pmem size(%lu)\n",size);
	return (IMG_T_pVOID)akuio_alloc_pmem(size);
}

/*
 * @brief		free physic memory
 * @param	mem[in]
 * @return	T_pVOID
 * @retval	the handle of the freed memory
 */
static IMG_T_VOID jpeg_akuio_free_pmem(T_pVOID mem)
{
	DPRINTF(" jpeg_akuio_free_pmem mem(%p)\n",mem);
	return (IMG_T_VOID)akuio_free_pmem(mem);
}

/*
 * @brief		convert virtual address to physic address
 * @param	mem[in]
 * @return	IMG_T_pVOID
 * @retval	the corresponding physic memory
 */
static IMG_T_pVOID jpeg_akuio_vaddr2paddr(T_pVOID mem)
{
	DPRINTF(" jpeg_akuio_vaddr2paddr mem(%p)\n",mem);
	return (IMG_T_pVOID)akuio_vaddr2paddr(mem);
}

/*
 * @brief		map register
 * @param	paddr[in], size[in]
 * @return	IMG_T_U32
 * @retval	0 for success,other for error.
 */
static IMG_T_U32 jpeg_akuio_map_regs(IMG_T_U32 paddr, IMG_T_U32 size)
{
	return (IMG_T_U32)akuio_map_regs(paddr, size);
}

/*
 * @brief		unmap register
 * @param	paddr[in], size[in]
 * @return	IMG_T_VOID
 * @retval	NONE
 */
static IMG_T_VOID jpeg_akuio_unmap_regs(IMG_T_U32 paddr, IMG_T_U32 size)
{
#if 0
	DPRINTF(" jpeg_akuio_unmap_regs paddr(0x%lx) size(%lu)\n",paddr, size);
	struct timeval tv;
    gettimeofday(&tv, NULL);
    unsigned int start = (tv.tv_sec * 1000 + tv.tv_usec / 1000 ); // microseconds to milliseconds
#endif
	akuio_unmap_regs((void*)paddr, size);
#if 0
	gettimeofday(&tv, NULL);
	unsigned int now = (tv.tv_sec * 1000 + tv.tv_usec / 1000 ); // microseconds to milliseconds
	int dist = now - start;
	DPRINTF("@@@@jpeg_akuio_unmap_regs,consumes time (%u)ms\n", now-start);
#endif
	//return (IMG_T_VOID);
}

/*
 * @brief		get hardware lock
 * @param	hw_id[in]
 * @return	IMG_T_pVOID
 * @retval	NULL if failed,otherwise the handle of lock
 */
static IMG_T_pVOID jpeg_akuio_lock(IMG_T_S32 hw_id)
{
	DPRINTF(" jpeg_akuio_lock hw_id(%ld)\n",hw_id);
	return akuio_lock_unblock((int)hw_id);
}

/*
 * @brief		release hardware lock
 * @param	hLock[in]
 * @return	IMG_T_S32
 * @retval	0 for success,other for error.
 */
static IMG_T_S32 jpeg_akuio_unlock(T_pVOID hLock)
{
	DPRINTF(" jpeg_akuio_unlock (%p)\n",hLock);
	return (IMG_T_S32)akuio_unlock(hLock);
}

/*
 * @brief		modify value of register
 * @param	addr[in] ,value[in], mask[in]
 * @return	IMG_T_BOOL
 * @retval	AK_TRUE for success,other for error.
 */
static IMG_T_BOOL jpeg_reg_modify(IMG_T_U32 addr, IMG_T_U32 value, IMG_T_U32 mask)
{
	DPRINTF(" jpeg_regModify addr(0x%08lx) value=(0x%08lx) mask=(0x%08lx)\n",addr,value,mask);
	akuio_sysreg_write(addr,value,mask);
	return AK_TRUE;
}

/*
 * @brief		flush l2 cache
 * @param	NONE
 * @return	IMG_T_VOID
 * @retval	NONE
 *//*
static IMG_T_VOID jpeg_flush_cache()
{
	DPRINTF(" jpeg_flush_cache\n");
	akuio_inv_cache();
}
*/


/*
 * @brief		set a task function for jpeg
 * @param	NONE
 * @return	IMG_T_VOID
 * @retval	NONE
 */
static IMG_T_VOID jpeg_task_func()
{
	DPRINTF("jpeg_task_func\n");
	akuio_wait_irq();
	DPRINTF("jpeg_task_func return\n");
}

/*
 * @brief		initialize jpeg CB_FUNS structure
 * @param	block[in], jpegCbFunc[in]
 * @return	IMG_T_VOID
 * @retval	NONE
 */
static void jpeg_init_cb_funs(T_S32 block, CB_FUNS *jpegCbFunc)
{
	memset(jpegCbFunc,0,sizeof(CB_FUNS));
	jpegCbFunc->malloc = jpeg_malloc;
	jpegCbFunc->free = jpeg_free;
	jpegCbFunc->printf = jpeg_printf;
	jpegCbFunc->dma_malloc = jpeg_akuio_alloc_pmem;
	jpegCbFunc->dma_free = jpeg_akuio_free_pmem;
	jpegCbFunc->vaddr2paddr = jpeg_akuio_vaddr2paddr;
	jpegCbFunc->reg_map = jpeg_akuio_map_regs;
	jpegCbFunc->reg_unmap = jpeg_akuio_unmap_regs;
	jpegCbFunc->regModify = jpeg_reg_modify;
	if(block != 0){
		jpegCbFunc->hw_lock = jpeg_akuio_lock;
		jpegCbFunc->hw_unlock = jpeg_akuio_unlock;
	}
}


////////////////////////////////////////////////////////////////////////////////////////////

/*
 * @brief		set callbacks of akjpeg, without hw_lock & hw_unlock
 * @param	NONE
 * @return	void
 * @retval	NONE
 */
void akjpeg_init_without_lock()
{
	//set up jpeg callback functions
	CB_FUNS jpegCbFunc;
	
	DPRINTF("akjpeg_init_without_lock\n");

#ifdef	CHIP_ID_AK37XX
	SetImageChipID(AK37xx_JPEG_VER);
#else
	SetImageChipID(AK980x_JPEG_VER);
#endif

	jpeg_init_cb_funs(0,&jpegCbFunc);
	Img_SetCallbackFuns(&jpegCbFunc);
	
	//set up function to flush cache
	//Img_SetFlushCacheFunc(jpeg_flush_cache);
	
	return;
}

/*
 * @brief		set callback of akjpeg,with hw_lock & hw_unlock
 * @param	NONE
 * @return	void
 * @retval	NONE
 */
void akjpeg_init_with_lock()
{
	//set up jpeg callback functions
	CB_FUNS jpegCbFunc;
	DPRINTF("akjpeg_init_with_lock\n");

	//AK37xx_JPEG_VER
#ifdef	CHIP_ID_AK37XX
	SetImageChipID(AK37xx_JPEG_VER);
#else
	SetImageChipID(AK980x_JPEG_VER);
#endif


	jpeg_init_cb_funs(1,&jpegCbFunc);
	Img_SetCallbackFuns(&jpegCbFunc);

	//set up function to flush cache
	//Img_SetFlushCacheFunc(jpeg_flush_cache);

	return;
}

/*
 * @brief		set task func for async encode task
 * @param	NONE
 * @return	void
 * @retval	NONE
 */
void akjpeg_set_task_func()
{
	//jpeg_task_func just wait irq
	Img_SetJPEGTaskFunc(jpeg_task_func);
}


/*
 * @brief		encode yuv to jpeg
 * @param	srcYUV[in], dstStream[in], pSize[in], pic_width[in], pic_height[in], quality[in]
 * @return	unsigned char
 * @retval	0 for error,other for success.
 */
unsigned char akjpeg_encode_yuv420( T_U8 *srcYUV, T_U8 *dstStream, T_U32 *pSize,
							   			  T_U32 pic_width, T_U32 pic_height, T_U32 quality)
{
	IMG_T_U8 *srcY,*srcU,*srcV;
	srcY = srcYUV;
	srcU = srcY + pic_width*pic_height;
	srcV = srcY + (pic_width*pic_height)*5/4;
	IMG_T_BOOL result = AK_FALSE;

#ifdef SHOW_ENCODE_TIME
    struct timeval tv;
	T_S32 dist = 0;
	T_U32 now = 0;
	T_U32 start = 0;
	gettimeofday(&tv, NULL);
	start = (tv.tv_sec * 1000 + tv.tv_usec / 1000 ); // microseconds to milliseconds
#endif
	
	DPRINTF("jpeg_encode_func srcYUV(%p),destStrea(%p),destSize(%lu),width(%lu),height(%lu),qaulity(%lu)\n",
		srcYUV, dstStream, *pSize, pic_width, pic_height, quality);

	result = Img_YUV2JPEG( srcY, srcU, srcV, dstStream, (IMG_T_U32*)pSize, 
						   pic_width, pic_height, quality );
	if( result == AK_FALSE ) {
		printf("!!!FAILED Img_YUV2JPEG\n");
	}
	
#ifdef SHOW_ENCODE_TIME
	gettimeofday(&tv, NULL);
	now = (tv.tv_sec * 1000 + tv.tv_usec / 1000 ); // microseconds to milliseconds
	dist = now - start;
	DPRINTF("@@@@encode srcYUV(%p),consumes time (%u)ms\n",srcYUV, now-start);
#endif

	return result;
}

unsigned char akjpeg_decode_jpeg( T_U8 *srcJpeg, T_U32 jpeg_size, T_U8 *dstYUV, 
									  T_U32 * pic_width, T_U32 * pic_height)
{
	IMG_T_BOOL result = AK_FALSE;
	
#ifdef SHOW_DECODE_TIME
	struct timeval tv;
	T_S32 dist = 0;
	T_U32 now = 0;
	T_U32 start = 0;
	gettimeofday(&tv, NULL);
	start = (tv.tv_sec * 1000 + tv.tv_usec / 1000 ); // microseconds to milliseconds
#endif

	DPRINTF("akjpeg_decode_jpeg srcJpeg(%p),dstYUV(%p),pic_width(%p),pic_height(%p),jpeg_size(%lu)\n",
		 srcJpeg, dstYUV, pic_width, pic_height, jpeg_size);

	result = Img_JPEG2YUV( srcJpeg, jpeg_size, dstYUV, (IMG_T_S32*)pic_width, (IMG_T_S32*)pic_height );
	if( result == AK_FALSE ){
		DPRINTF("!!!FAILED Img_JPEG2YUV\n");
	}

#ifdef SHOW_DECODE_TIME
	gettimeofday(&tv, NULL);
	now = (tv.tv_sec * 1000 + tv.tv_usec / 1000 ); // microseconds to milliseconds
	dist = now - start;
	DPRINTF("@@@@Decode, consumes time (%u)ms\n", now-start);
#endif	

	return result;
}


