/**
* @FILENAME gui_api.h
* @BRIEF gui api header
* Copyright (C) 2012 Anyka (Guangzhou) Software Technology Co., LTD
* @AUTHOR malei
* @DATE 2012-01-17
* @REF Refer to Anyka C coding rule
*/

#ifndef __gui_api_h__
#define __gui_api_h__

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif
//wait interrupt hardware id
#define GUI_HW_ID 1

/*akuio lib callback define*/
typedef T_VOID* (*CALLBACK_FUNC_LOCK)(T_S32 hw_id);
typedef T_S32   (*CALLBACK_FUNC_UNLOCK)(T_VOID* lock_handle);
typedef T_VOID* (*CALLBACK_FUNC_REGS_MAP)(T_U32 phys, T_U32 size);
typedef T_VOID  (*CALLBACK_FUNC_REGS_UNMAP)(T_VOID*vaddr, T_U32 size);
typedef T_VOID  (*CALLBACK_FUNC_SYSREG_WRITE)(T_U32 phys, T_U32 val, T_U32 mask);

/* io operation callback define*/
typedef T_S32   (*CALLBACK_FUNC_PRINTF)(T_S8 *format, ...);
typedef T_VOID* (*CALLBACK_FUNC_MALLOC)(T_S32 size);
typedef T_VOID  (*CALLBACK_FUNC_FREE)(T_VOID* ptr);
typedef T_VOID* (*CALLBACK_FUNC_MEMCPY)(T_VOID* out, T_VOID* in, T_S32 size);
typedef T_VOID* (*CALLBACK_FUNC_MEMSET)(T_VOID* out, int c, T_S32 length);

/* CMEM callback define*/
typedef T_U32   (*CALLBACK_FUNC_CMEM_PADDR)(T_VOID* vaddr);

/*standby mode callback define*/
typedef T_S32   (*CALLBACK_FUNC_WAKE_LOCK)(T_S32 lock, T_U8 *handle);
typedef T_S32   (*CALLBACK_FUNC_WAKE_UNLOCK)(T_U8 *handle);

/* gui module interrupt callback function type define */
typedef T_VOID  (*CALLBACK_FUNC_GUI_INTERRUPT_WAIT)();

typedef struct  _gui_CallbackFunc {
    //io operation callback
	CALLBACK_FUNC_PRINTF        printf;
    CALLBACK_FUNC_MALLOC        malloc;
    CALLBACK_FUNC_FREE          free;
    CALLBACK_FUNC_MEMCPY        memcpy;
    CALLBACK_FUNC_MEMSET        memset;

    //virtual addr to physical addr callback func
    CALLBACK_FUNC_CMEM_PADDR    mem_paddr;

    //akuio callback function
	CALLBACK_FUNC_LOCK          hw_lock;
	CALLBACK_FUNC_UNLOCK        hw_unlock;
	CALLBACK_FUNC_REGS_MAP      map_reg;
    CALLBACK_FUNC_REGS_UNMAP    unmap_reg;
    CALLBACK_FUNC_SYSREG_WRITE  reg_modify;

    //standby mode callback function
    CALLBACK_FUNC_WAKE_LOCK     wake_lock;
    CALLBACK_FUNC_WAKE_UNLOCK   wake_unlock;

    //gui module interrupt wait callback function
    CALLBACK_FUNC_GUI_INTERRUPT_WAIT wait_interrupt;
} gui_CallbackFunc;

/*
* pixel format
*/
enum  E_ColorFormat {
	COLOR_RGB888,
	COLOR_RGBA8888,
	COLOR_YUV420,// just for extension
};

/*
 * logic operation
 */
enum E_LogicOp {
    LOGICOP_NONE = 0,//this op do nothing
    LOGICOP_AND, // source and destination
    LOGICOP_OR, // source or destination
    LOGICOP_XOR,// source xor destination
    LOGICOP_COPY,//this op copy source value to destion
};

typedef enum E_RotateAngle {
    ROTATE_CW_0 = 0,//rotate clockwise angle 0 degree
    ROTATE_CW_90,
} GUI_ROTATEANGLE;

#define STATE_NONE             0
#define STATE_ALPHA_SHIFT      1
#define STATE_TRANSCOLOR_SHIFT 2
#define STATE_ROTATE_SHIFT     3
#define STATE_IMAGE_SRC_SHIFT  4
#define STATE_IMAGE_DST_SHIFT  5
#define STATE_PENCOLOR_SHIFT   6
#define STATE_CLIP_SHIFT       7

