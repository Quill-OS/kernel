#ifndef fake_s1d13522_h //[
#define fake_s1d13522_h

#include <linux/kernel.h>
#include "epdfb_dc.h"


//#define OUTPUT_SNAPSHOT_IMGFILE	16
//



typedef enum {
	epdfb_rotate_0=0,
	epdfb_rotate_90=1,
	epdfb_rotate_180=2,
	epdfb_rotate_270=3,
} epdfb_rotate_t;



extern volatile unsigned char *gpbLOGO_vaddr;
extern volatile unsigned char *gpbLOGO_paddr;
extern volatile unsigned long gdwLOGO_size;

extern volatile unsigned char *gpbWF_vaddr;
extern volatile unsigned char *gpbWF_paddr;
extern volatile unsigned long gdwWF_size;

extern volatile long glVCOM_uV;


EPDFB_DC *fake_s1d13522_init(void);
EPDFB_DC *fake_s1d13522_initEx(unsigned char bBitsPerPixel,unsigned char *pbDCBuf);
int fake_s1d13522_release(EPDFB_DC *pDC);
EPDFB_DC *fake_s1d13522_initEx2(unsigned char bBitsPerPixel,unsigned char *pbDCBuf,
	unsigned short wScrW,unsigned short wScrH);
EPDFB_DC *fake_s1d13522_initEx3(unsigned char bBitsPerPixel,unsigned char *pbDCBuf,
	unsigned short wScrW,unsigned short wScrH,unsigned short wFBW,unsigned short wFBH);
	
int32_t fake_s1d13522_ioctl(unsigned int cmd,unsigned long arg,EPDFB_DC *pDC);
void fake_s1d13522_progress_start(EPDFB_DC *pDC);

void fake_s1d13522_progress_stop(void);

void fake_s1d13522_parse_epd_cmdline(void);

int fake_s1d13522_display_img(u16 wX,u16 wY,u16 wW,u16 wH,u8 *pbImgBuf,
	EPDFB_DC *pDC,int iPixelBits,epdfb_rotate_t I_tRotate);


// fb_check_var() helper ... 
int fake_s1d13522_check_var(struct fb_var_screeninfo *var, struct fb_info *info);
// gallen 2011/05/24 copy from s1d13521base.c .
int fake_s1d13522_setcolreg(unsigned regno, unsigned red, unsigned green,
                                unsigned blue, unsigned transp, struct fb_info *info);


int fake_s1d13522_write_file_ex2(char *filename, char *data,unsigned long dwDataSize,unsigned long dwSeekSize);


int fb_capture_ex(EPDFB_DC *pDC,int iSrcImgX,int iSrcImgY,int iSrcImgW,int iSrcImgH,
		int iBitsTo,EPDFB_ROTATE_T I_tRotateDegree,char *pszFileName,
		unsigned long I_dwCapTotal,int iUpdMode,int iIsFullUpd);

void fb_capture(EPDFB_DC *pDC,int iBitsTo,EPDFB_ROTATE_T I_tRotateDegree,char *pszFileName);

int fake_s1d13522_is_logo_bypss(void);
#endif //]fake_s1d13522_h
