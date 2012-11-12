#ifndef __AKBASETYPE_H__
#define __AKBASETYPE_H__

/**
* @FILENAME akbasetype.h
* @BRIEF define some data type
* Copyright (C) 2006 Anyka (Guangzhou) Software Technology Co., LTD
* @AUTHOR xie_zhishan
* @DATE 2006-9-19
* @UPDATE 2006-11-9
* @VERSION 1.0
* @REF Refer to Anyka C coding rule
*/

typedef unsigned char       T_U8;        /* unsigned 8 bit integer */
typedef unsigned short      T_U16;       /* unsigned 16 bit integer */
typedef unsigned long       T_U32;       /* unsigned 32 bit integer */
typedef signed char         T_S8;        /* signed 8 bit integer */
typedef signed short        T_S16;       /* signed 16 bit integer */
typedef signed long         T_S32;       /* signed 32 bit integer */
typedef void                T_VOID;      /* void */
typedef T_U8                T_BOOL;      /* BOOL type */

#ifdef WIN32
typedef __int64             T_S64;
typedef unsigned __int64    T_U64;
#else
typedef long long           T_S64;
typedef unsigned long long  T_U64;
#endif

#define AK_FALSE            0
#define AK_TRUE             1
#define AK_NULL             ((T_VOID*)0)

typedef T_S8                T_CHR;       /* char */

typedef T_VOID *            T_pVOID;     /* pointer of void data */
typedef const T_VOID*       T_pCVOID;    /* const pointer of void data */

typedef T_S8 *              T_pSTR;      /* pointer of string */
typedef const T_S8 *        T_pCSTR;     /* const pointer of string */

typedef T_U8 *              T_pDATA;     /* pointer of data */
typedef const T_U8 *        T_pCDATA;    /* const pointer of data */

#define    T_U8_MAX         ((T_U8)0xff)                // maximum T_U8 value
#define    T_U16_MAX        ((T_U16)0xffff)             // maximum T_U16 value
#define    T_U32_MAX        ((T_U32)0xffffffff)         // maximum T_U32 value
#define    T_S8_MIN         ((T_S8)(-127-1))            // minimum T_S8 value
#define    T_S8_MAX         ((T_S8)127)                 // maximum T_S8 value
#define    T_S16_MIN        ((T_S16)(-32767L-1L))       // minimum T_S16 value
#define    T_S16_MAX        ((T_S16)(32767L))           // maximum T_S16 value
#define    T_S32_MIN        ((T_S32)(-2147483647L-1L))  // minimum T_S32 value
#define    T_S32_MAX        ((T_S32)(2147483647L))      // maximum T_S32 value


#ifndef WORD
    #define  WORD           unsigned short
#endif

#ifndef DWORD
    #define DWORD           T_U32
#endif

#ifndef BYTE
    #define BYTE            T_U8
#endif



//------------------------------------------------------
// used in defining packed structure
//------------------------------------------------------

#ifndef _WINGDI_

#ifndef OS_WIN32
#    ifdef __CC_ARM        // armcc
#        define PACKED_STRUCT    __packed struct
#    else                // arm-elf-gcc
#        define PACKED_STRUCT    struct __attribute__((packed))
#    endif
#else            // OS_WIN32, another method to define packed
#    define  PACKED_STRUCT    struct
#endif


/**************************BITMAPFILEHEADER defination************************/

#ifdef OS_WIN32
#pragma pack(1)
#endif

typedef PACKED_STRUCT tagBITMAPFILEHEADER
{
    T_U16   bfType;
    T_U32   bfSize;
    T_U16   bfReserved1;
    T_U16   bfReserved2;
    T_U32   bfOffBits;
}BITMAPFILEHEADER;

#ifdef OS_WIN32
#pragma pack()
#endif


/**************************BITMAPFILEHEADER defination end*******************/

/**************************BITMAPINFOHEADER defination************************/

#ifdef OS_WIN32
#pragma pack(1)
#endif

typedef PACKED_STRUCT tagBITMAPINFOHEADER
{
    T_U32   biSize;
    T_S32   biWidth;
    T_S32   biHeight;
    T_U16   biPlanes;
    T_U16   biBitCount;
    T_U32   biCompression;
    T_U32   biSizeImage;
    T_S32   biXPelsPerMeter;
    T_S32   biYPelsPerMeter;
    T_U32   biClrUsed;
    T_U32   biClrImportant;
}BITMAPINFOHEADER;

#ifdef OS_WIN32
#pragma pack()
#endif

/**************************BITMAPINFOHEADER defination end*******************/

#endif    // #ifndef _WINGDI_

//////////////////////

// fixed point type
typedef T_S32               Fixed;

//-----------------------------------------
// fixed point calculation
//-----------------------------------------

#define AK_FALSE            0
#define AK_TRUE             1

#define PRESICION           16

#define ONE                 (1<<PRESICION)
#define HALF                (ONE>>1)
#define ZERO                0


#define MUL(a, b)           (Fixed) (((T_S64)(a) * (T_S64)(b)) >> PRESICION)
#define IntToFixed(a)       (Fixed) ((a)<<PRESICION)
#define FixedToInt(a)       (T_S32) ((a)>>PRESICION)
#define ROUND(a)            FixedToInt((a))

//////////////////////

#endif

