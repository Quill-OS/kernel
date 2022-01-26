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


struct mxcfb_mx5_rect {
	unsigned long top;
	unsigned long left;
	unsigned long width;
	unsigned long height;
};

struct mxcfb_mx5_alt_buffer_data {
#if defined(CONFIG_ANDROID) || defined (ANDROID)//[
#else//][!CONFIG_ANDROID
	void *virt_addr;
#endif//]!CONFIG_ANDROID
	unsigned long phys_addr;
	unsigned long width;	/* width of entire buffer */
	unsigned long height;	/* height of entire buffer */
	struct mxcfb_mx5_rect alt_update_region;	/* region within buffer to update */
};

struct mxcfb_mx5_update_data {
	struct mxcfb_mx5_rect update_region;
	unsigned long waveform_mode;
	unsigned long update_mode;
	unsigned long update_marker;
	int temp;
	unsigned int flags;
	struct mxcfb_mx5_alt_buffer_data alt_buffer_data;
};




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

typedef struct tagEPDFB_IMG {
	unsigned long dwX,dwY;
	unsigned long dwW,dwH;
	unsigned char bPixelBits,bReserve;
	unsigned char *pbImgBuf;
} EPDFB_IMG;

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
typedef int (*fnPutImg)(EPDFB_IMG *I_ptPutImage,EPDFB_ROTATE_T I_tRotate);
typedef int (*fnSetVCOM)(int iVCOM_set_mV);
typedef int (*fnSetVCOMToFlash)(int iVCOM_set_mV);
typedef int (*fnGetVCOM)(int *O_piVCOM_get_mV);
typedef int (*fnSendEPDUpd)(struct mxcfb_mx5_update_data *ptUPD_DATA);


// epd framebuffer device content ...
typedef struct tagEPDFB_DC {
	// public :
	unsigned long dwWidth;// visible DC width .
	unsigned long dwHeight;// visible DC height .
	unsigned long dwFBWExtra; // frame buffer width extra width .
	unsigned long dwFBHExtra; // frame buffer height extra height .
	unsigned long dwFBXOffset; // frame buffer X offset .
	unsigned long dwFBYOffset; // frame buffer Y offset .
	unsigned char bPixelBits;
	unsigned char bReservedA[1];
	volatile unsigned char *pbDCbuf;// DC buffer .
	
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
	fnPutImg pfnPutImg;
	fnSetVCOM pfnSetVCOM;
	fnSetVCOMToFlash pfnSetVCOMToFlash;
	fnGetVCOM pfnGetVCOM;
	fnSendEPDUpd pfnSendEPDUpd;
	
	// private : do not modify these member var .
	//  only for epdfbdc manager .
	unsigned long dwMagicPrivateBegin;
	
	unsigned long dwDCSize; // visible dc size .
	unsigned long dwBufSize; // dc buffer real size .
	
	unsigned long dwDCWidthBytes;
	unsigned long dwFlags;
	int iWFMode,iLastWFMode;
	int iIsForceWaitUpdateFinished;
	unsigned long dwDirtyOffsetStart,dwDirtyOffsetEnd;
	unsigned long dwMagicPrivateEnd;
	int dither8[16][16];
} EPDFB_DC;


#define epdfbdc_create_e60mt2()	epdfbdc_create(800,600,4,0)

EPDFB_DC *epdfbdc_create(unsigned long dwW,unsigned long dwH,\
	unsigned char bPixelBits,unsigned char *pbDCbuf);
	
#define EPDFB_DC_FLAG_DEFAUT	(0x0)

#define EPDFB_DC_FLAG_REVERSEDRVDATA	0x00000001 // reverse drive data -> pixel 3,pixel 4,pixel 1,pixel 2  .
#define EPDFB_DC_FLAG_REVERSEINPDATA	0x00000002 // reverse input data -> pixel 3,pixel 4,pixel 1,pixel 2  .

#define EPDFB_DC_FLAG_SKIPLEFTPIXEL			0x00000004 // shift input image data (skip 1 pixel in image left side).
#define EPDFB_DC_FLAG_SKIPRIGHTPIXEL		0x00000008 // shift input image data (skip 1 pixel in image right side).


#define EPDFB_DC_FLAG_DITHER8				0x00000100 // 

#define EPDFB_DC_FLAG_OFB_RGB565		0x00010000 // output framebuffer data format is rgb565 .
#define EPDFB_DC_FLAG_FLASHDIRTY		0x00020000 // only flash dirty image .



EPDFB_DC *epdfbdc_create_ex(unsigned long dwW,unsigned long dwH,\
	unsigned char bPixelBits,unsigned char *pbDCbuf,unsigned long dwCreateFlag);
	
EPDFB_DC *epdfbdc_create_ex2(unsigned long dwFBW,unsigned long dwFBH,\
	unsigned long dwW,unsigned long dwH,\
	unsigned char bPixelBits,unsigned char *pbDCbuf,unsigned long dwCreateFlag);


EPDFB_DC_RET epdfbdc_delete(EPDFB_DC *I_pEPD_dc);




EPDFB_DC_RET epdfbdc_fbimg_normallize(EPDFB_DC *I_pEPD_dc,\
	EPDFB_IMG *IO_pEPD_img);

EPDFB_DC_RET epdfbdc_put_fbimg(EPDFB_DC *I_pEPD_dc,\
	EPDFB_IMG *I_pEPD_img,EPDFB_ROTATE_T I_tRotateDegree);

EPDFB_DC_RET epdfbdc_put_dcimg(EPDFB_DC *pEPD_dc,
	EPDFB_DC *pEPD_dcimg,EPDFB_ROTATE_T tRotateDegree,
	unsigned long I_dwDCimgX,unsigned long I_dwDCimgY,
	unsigned long I_dwDCimgW,unsigned long I_dwDCimgH,
	unsigned long I_dwDCPutX,unsigned long I_dwDCPutY);

EPDFB_DC_RET epdfbdc_get_dirty_region(EPDFB_DC *I_pEPD_dc,\
	unsigned long *O_pdwDirtyOffsetStart,unsigned long *O_pdwDirtyOffsetEnd);
	
EPDFB_DC_RET epdfbdc_set_pixel(EPDFB_DC *I_pEPD_dc,\
	unsigned long I_dwX,unsigned long I_dwY,unsigned long I_dwPVal);
EPDFB_DC_RET epdfbdc_draw_line(EPDFB_DC *I_pEPD_dc,\
	unsigned long I_dwX1,unsigned long I_dwY1,\
	unsigned long I_dwX2,unsigned long I_dwY2,\
	unsigned char I_bLineColor,unsigned short I_wLineWidth);

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
	
EPDFB_DC_RET epdfbdc_set_width_height(EPDFB_DC *I_pEPD_dc,
		unsigned long dwFBW,unsigned long dwFBH,
		unsigned long dwW,unsigned long dwH);
		
EPDFB_DC_RET epdfbdc_set_fbxyoffset(EPDFB_DC *I_pEPD_dc,
		unsigned long dwFBXOffset,unsigned long dwFBYOffset);

EPDFB_DC_RET epdfbdc_set_flags(EPDFB_DC *I_pEPD_dc,unsigned long *pIO_dwFlags);

EPDFB_DC_RET epdfbdc_get_flags(EPDFB_DC *I_pEPD_dc,unsigned long *pO_dwFlags);

#endif //]__epdfb_dc_h