typedef enum E_GuiState {
    GUI_NONE       =          1<<STATE_NONE,//no state
    GUI_ALPHA      =          1<<STATE_ALPHA_SHIFT,//alpha
    GUI_TRANSCOLOR =          1<<STATE_TRANSCOLOR_SHIFT,//transparent color
    GUI_ROTATE     =          1<<STATE_ROTATE_SHIFT,
    GUI_IMAGE_SRC  =          1<<STATE_IMAGE_SRC_SHIFT,
    GUI_IMAGE_DST  =          1<<STATE_IMAGE_DST_SHIFT,
    GUI_PENCOLOR   =          1<<STATE_PENCOLOR_SHIFT,
    GUI_CLIP       =          1<<STATE_CLIP_SHIFT,
    GUI_ALL		   =          0xfe,
} GUI_STATE;

typedef enum E_GuiError {
    GUI_NO_ERROR = 0,
    GUI_INPUT_NULL_POINTER,
    GUI_PARAMETER_ERROR,
    GUI_CALLBACK_FUNC_ERROR,
    GUI_NOT_INITIALIZE,
    GUI_STATE_ALPHA_ERROR,
    GUI_STATE_TRANSCOLOR_ERROR,
    GUI_STATE_ROTATE_ERROR,
    GUI_STATE_IMAGE_SRC_ERROR,
    GUI_STATE_IMAGE_DST_ERROR,
    GUI_STATE_PENCOLOR_ERROR,
    GUI_STATE_CLIP_ERROR,
    GUI_SCALE_FACTOR_ERROR,
    GUI_TIMEOUT_ERROR,
    GUI_MALLOC_ERROR
} GUI_EORROR;

//----------------------------------------
//  2D module  function
//----------------------------------------

/*
* @BRIEF   initial gui api
* @PARAM   none
* @RETURN  AK_TRUE if success, AK_FALSE if failed
* @COMMENT this func create system dependent thread context,
*          and init error variable
*/
T_S32 Gui_Initialize();

/*
* @BRIEF   release gui thread context resource and set error variable to NO ERROR
* @RETURN  T_VOID
* @COMMENT 
*/
T_VOID Gui_Destroy();

//----------------------------------------
//  set 2D module state function
//----------------------------------------

/*
* @BRIEF   set callback function to state machine
* @PARAM   cb_func:callback func variable struct
* @RETURN  T_VOID
*/
T_VOID Gui_SetCallbackFunc(gui_CallbackFunc *cb_func);

/*
* @BRIEF   gui module state enable/disable
* @PARAM   state:module state
* @PARAM   value:enable(AK_TRUE)/disable(AK_FALSE)
* @RETURN  T_VOID
* @COMMENT set gui module state(etc alpha, transcolor, rotate, image, pencolor,
*          and clip) enable/disable
*/
T_VOID Gui_SetState(GUI_STATE state, T_U8 value);

/*
* @BRIEF   set alpha value
* @PARAM   value:alpha blending level
* @RETURN  T_VOID
* @COMMENT currently, the gui module only support 16 level of alpha blending,
*          so, the validate alpha value is 0 ~ 0xf.
*/
T_VOID Gui_SetAlpha(T_U8 value);

/*
* @BRIEF   set logic operation
* @PARAM   value:logic operation
* @RETURN  T_VOID
* @COMMENT the logic operation mode is one of these(LOGICOP_NONE, LOGICOP_AND, 
*          LOGICOP_OR, LOGICOP_XOR, and LOGICOP_COPY)
*/
T_VOID Gui_SetLogicOp(T_U8 value);

/*
* @BRIEF   set transparent color
* @PARAM   neq:AK_TRUE, if the pixel is not equal to transcolor, then transparent,
* @PARAM   dst:AK_TRUE, compare the transcolor to dst image,AK_FALSE, compare to source image
* @PARAM   color:transparenct color, format:[0-7]is B channel,[8-15]is G channel,[16-23]is R channel
* @RETURN  T_VOID
* @COMMENT 
*/
T_VOID Gui_SetTransColor(T_U8 neq, T_U8 dst, T_U32 color);

/*
* @BRIEF   set rotate angle
* @PARAM   angle:ROTATE_CW_0 or ROTATE_CW_90
* @RETURN  T_VOID
* @COMMENT set rotate angle to internal state machine.
*/
T_VOID Gui_SetRotate(GUI_ROTATEANGLE angle);

/*
* @BRIEF   set src image information
* @PARAM   addr:image data address
* @PARAM   width:image width
* @PARAM   height:image height
* @PARAM   format:image pixel format - current format is COLOR_RGB888
* @RETURN  T_VOID
* @COMMENT set source image infomation to internal state machine.
*/
T_VOID Gui_SetImageSrc(T_VOID *addr, T_U16 width, T_U16 height, T_U8 format);

