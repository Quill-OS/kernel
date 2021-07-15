/***************************************************
 * EPD frame buffer device content manager (4 bits).
 * 
 * Author : Gallen Lin 
 * Data : 2011/01/20 
 * Revision : 1.0
 * File Name : epdfb_dc.h 
 * 
****************************************************/

#ifndef __epdfb_dc_h//[
#define __epdfb_dc_h


typedef enum {
	EPDFB_DC_SUCCESS = 0,
	EPDFB_DC_PARAMERR = -1,
	EPDFB_DC_MEMMALLOCFAIL = -2,
	EPDFB_DC_OBJECTERR = -3, // dc object content error .
	EPDFB_DC_PIXELBITSNOTSUPPORT = -4,
} EPDFB_DC_RET;

typedef enum {
	EPDFB_R_0 = 0,
	EPDFB_R_90 = 1,
	EPDFB_R_180 = 2,
	EPDFB_R_270 = 3,
} EPDFB_ROTATE_T;


typedef void (*fnVcomEnable)(int iIsEnable);
typedef void (*fnPwrOnOff)(int iIsOn);
typedef void (*fnPwrAutoOffIntervalMax)(int iIsEnable);
typedef void (*fnAutoOffEnable)(int iIsEnable);
typedef int (*fnGetWaveformBpp)(void);
typedef int (*fnSetPartialUpdate)(int iIsPartial);
typedef unsigned char *(*fnGetRealFrameBuf)(void);
typedef unsigned char *(*fnGetRealFrameBufEx)(unsigned long *O_pdwFBSize);
typedef void (*fnDispStart)(int iIsStart);
typedef int (*fnGetWaveformMode)(void);
typedef void (*fnSetWaveformMode)(int iWaveformMode);
typedef int (*fnIsUpdating)(void);
typedef int (*fnWaitUpdateComplete)(void);
typedef int (*fnSetUpdateRect)(unsigned short wX,unsigned short wY,
	unsigned short wW,unsigned short wH);


// epd framebuffer device content ...
typedef struct tagEPDFB_DC {
	// public :
	unsigned long dwWidth;
	unsigned long dwHeight;
	unsigned char bPixelBits;
	unsigned char bReservedA[1];
	volatile unsigned char *pbDCbuf;
	
	fnGetWaveformBpp pfnGetWaveformBpp;
	fnVcomEnable pfnVcomEnable;
	fnSetPartialUpdate pfnSetPartialUpdate;
	fnGetRealFrameBuf pfnGetRealFrameBuf;
	fnGetRealFrameBufEx pfnGetRealFrameBufEx;
	fnDispStart pfnDispStart;
	fnGetWaveformMode pfnGetWaveformMode;
	fnSetWaveformMode pfnSetWaveformMode;
	fnIsUpdating pfnIsUpdating;
	fnWaitUpdateComplete pfnWaitUpdateComplete;
	fnSetUpdateRect pfnSetUpdateRect;
	fnPwrAutoOffIntervalMax pfnPwrAutoOffIntervalMax;
	fnPwrOnOff pfnPwrOnOff;
	fnAutoOffEnable pfnAutoOffEnable;
	
	// private : do not modify these member var .
	//  only for epdfbdc manager .
	unsigned long dwMagicPrivateBegin;
	
	unsigned long dwDCSize;
	unsigned long dwBufSize;
	
	unsigned long dwDCWidthBytes;
	unsigned long dwFlags;
	int iWFMode,iLastWFMode;
	int iIsForceWaitUpdateFinished;
	unsigned long dwDirtyOffsetStart,dwDirtyOffsetEnd;
	unsigned long dwMagicPrivateEnd;
	
} EPDFB_DC;

typedef struct tagEPDFB_IMG {
	unsigned long dwX,dwY;
	unsigned long dwW,dwH;
	unsigned char bPixelBits,bReserve;
	unsigned char *pbImgBuf;
} EPDFB_IMG;

#define epdfbdc_create_e60mt2()	epdfbdc_create(800,600,4,0)

EPDFB_DC *epdfbdc_create(unsigned long dwW,unsigned long dwH,\
	unsigned char bPixelBits,unsigned char *pbDCbuf);
	
#define EPDFB_DC_FLAG_DEFAUT	(0x0)

#define EPDFB_DC_FLAG_REVERSEDRVDATA	0x00000001 // reverse drive data -> pixel 3,pixel 4,pixel 1,pixel 2  .
#define EPDFB_DC_FLAG_REVERSEINPDATA	0x00000002 // reverse input data -> pixel 3,pixel 4,pixel 1,pixel 2  .

#define EPDFB_DC_FLAG_SKIPLEFTPIXEL			0x00000004 // shift input image data (skip 1 pixel in image left side).
#define EPDFB_DC_FLAG_SKIPRIGHTPIXEL		0x00000008 // shift input image data (skip 1 pixel in image right side).



#define EPDFB_DC_FLAG_OFB_RGB565		0x00010000 // output framebuffer data format is rgb565 .
#define EPDFB_DC_FLAG_FLASHDIRTY		0x00020000 // only flash dirty image .



EPDFB_DC *epdfbdc_create_ex(unsigned long dwW,unsigned long dwH,\
	unsigned char bPixelBits,unsigned char *pbDCbuf,unsigned long dwCreateFlag);
	
EPDFB_DC_RET epdfbdc_delete(EPDFB_DC *I_pEPD_dc);

EPDFB_DC_RET epdfbdc_fbimg_normallize(EPDFB_DC *I_pEPD_dc,\
	EPDFB_IMG *IO_pEPD_img);

EPDFB_DC_RET epdfbdc_put_fbimg(EPDFB_DC *I_pEPD_dc,\
	EPDFB_IMG *I_pEPD_img,EPDFB_ROTATE_T I_tRotateDegree);

EPDFB_DC_RET epdfbdc_get_dirty_region(EPDFB_DC *I_pEPD_dc,\
	unsigned long *O_pdwDirtyOffsetStart,unsigned long *O_pdwDirtyOffsetEnd);
	
EPDFB_DC_RET epdfbdc_set_pixel(EPDFB_DC *I_pEPD_dc,\
	unsigned long I_dwX,unsigned long I_dwY,unsigned long I_dwPVal);

EPDFB_DC_RET epdfbdc_dcbuf_to_RGB565(EPDFB_DC *I_pEPD_dc,\
	unsigned char *O_pbRGB565Buf,unsigned long I_dwRGB565BufSize);

EPDFB_DC_RET epdfbdc_rotate(EPDFB_DC *I_pEPD_dc,EPDFB_ROTATE_T I_tRotate);
EPDFB_DC_RET epdfbdc_set_host_dataswap(EPDFB_DC *I_pEPD_dc,int I_isSet);
EPDFB_DC_RET epdfbdc_set_drive_dataswap(EPDFB_DC *I_pEPD_dc,int I_isSet);
EPDFB_DC_RET epdfbdc_set_skip_pixel(EPDFB_DC *I_pEPD_dc,int iIsSet,int iIsSkipRight);

EPDFB_DC_RET epdfbdc_get_rotate_active(EPDFB_DC *I_pEPD_dc,
	unsigned long *IO_pdwX,unsigned long *IO_pdwY,
	unsigned long *IO_pdwW,unsigned long *IO_pdwH,
	EPDFB_ROTATE_T I_tRotate);
	

#endif //]__epdfb_dc_h

