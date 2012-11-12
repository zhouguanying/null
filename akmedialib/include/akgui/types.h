/**
* @FILENAME types.h
* @BRIEF type defines
* Copyright (C) 2011 Anyka (Guangzhou) Software Technology Co., LTD
* @AUTHOR malei
* @DATE 2011-12-15
* @REF Refer to Anyka C coding rule
*/

#ifndef		__types_h__
#define		__types_h__

#ifdef __cplusplus
extern "C" {
#endif

typedef		unsigned char		T_U8;		// unsigned 8 bit integer
typedef		signed char			T_S8;		// signed 8 bit integer

typedef		unsigned short		T_U16;		// unsigned 16 bit integer
typedef		signed short		T_S16;		// signed 16 bit integer

typedef		unsigned long		T_U32;		// unsigned 32 bit integer
typedef		signed long			T_S32;		// signed 32 bit integer

#ifdef HAVE_MSVC
typedef		__int64				T_S64;		// signed 64 bit integer
typedef		unsigned __int64	T_U64;		// unsigned 64 bit integer
#endif	// HAVE_MSVC

#ifdef HAVE_GNUC
typedef		long long			T_S64;		// signed 64 bit integer
typedef		unsigned long long	T_U64;		// unsigned 64 bit integer
#endif	// HAVE_GNUC

#ifdef HAVE_ARMCC
typedef		long long			T_S64;		// signed 64 bit integer
typedef		unsigned long long	T_U64;		// unsigned 64 bit integer
#endif	// HAVE_ARMCC

typedef		char				T_CHAR;		// char
typedef		short				T_SHORT;	// short
typedef		int					T_INT;		// int

typedef		float				T_FLOAT;	// float
typedef		double				T_DOUBLE;	// double
typedef		void				T_VOID;		// void

typedef		T_U8				T_BYTE;		// byte
typedef		T_S32				T_BOOL;		// bool
typedef		T_S32				T_FIXED;	// fixed point(32 bit integer)

typedef		T_VOID *			T_pVOID;	// pointer of void data
typedef		const T_VOID *		T_pCVOID;	// const pointer of void data

typedef		T_CHAR *			T_pSTR;		// pointer of string
typedef		const T_CHAR *		T_pCSTR;	// const pointer of string

typedef		T_U8 *				T_pDATA;	// pointer of data
typedef		const T_U8 *		T_pCDATA;	// const pointer of data

typedef		T_S16				T_LEN;		// length type: unsigned short
typedef		T_S16				T_POS;		// position type: short
typedef		T_U32				T_COLOR;
typedef		T_U32				T_TIMER;

typedef		T_U32				T_HANDLE;	// a handle
typedef		T_pVOID				T_LPTHREAD_START;	// handle for thread

#define		AK_FALSE	0
#define		AK_TRUE		1

#ifdef __cplusplus
#	define		AK_NULL		0
#else
#	define		AK_NULL		((T_pVOID)0)
#endif	// __cplusplus

#ifdef __cplusplus
}
#endif

#endif	// __types_h__