/*
* @BRIEF   set src image active region
* @PARAM   x:region horazonal offset to image oregion(0, 0)
* @PARAM   y:region vertical offset to image oregion(0, 0)
* @PARAM   w:region width
* @PARAM   h:region height
* @RETURN  T_VOID
* @COMMENT set source image ROI(region of interest) to internal state machine.
*/
T_VOID Gui_SetRectSrc(T_U16 x, T_U16 y, T_U16 w, T_U16 h);

/*
* @BRIEF   set dst image information
* @PARAM   addr:image data address
* @PARAM   width:image width
* @PARAM   height:image height
* @PARAM   format:image pixel format - current format is COLOR_RGB888
* @RETURN  T_VOID
* @COMMENT set dst image infomation to internal state machine.
*/
T_VOID Gui_SetImageDst(T_VOID *addr, T_U16 width, T_U16 height, T_U8 format);

/*
* @BRIEF   set dst image active region
* @PARAM   x:region horazonal offset to image oregion(0, 0)
* @PARAM   y:region vertical offset to image oregion(0, 0)
* @PARAM   w:region width
* @PARAM   h:region height
* @RETURN  T_VOID
* @COMMENT set dst image ROI(region of interest) to internal state machine.
*/
T_VOID Gui_SetRectDst(T_U16 x, T_U16 y, T_U16 w, T_U16 h);

/*
* @BRIEF   set pen color value to internal state machine.
* @PARAM   color:the forground color
* @RETURN  T_VOID
* @COMMENT color format:[0-7]is B channel,[8-15]is G channel,[16-23]is R channel
*/
T_VOID Gui_SetPenColor(T_U32 color);

/*
* @BRIEF   set clip value to internal state machine.
* @PARAM   left:left bound
* @PARAM   right:right bound
* @PARAM   top:top bound
* @PARAM   bottom:bottom bound
* @RETURN  T_VOID
* @COMMENT 
*/
T_VOID Gui_SetClip(T_U16 left, T_U16 right, T_U16 top, T_U16 bottom);

//----------------------------------------
//   2D module draw function
//----------------------------------------

/*
* @BRIEF   gui dst image ROI filling  
* @PARAM   none
* @RETURN  AK_TRUE if success, AK_FALSE if failed
* @COMMENT this func start hardware operation, fill previous set pencolor to dst image ROI
*/
T_S32 Gui_RectFill();

/*
* @BRIEF   gui ROI filling width specified color 
* @PARAM   color:[0-7] B channel,[8-15] G channel, [16-23] R channel
* @RETURN  AK_TRUE if success, AK_FALSE if failed
* @COMMENT this func start hardware operation, fill the specified color to dst image ROI
*/
T_S32 Gui_RectFillWithColor(T_U32 color);

/*
* @BRIEF   gui draw operation 
* @PARAM   none
* @RETURN  AK_TRUE if success, AK_FALSE if failed
* @COMMENT this func start hardware operation, draw the src image ROI to dst image ROI
*/
T_S32 Gui_RectDraw();

/*
* @BRIEF   gui draw operation width alpha blending, 
* @PARAM   value:alpha blending value, (0x0~0xf)
* @RETURN  AK_TRUE if success, AK_FALSE if failed
* @COMMENT this func start hardware operation, draw the src image ROI to dst image ROI
*/
T_S32 Gui_RectDrawWithAlpha(T_U8 value);

/*
* @BRIEF   gui draw operation width transcolor, 
* @PARAM   neq:transcolor compare plarity
* @PARAM   dst:transcolor compare object
* @PARAM   color:transcolor value
* @RETURN  AK_TRUE if success, AK_FALSE if failed
* @COMMENT this func start hardware operation, draw the src image ROI to dst image ROI
*/
T_S32 Gui_RectDrawWithTransColor(T_U8 neq, T_U8 dst, T_U32 color);

/*
* @BRIEF   gui scale operation 
* @PARAM   none
* @RETURN  AK_TRUE if success, AK_FALSE if failed
* @COMMENT this func start hardware operation, scale the src image ROI to dst image ROI
*/
T_S32 Gui_RectScale();

/*
* @BRIEF   gui scale operation width alpha blending, 
* @PARAM   alpha:alpha blending level
* @RETURN  AK_TRUE if success, AK_FALSE if failed
* @COMMENT this func start hardware operation, scale the src image ROI to dst image ROI
*/
T_S32 Gui_RectScaleWithAlpha(T_U8 alpha);

/*
* @BRIEF   gui scale operation width transcolor
* @PARAM   neq:transcolor compare polarty
* @PARAM   dst:transcolor compare object
* @PARAM   color:trancolor value
* @RETURN  AK_TRUE if success, AK_FALSE if failed
* @COMMENT this func start hardware operation, scale the src image ROI to dst image ROI
*/
T_S32 Gui_RectScaleWithTransColor(T_U8 neq, T_U8 dst, T_U32 color);

//----------------------------------------
//   get 2D module state function
//----------------------------------------

/*
* @BRIEF   query gui module state
* @PARAM   state:module state
* @PARAM   value:get value from internal,enable(AK_TRUE)/disable(AK_FALSE)
* @RETURN  T_VOID
* @COMMENT query if the specified state of internal state machine is enable/disable.
*/
T_VOID Gui_GetState(GUI_STATE state, T_U8 *value);

/*
* @BRIEF   get alpha value
* @PARAM   value:alpha blending level
* @RETURN  T_VOID
* @COMMENT get alpha value in internal state machine.
*/
T_VOID Gui_GetAlpha(T_U8 *value);

/*
* @BRIEF   get logic operation
* @PARAM   value:logic operation get from internal state machine
* @RETURN  T_VOID
* @COMMENT the logic operation mode is one of these(LOGICOP_NONE, LOGICOP_AND, 
*          LOGICOP_OR, LOGICOP_XOR, and LOGICOP_COPY)
*/
T_VOID Gui_GetLogicOp(T_U8 *value);

/*
* @BRIEF   get transparent color
* @PARAM   neq:if get AK_TRUE, not equal mode, else equal compare mode
* @PARAM   dst:if get AK_TRUE, compare to dest image, else compare to source image
* @PARAM   color:transparenct color value
* @RETURN  T_VOID
* @COMMENT color format:format:[0-7]is B channel,[8-15]is G channel,[16-23]is R channel
*/
T_VOID Gui_GetTransColor(T_U8 *neq, T_U8 *dst, T_U32 *color);

/*
* @BRIEF   get rotate angle
* @PARAM   angle:if get ROTATE_CW_0, not rotate else ROTATE_CW_90, rotate 90 clockwise
* @RETURN  T_VOID
* @COMMENT get rotate angle to internal state machine.
*/
T_VOID Gui_GetRotate(T_U8 *angle);

/*
* @BRIEF   get src image information
* @PARAM   addr:image data address
* @PARAM   width:image width
* @PARAM   height:image height
* @PARAM   format:image pixel format - current format is COLOR_RGB888
* @RETURN  T_VOID
* @COMMENT get source image infomation to internal state machine.
*/
T_VOID Gui_GetImageSrc(T_U32 **addr, T_U16 *width, T_U16 *height, T_U8 *format);

/*
* @BRIEF   get src image active region
* @PARAM   x:region horazonal offset to image oregion(0, 0)
* @PARAM   y:region vertical offset to image oregion(0, 0)
* @PARAM   w:region width
* @PARAM   h:region height
* @RETURN  T_VOID
* @COMMENT get source image ROI(region of interest) to internal state machine.
*/
T_VOID Gui_GetRectSrc(T_U16 *x, T_U16 *y, T_U16 *w, T_U16 *h);

/*
* @BRIEF   get dst image information
* @PARAM   addr:image data address
* @PARAM   width:image width
* @PARAM   height:image height
* @PARAM   format:image pixel format - current format is COLOR_RGB888
* @RETURN  T_VOID
* @COMMENT get dst image infomation to internal state machine.
*/
T_VOID Gui_GetImageDst(T_U32 **addr, T_U16 *width, T_U16 *height, T_U8 *format);

/*
* @BRIEF   get dst image active region
* @PARAM   x:region horazonal offset to image oregion(0, 0)
* @PARAM   y:region vertical offset to image oregion(0, 0)
* @PARAM   w:region width
* @PARAM   h:region height
* @RETURN  T_VOID
* @COMMENT get dst image ROI(region of interest) to internal state machine.
*/
T_VOID Gui_GetRectDst(T_U16 *x, T_U16 *y, T_U16 *w, T_U16 *h);

/*
* @BRIEF   get pen color value to internal state machine.
* @PARAM   color:the forground color
* @RETURN  T_VOID
* @COMMENT color format:[0-7]is B channel,[8-15]is G channel,[16-23]is R channel
*/
T_VOID Gui_GetPenColor(T_U32 *color);

/*
* @BRIEF   get clip value to internal state machine.
* @PARAM   left:left bound
* @PARAM   right:right bound
* @PARAM   top:top bound
* @PARAM   bottom:bottom bound
* @RETURN  T_VOID
* @COMMENT 
*/
T_VOID Gui_GetClip(T_U16 *left, T_U16 *right, T_U16 *top, T_U16 *bottom);

//----------------------------------------
//   error function
//----------------------------------------

/*
* @BRIEF   get error state
* @RETURN  GUI_ERROR type
* @COMMENT this function should call if you not sure about the previous gui function call
*/
GUI_EORROR Gui_GetLastError();

#ifdef __cplusplus
}
#endif

#endif

