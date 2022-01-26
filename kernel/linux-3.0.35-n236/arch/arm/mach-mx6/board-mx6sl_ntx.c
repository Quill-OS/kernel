/*
 * Copyright (C) 2012 Freescale Semiconductor, Inc. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <linux/types.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/nodemask.h>
#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/fsl_devices.h>
#include <linux/smsc911x.h>
#include <linux/spi/spi.h>
#include <linux/spi/flash.h>
#include <linux/i2c.h>
#include <linux/i2c/pca953x.h>
#include <linux/ata.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/mtd/partitions.h>
#include <linux/pmic_external.h>
#include <linux/pmic_status.h>
#include <linux/mxcfb.h>
#include <linux/pwm_backlight.h>
#include <linux/fec.h>
#include <linux/memblock.h>
#include <linux/gpio.h>
#include <linux/etherdevice.h>
#include <linux/regulator/anatop-regulator.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/fixed.h>
#include <linux/mfd/max17135.h>
#include <sound/wm8962.h>
#include <sound/pcm.h>
#include <linux/power/sabresd_battery.h>
#include <../drivers/misc/ntx-misc.h>
#include <linux/platform_data/lm3630a_bl.h>
#include <linux/i2c/si114x.h>

#include <mach/common.h>
#include <mach/hardware.h>
#include <mach/mxc_dvfs.h>
#include <mach/memory.h>
#include <mach/iomux-mx6sl.h>
#include <mach/imx-uart.h>
#include <mach/viv_gpu.h>

#include <asm/irq.h>
#include <asm/setup.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/time.h>
#include <linux/usbplugevent.h>
#include <linux/mmc/sdhci.h>

#include <linux/byteorder/generic.h>

#include "usb.h"
#include "devices-imx6q.h"
#include "crm_regs.h"
#include "cpu_op-mx6.h"
#include "board-mx6sl_common.h"
#include "board-mx6sl_ntx.h"

#include "ntx_hwconfig.h"

#include "ntx_firmware_parser.h"
#include "ntx_firmware.h"

#include <linux/mfd/ricoh619.h>
#include <linux/rtc/rtc-ricoh619.h>
#include <linux/power/ricoh619_battery.h>
#include <linux/regulator/ricoh619-regulator.h>

#include "../../../drivers/input/keyboard/gpiofn.h"
#define TOUCH_HOME_LED		1
#include "../../../drivers/misc/ntx-misc.h"
#include "../../../drivers/video/backlight/lm3630a_bl_tables.h"
#include "../../../drivers/input/touchscreen/focaltech_core.h"

#include <linux/cyttsp5_core.h>
#include <linux/cyttsp5_platform.h>

#define GDEBUG 0
#include <linux/gallen_dbg.h>


#define EPD_TIMING_ED068OG1_NUMCE3	1
#define EPD_TIMING_ED068TG1			1



extern int gSleep_Mode_Suspend;


volatile unsigned gMX6SL_NTX_ACIN_PG = IMX_GPIO_NR(4, 20);	/* FEC_MDIO */
volatile unsigned gMX6SL_NTX_CHG = IMX_GPIO_NR(4, 21);	/* FEC_TX_CLK */
volatile unsigned gMX6SL_MSP_INT = IMX_GPIO_NR(4, 19);	/* FEC_RX_ER */
volatile unsigned gMX6SL_PWR_SW = IMX_GPIO_NR(4, 25);	/* FEC_CRS_DV */
volatile unsigned gMX6SL_IR_TOUCH_INT = IMX_GPIO_NR(4, 24);	/* FEC_TXD0 */
volatile unsigned gMX6SL_IR_TOUCH_RST = IMX_GPIO_NR(4, 17);	/* FEC_RXD0 */
volatile unsigned gMX6SL_HALL_EN = IMX_GPIO_NR(4, 23);	/* FEC_MDC */
volatile unsigned gMX6SL_ON_LED = IMX_GPIO_NR(4, 22);	/* FEC_TX_EN */
volatile unsigned gMX6SL_CHG_LED = IMX_GPIO_NR(4, 16);	/* FEC_TXD1 */
volatile unsigned gMX6SL_ACT_LED = IMX_GPIO_NR(4, 26);	/* FEC_REF_CLK */
volatile unsigned gMX6SL_WIFI_3V3 = IMX_GPIO_NR(5, 0);	/* SD2_DAT7 */
volatile unsigned gMX6SL_WIFI_RST = IMX_GPIO_NR(4, 27);	/* SD2_RST */
volatile unsigned gMX6SL_WIFI_INT = IMX_GPIO_NR(4, 29);	/* SD2_DAT6 */
volatile unsigned gMX6SL_HOME_LED = IMX_GPIO_NR(5, 10);	/* SD2_DAT6 */
volatile unsigned gMX6SL_FL_W_H_EN = MX6SL_FL_R_EN; /* EPDC_SCE2 */
volatile unsigned gMX6SL_FL_PWR_EN = MX6SL_FL_EN; 

volatile int giISD_3V3_ON_Ctrl = -1;

static int spdc_sel;
static int max17135_regulator_init(struct max17135 *max17135);

extern char *gp_reg_id;
extern char *soc_reg_id;
extern char *pu_reg_id;
extern int __init mx6sl_ntx_init_pfuze100(u32 int_gpio);
extern void tle4913_init(void);
#ifdef CONFIG_SND_SOC_ALC5640//[
extern void headphone_detect_init(void);
#endif //]CONFIG_SND_SOC_ALC5640

extern void fl_pwr_force_enable (int isEnable);
extern void ntx_fl_set_turnon_level(int iON_Lvl);
static int csi_enabled;

#define _MYINIT_DATA	
#define _MYINIT_TEXT	
volatile static unsigned char _MYINIT_DATA *gpbHWCFG_paddr;
//volatile unsigned char *gpbHWCFG_vaddr;
volatile unsigned long _MYINIT_DATA gdwHWCFG_size;
volatile int _MYINIT_DATA giBootPort;

volatile NTX_HWCONFIG *gptHWCFG;

static void * _MemoryRequest(void *addr, u32 len, const char * name)
{
    void * mem = NULL;
    do {
        printk(KERN_DEBUG "***%s:%d: request memory region! addr=%p, len=%hd***\n",
                __FUNCTION__, __LINE__, addr, len);
        if (!request_mem_region((u32)addr, len, name)) {
            printk(KERN_CRIT "%s():  request memory region failed! addr=%p, len %hd\n",__FUNCTION__, addr, len);
            break;
        }
        mem = (void *) ioremap_nocache((u32)addr, len);
        if (!mem) {
            printk(KERN_CRIT "***%s:%d: could not ioremap %s***\n", __FUNCTION__, __LINE__, name);
            release_mem_region((u32)addr, len);
            break;
        }
    } while (0);
    return mem;
}

int gIsCustomerUi;
static int _MYINIT_TEXT hwcfg_p_setup(char *str)
{
	gpbHWCFG_paddr = (unsigned char *)simple_strtoul(str,NULL,0);
	if(NULL==gptHWCFG) {
		gptHWCFG = (NTX_HWCONFIG *)_MemoryRequest((void *)gpbHWCFG_paddr, gdwHWCFG_size, "hwcfg_p");
		if(!gptHWCFG) {
			return 0;
		}
	}
	printk("%s() hwcfg_p=%p,vaddr=%p,size=%d,pcb=0x%x\n",__FUNCTION__,
		gpbHWCFG_paddr,gptHWCFG,(int)gdwHWCFG_size,gptHWCFG->m_val.bPCB);
	gIsCustomerUi = (int)gptHWCFG->m_val.bUIStyle;
	
	return 1;
}

#ifdef CONFIG_ANDROID_RAM_CONSOLE
static struct resource ram_console_resource = {
	.name = "android ram console",
	.flags = IORESOURCE_MEM,
};

static struct platform_device android_ram_console = {
	.name = "ram_console",
	.num_resources = 1,
	.resource = &ram_console_resource,
};

static int __init imx6x_add_ram_console(void)
{
	return platform_device_register(&android_ram_console);
}
#else
#define imx6x_add_ram_console() do {} while (0)
#endif

static int _MYINIT_TEXT ram_console_p_setup(char *str)
{
#ifdef CONFIG_ANDROID_RAM_CONSOLE
	phys_addr_t phys = (unsigned char *)simple_strtoul(str,NULL,0);

	ram_console_resource.start = phys;
	ram_console_resource.end   = phys + SZ_1M - 1;
#endif

	return 1;
}

static int _MYINIT_TEXT hwcfg_size_setup(char *str)
{
	gdwHWCFG_size = (unsigned long)simple_strtoul(str,NULL,0);
	printk("%s() hwcfg_szie=%d\n",__FUNCTION__,(int)gdwHWCFG_size);
	return 1;
}

static int _MYINIT_TEXT boot_port_setup(char *str)
{
	giBootPort = (int)simple_strtoul(str,NULL,0);
	printk("%s() boot_port=%d\n",__FUNCTION__,giBootPort);
	return 1;
}


volatile static unsigned char _MYINIT_DATA *gpbNTXFW_paddr;
volatile unsigned long _MYINIT_DATA gdwNTXFW_size;
volatile NTX_FIRMWARE_HDR *gptNTXFW;

NTX_FW_LM3630FL_RGBW_current_tab_hdr *gptLm3630fl_RGBW_curr_tab_hdr = 0;
NTX_FW_LM3630FL_dualcolor_hdr *gptLm3630fl_dualcolor_tab_hdr = 0;
NTX_FW_LM3630FL_dualcolor_percent_tab *gptLm3630fl_dualcolor_percent_tab = 0;

static int ntxfw_item_proc(
		NTX_FIRMWARE_HDR *I_ptFWHdr,
		NTX_FIRMWARE_ITEM_HDR *I_ptFWItemHdr,
		void *I_pvFWItemBin,int I_iItemIdx)
{
	int iRet = 0;

	if(!gptHWCFG) {
		return -1;
	}

	if(I_ptFWItemHdr->dw12345678!=0x12345678) {
		printk(KERN_WARNING"ntxfw format error !\n");
		return -2;
	}

	printk("ntxfw[%d],\"%s\",type=0x%x,sz=%d\n",I_iItemIdx,
			I_ptFWItemHdr->szFirmwareName,
			I_ptFWItemHdr->wFirmwareType,
			(int)I_ptFWItemHdr->dwFirmwareSize);

	if( 2==gptHWCFG->m_val.bFL_PWM||4==gptHWCFG->m_val.bFL_PWM||
			5==gptHWCFG->m_val.bFL_PWM||6==gptHWCFG->m_val.bFL_PWM||
			7==gptHWCFG->m_val.bFL_PWM) 
	{

		if(NTX_FW_TYPE_FLPERCETCURTABLE==I_ptFWItemHdr->wFirmwareType) {
			extern void ntx_percent_curr_tab_set(void *pvTable);
			ntx_percent_curr_tab_set(I_pvFWItemBin);
		}
#ifdef CONFIG_BACKLIGHT_LM3630A//[
		// models with lm3630 .
		else if(NTX_FW_TYPE_LM3630_FLPERCTTAB==I_ptFWItemHdr->wFirmwareType) {
			NTX_FW_LM3630FL_percent_tab *ptLm3630fl_percent_tab = I_pvFWItemBin;
			printk("lm3630fl percent table,color=%d\n",ptLm3630fl_percent_tab->bColor);

			if(1==gptHWCFG->m_val.bFrontLight) {
				// TABLE0 .
				lm3630a_set_FL_W_duty_table(gptHWCFG->m_val.bFrontLight,
					100,ptLm3630fl_percent_tab->bPercentBrightnessA);
				lm3630a_set_default_power_by_table(gptHWCFG->m_val.bFrontLight,
					ptLm3630fl_percent_tab->bDefaultCurrent);
			}
		}
		else if(NTX_FW_TYPE_LM3630_FLCURTABLE==I_ptFWItemHdr->wFirmwareType) {
			NTX_FW_LM3630FL_current_tab *ptLm3630fl_ricohcurr_tab = I_pvFWItemBin;
			printk("lm3630fl ricoh current table,color=%d\n",ptLm3630fl_ricohcurr_tab->bColor);

			if(5==gptHWCFG->m_val.bFL_PWM) {
				// RGBW FL .
				if(NTX_FW_FL_COLOR_WHITE==ptLm3630fl_ricohcurr_tab->bColor) {
					lm3630a_set_FL_RicohCurrTab(1,1,ptLm3630fl_ricohcurr_tab->dwCurrentA,255);
				}
				else 
				if(NTX_FW_FL_COLOR_RED==ptLm3630fl_ricohcurr_tab->bColor) {
					lm3630a_set_FL_RicohCurrTab(1,0,ptLm3630fl_ricohcurr_tab->dwCurrentA,255);
				}
				else 
				if(NTX_FW_FL_COLOR_GREEN==ptLm3630fl_ricohcurr_tab->bColor) {
					lm3630a_set_FL_RicohCurrTab(0,1,ptLm3630fl_ricohcurr_tab->dwCurrentA,255);
				}
				else 
				if(NTX_FW_FL_COLOR_BLUE==ptLm3630fl_ricohcurr_tab->bColor) {
					lm3630a_set_FL_RicohCurrTab(0,0,ptLm3630fl_ricohcurr_tab->dwCurrentA,255);
				}
			}
			
		}
		else if(NTX_FW_TYPE_LM3630_RGBW_CURTAB_HDR==I_ptFWItemHdr->wFirmwareType) {
			gptLm3630fl_RGBW_curr_tab_hdr = I_pvFWItemBin;
			printk("lm3630fl RGBW curr table:%d items\n",(int)gptLm3630fl_RGBW_curr_tab_hdr->dwTotalItems);
		}
		else if(NTX_FW_TYPE_LM3630_RGBW_CURTAB==I_ptFWItemHdr->wFirmwareType) {
			NTX_FW_LM3630FL_RGBW_current_item *L_ptLm3630fl_RGBW_cur_tab = I_pvFWItemBin;
			if(gptLm3630fl_RGBW_curr_tab_hdr) {
				lm3630a_set_FL_RGBW_RicohCurrTab(gptLm3630fl_RGBW_curr_tab_hdr->dwTotalItems,L_ptLm3630fl_RGBW_cur_tab);
			}
			else {
				printk(KERN_ERR"[Warning] LM3630FL RGBW curr table header not exist !!\n");
			}
		}
		else if(NTX_FW_TYPE_LM3630_DUALFL_HDR==I_ptFWItemHdr->wFirmwareType) {
			gptLm3630fl_dualcolor_tab_hdr = I_pvFWItemBin;
			printk("lm3630fl dual color table : %d temperatures ,c1_pwr=0x%x,c2_pwr=0x%x\n",
				(int)gptLm3630fl_dualcolor_tab_hdr->dwTotalColors,
				gptLm3630fl_dualcolor_tab_hdr->bDefaultC1_Current,
				gptLm3630fl_dualcolor_tab_hdr->bDefaultC2_Current);
		}
		else if(NTX_FW_TYPE_LM3630_DUALFL_PERCENTTAB==I_ptFWItemHdr->wFirmwareType) {
			if(gptLm3630fl_dualcolor_tab_hdr) {
				gptLm3630fl_dualcolor_percent_tab = I_pvFWItemBin;
			}
			else {
				printk(KERN_ERR"[Warning] LM3630FL dualcolor table header not exist !!\n");
			}
		}
		else if(NTX_FW_TYPE_LM3630_MIX2COLOR11_CURTAB==I_ptFWItemHdr->wFirmwareType) {
			NTX_FW_LM3630FL_MIX2COLOR11_current_tab *L_ptLm3630fl_Mix2Color11_curr_tab = I_pvFWItemBin;
			lm3630a_set_FL_Mix2color11_RicohCurrTab(L_ptLm3630fl_Mix2Color11_curr_tab);
		}
#endif //]CONFIG_BACKLIGHT_LM3630A
		else {
		}
	}

	return iRet;
}


static int _MYINIT_TEXT ntxfw_p_setup(char *str)
{
	gpbNTXFW_paddr = (unsigned char *)simple_strtoul(str,NULL,0);
	gptNTXFW = (NTX_FIRMWARE_HDR *)_MemoryRequest((void *)gpbNTXFW_paddr, gdwNTXFW_size, "ntxfw_p");

	if(ntx_firmware_parse_fw_buf(gptNTXFW,gdwNTXFW_size,ntxfw_item_proc)<0) {
		gpbNTXFW_paddr = 0;
		gptNTXFW = 0;
		gdwNTXFW_size = 0;
		return 0;
	}
	else {
		printk("%s() ntxfw_p=%p,vaddr=%p,size=%d,name=\"%s\",items=%d\n",__FUNCTION__,
			gpbNTXFW_paddr,gptNTXFW,gdwNTXFW_size,gptNTXFW->szFirmwareName,(int)gptNTXFW->wFirmwareItems);
		return 1;
	}

}
static int _MYINIT_TEXT ntxfw_size_setup(char *str)
{
	gdwNTXFW_size = (unsigned long)simple_strtoul(str,NULL,0);
	printk("%s() ntxfw_szie=%d\n",__FUNCTION__,(int)gdwNTXFW_size);
	return 1;
}


static void _parse_cmdline(void)
{
	static int iParseCnt = 0;
	char *pcPatternStart,*pcPatternVal,*pcPatternValEnd,cTempStore;
	unsigned long ulPatternLen;

	char *szParsePatternA[]={"hwcfg_sz=","hwcfg_p=","boot_port=","ram_console_p=","ntxfw_sz=","ntxfw_p="};
	int ((*pfnDispatchA[])(char *str))={hwcfg_size_setup,hwcfg_p_setup,boot_port_setup,ram_console_p_setup ,ntxfw_size_setup,ntxfw_p_setup};
		
	int i;
	char *pszCmdLineBuf;
	
	
	if(iParseCnt++>0) {
		printk("%s : cmdline parse already done .\n",__FUNCTION__);
		return ;
	}
	//printk("%s():cmdline(%d)=%s\n",__FUNCTION__,strlen(saved_command_line),saved_command_line);
		
	pszCmdLineBuf = kmalloc(strlen(saved_command_line)+1,GFP_KERNEL);
	//ASSERT(pszCmdLineBuf);
	strcpy(pszCmdLineBuf,saved_command_line);
	printk("%s():cp cmdline=%s\n",__FUNCTION__,pszCmdLineBuf);
	
	for(i=0;i<sizeof(szParsePatternA)/sizeof(szParsePatternA[0]);i++) {
		ulPatternLen = strlen(szParsePatternA[i]);
		pcPatternStart = strstr(pszCmdLineBuf,szParsePatternA[i]);
		if(pcPatternStart) {
			pcPatternVal=pcPatternStart + ulPatternLen ;
			pcPatternValEnd = strchr(pcPatternVal,' ');
			cTempStore='\0';
			if(pcPatternValEnd) {
				cTempStore = *pcPatternValEnd;
				*pcPatternValEnd = '\0';
			}
			//printk("%s():pattern \"%s\" ,val=\"%s\"\n",__FUNCTION__,szParsePatternA[i],pcPatternVal);
			pfnDispatchA[i](pcPatternVal);
			if(pcPatternValEnd) {
				*pcPatternValEnd = cTempStore;
			}
		}
		else 
			printk ("[%s-%d] %s not found !!!\n",__func__,__LINE__,szParsePatternA[i]);
	}
	if (NULL == gptHWCFG) {
		printk ("[%s-%d] personal initial hw config.\n",__func__,__LINE__);
		gptHWCFG = (NTX_HWCONFIG *)kmalloc(sizeof(NTX_HWCONFIG), GFP_KERNEL);
		gptHWCFG->m_val.bTouchCtrl = 8;
	}
}



/* uart2 pins */
#if 0 // gallen disabled .
static iomux_v3_cfg_t mx6sl_uart2_pads[] = {
	MX6SL_PAD_SD2_DAT5__UART2_TXD,
	MX6SL_PAD_SD2_DAT4__UART2_RXD,
	MX6SL_PAD_SD2_DAT6__UART2_RTS,
	MX6SL_PAD_SD2_DAT7__UART2_CTS,
};
#endif

enum sd_pad_mode {
	SD_PAD_MODE_LOW_SPEED,
	SD_PAD_MODE_MED_SPEED,
	SD_PAD_MODE_HIGH_SPEED,
};

static int plt_sd_pad_change(unsigned int index, int clock)
{
	/* LOW speed is the default state of SD pads */
	static enum sd_pad_mode pad_mode = SD_PAD_MODE_LOW_SPEED;

	iomux_v3_cfg_t *sd_pads_200mhz = NULL;
	iomux_v3_cfg_t *sd_pads_100mhz = NULL;
	iomux_v3_cfg_t *sd_pads_50mhz = NULL;

	u32 sd_pads_200mhz_cnt;
	u32 sd_pads_100mhz_cnt;
	u32 sd_pads_50mhz_cnt;

	switch (index) {
	case 0:
		sd_pads_200mhz = mx6sl_sd1_200mhz;
		sd_pads_100mhz = mx6sl_sd1_100mhz;
		sd_pads_50mhz = mx6sl_sd1_50mhz;

		sd_pads_200mhz_cnt = ARRAY_SIZE(mx6sl_sd1_200mhz);
		sd_pads_100mhz_cnt = ARRAY_SIZE(mx6sl_sd1_100mhz);
		sd_pads_50mhz_cnt = ARRAY_SIZE(mx6sl_sd1_50mhz);
		break;
	case 1:
		sd_pads_200mhz = mx6sl_sd2_200mhz;
		sd_pads_100mhz = mx6sl_sd2_100mhz;
		sd_pads_50mhz = mx6sl_sd2_50mhz;

		sd_pads_200mhz_cnt = ARRAY_SIZE(mx6sl_sd2_200mhz);
		sd_pads_100mhz_cnt = ARRAY_SIZE(mx6sl_sd2_100mhz);
		sd_pads_50mhz_cnt = ARRAY_SIZE(mx6sl_sd2_50mhz);
		break;
	case 2:
		sd_pads_200mhz = mx6sl_sd3_200mhz;
		sd_pads_100mhz = mx6sl_sd3_100mhz;
		sd_pads_50mhz = mx6sl_sd3_50mhz;

		sd_pads_200mhz_cnt = ARRAY_SIZE(mx6sl_sd3_200mhz);
		sd_pads_100mhz_cnt = ARRAY_SIZE(mx6sl_sd3_100mhz);
		sd_pads_50mhz_cnt = ARRAY_SIZE(mx6sl_sd3_50mhz);
		break;
	case 3:
		sd_pads_200mhz = mx6sl_brd_ntx_sd4_pads;
		sd_pads_100mhz = mx6sl_brd_ntx_sd4_pads;
		sd_pads_50mhz = mx6sl_brd_ntx_sd4_pads;

		sd_pads_200mhz_cnt = ARRAY_SIZE(mx6sl_brd_ntx_sd4_pads);
		sd_pads_100mhz_cnt = ARRAY_SIZE(mx6sl_brd_ntx_sd4_pads);
		sd_pads_50mhz_cnt = ARRAY_SIZE(mx6sl_brd_ntx_sd4_pads);
		break;
		
	default:
		printk(KERN_ERR "no such SD host controller index %d\n", index);
		return -EINVAL;
	}

	if (clock > 100000000) {
		if (pad_mode == SD_PAD_MODE_HIGH_SPEED)
			return 0;
		BUG_ON(!sd_pads_200mhz);
		pad_mode = SD_PAD_MODE_HIGH_SPEED;
		return mxc_iomux_v3_setup_multiple_pads(sd_pads_200mhz,
							sd_pads_200mhz_cnt);
	} else if (clock > 52000000) {
		if (pad_mode == SD_PAD_MODE_MED_SPEED)
			return 0;
		BUG_ON(!sd_pads_100mhz);
		pad_mode = SD_PAD_MODE_MED_SPEED;
		return mxc_iomux_v3_setup_multiple_pads(sd_pads_100mhz,
							sd_pads_100mhz_cnt);
	} else {
		if (pad_mode == SD_PAD_MODE_LOW_SPEED)
			return 0;
		BUG_ON(!sd_pads_50mhz);
		pad_mode = SD_PAD_MODE_LOW_SPEED;
		return mxc_iomux_v3_setup_multiple_pads(sd_pads_50mhz,
							sd_pads_50mhz_cnt);
	}
}


static struct esdhc_platform_data mx6_ntx_isd_data = {
	.always_present = 1,
	.delay_line		= 0,
	.platform_pad_change = plt_sd_pad_change,
	.cd_type = ESDHC_CD_PERMANENT,
};
static struct esdhc_platform_data mx6_ntx_isd8bits_data = {
	.always_present = 1,
	.delay_line		= 0,
	.support_8bit = 1,
	.platform_pad_change = plt_sd_pad_change,
	.cd_type = ESDHC_CD_PERMANENT,
};

static struct esdhc_platform_data mx6_ntx_sd_wifi_data = {
	.cd_gpio		= MX6SL_WIFI_3V3,
	.wp_gpio 		= -1,
	.keep_power_at_suspend	= 1,
	.delay_line		= 0,
//	.platform_pad_change = plt_sd_pad_change,
	.cd_type = ESDHC_CD_WIFI_PWR,
};

static const struct esdhc_platform_data mx6_ntx_q22_sd_wifi_data __initconst = {
	.cd_gpio		= IMX_GPIO_NR(4, 29),
	.wp_gpio 		= -1,
	.keep_power_at_suspend	= 1,
	.delay_line		= 0,
//	.platform_pad_change = plt_sd_pad_change,
	.cd_type = ESDHC_CD_WIFI_PWR,
};

static const struct esdhc_platform_data mx6_ntx_esd_data __initconst = {
	.cd_gpio		= MX6SL_EXT_SD_CD,
	.wp_gpio		= -1,
	.keep_power_at_suspend	= 1,
	.delay_line		= 0,
	.platform_pad_change = plt_sd_pad_change,
	.cd_type = ESDHC_CD_GPIO,
};
static const struct esdhc_platform_data mx6_ntx_esd_nocd_data __initconst = {
	.always_present = 1,
	.cd_gpio		= 0,
	.wp_gpio		= -1,
	.keep_power_at_suspend	= 1,
	.delay_line		= 0,
	.platform_pad_change = 0,
	.cd_type = ESDHC_CD_PERMANENT,
};

#define mV_to_uV(mV) (mV * 1000)
#define uV_to_mV(uV) (uV / 1000)
#define V_to_uV(V) (mV_to_uV(V * 1000))
#define uV_to_V(uV) (uV_to_mV(uV) / 1000)

#if 0
static struct regulator_consumer_supply ntx_vmmc_consumers[] = {
	REGULATOR_SUPPLY("vmmc", "sdhci-esdhc-imx.0"),
	REGULATOR_SUPPLY("vmmc", "sdhci-esdhc-imx.1"),
	REGULATOR_SUPPLY("vmmc", "sdhci-esdhc-imx.2"),
	REGULATOR_SUPPLY("vmmc", "sdhci-esdhc-imx.3"),
};

static struct regulator_init_data ntx_vmmc_init = {
	.num_consumer_supplies = ARRAY_SIZE(ntx_vmmc_consumers),
	.consumer_supplies = ntx_vmmc_consumers,
};

static struct fixed_voltage_config ntx_vmmc_reg_config = {
	.supply_name	= "vmmc",
	.microvolts	= 3300000,
	.gpio		= -1,
	.init_data	= &ntx_vmmc_init,
};

static struct platform_device ntx_vmmc_reg_devices = {
	.name		= "reg-fixed-voltage",
	.id		= 0,
	.dev		= {
		.platform_data = &ntx_vmmc_reg_config,
	},
};
#endif

static struct regulator_consumer_supply display_consumers[] = {
	{
		/* MAX17135 */
		.supply = "DISPLAY",
	},
};

static struct regulator_consumer_supply vcom_consumers[] = {
	{
		/* MAX17135 */
		.supply = "VCOM",
	},
};

static struct regulator_consumer_supply v3p3_consumers[] = {
	{
		/* MAX17135 */
		.supply = "V3P3",
	},
};

static struct regulator_init_data max17135_init_data[] = {
#if 0
	{
		.constraints = {
			.name = "DISPLAY",
			.valid_ops_mask =  REGULATOR_CHANGE_STATUS,
		},
		.num_consumer_supplies = ARRAY_SIZE(display_consumers),
		.consumer_supplies = display_consumers,
	}, {
		.constraints = {
			.name = "GVDD",
			.min_uV = V_to_uV(20),
			.max_uV = V_to_uV(20),
		},
	}, {
		.constraints = {
			.name = "GVEE",
			.min_uV = V_to_uV(-22),
			.max_uV = V_to_uV(-22),
		},
	}, {
		.constraints = {
			.name = "HVINN",
			.min_uV = V_to_uV(-22),
			.max_uV = V_to_uV(-22),
		},
	}, {
		.constraints = {
			.name = "HVINP",
			.min_uV = V_to_uV(20),
			.max_uV = V_to_uV(20),
		},
	}, {
		.constraints = {
			.name = "VCOM",
			.min_uV = mV_to_uV(-4325),
			.max_uV = mV_to_uV(-500),
			.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE |
			REGULATOR_CHANGE_STATUS,
		},
		.num_consumer_supplies = ARRAY_SIZE(vcom_consumers),
		.consumer_supplies = vcom_consumers,
	}, {
		.constraints = {
			.name = "VNEG",
			.min_uV = V_to_uV(-15),
			.max_uV = V_to_uV(-15),
		},
	}, {
		.constraints = {
			.name = "VPOS",
			.min_uV = V_to_uV(15),
			.max_uV = V_to_uV(15),
		},
	}, {
		.constraints = {
			.name = "V3P3",
			.valid_ops_mask =  REGULATOR_CHANGE_STATUS,
		},
		.num_consumer_supplies = ARRAY_SIZE(v3p3_consumers),
		.consumer_supplies = v3p3_consumers,
	},
#endif
};

static const struct anatop_thermal_platform_data
	mx6sl_anatop_thermal_data __initconst = {
			.name = "anatop_thermal",
	};

static struct platform_device max17135_sensor_device = {
	.name = "max17135_sensor",
	.id = 0,
};

static struct max17135_platform_data max17135_pdata __initdata = {
	.vneg_pwrup = 1,
	.gvee_pwrup = 1,
	.vpos_pwrup = 2,
	.gvdd_pwrup = 1,
	.gvdd_pwrdn = 1,
	.vpos_pwrdn = 2,
	.gvee_pwrdn = 1,
	.vneg_pwrdn = 1,
	.gpio_pmic_pwrgood = MX6SL_BRD_EPDC_PWRSTAT,
	.gpio_pmic_vcom_ctrl = MX6SL_BRD_EPDC_VCOM,
	.gpio_pmic_wakeup = MX6SL_BRD_EPDC_PMIC_WAKE,
	.gpio_pmic_v3p3 = MX6SL_BRD_EPDC_PWRCTRL0,
	.gpio_pmic_intr = MX6SL_BRD_EPDC_PMIC_INT,
	.regulator_init = max17135_init_data,
	.init = max17135_regulator_init,
};

static int __init max17135_regulator_init(struct max17135 *max17135)
{
#if 0
	struct max17135_platform_data *pdata = &max17135_pdata;
	int i, ret;

	max17135->gvee_pwrup = pdata->gvee_pwrup;
	max17135->vneg_pwrup = pdata->vneg_pwrup;
	max17135->vpos_pwrup = pdata->vpos_pwrup;
	max17135->gvdd_pwrup = pdata->gvdd_pwrup;
	max17135->gvdd_pwrdn = pdata->gvdd_pwrdn;
	max17135->vpos_pwrdn = pdata->vpos_pwrdn;
	max17135->vneg_pwrdn = pdata->vneg_pwrdn;
	max17135->gvee_pwrdn = pdata->gvee_pwrdn;

	max17135->max_wait = pdata->vpos_pwrup + pdata->vneg_pwrup +
		pdata->gvdd_pwrup + pdata->gvee_pwrup;

	max17135->gpio_pmic_pwrgood = pdata->gpio_pmic_pwrgood;
	max17135->gpio_pmic_vcom_ctrl = pdata->gpio_pmic_vcom_ctrl;
	max17135->gpio_pmic_wakeup = pdata->gpio_pmic_wakeup;
	max17135->gpio_pmic_v3p3 = pdata->gpio_pmic_v3p3;
	max17135->gpio_pmic_intr = pdata->gpio_pmic_intr;

	gpio_request(max17135->gpio_pmic_wakeup, "epdc-pmic-wake");
	gpio_direction_output(max17135->gpio_pmic_wakeup, 0);

	gpio_request(max17135->gpio_pmic_vcom_ctrl, "epdc-vcom");
	gpio_direction_output(max17135->gpio_pmic_vcom_ctrl, 0);

	gpio_request(max17135->gpio_pmic_v3p3, "epdc-v3p3");
	gpio_direction_output(max17135->gpio_pmic_v3p3, 0);

	gpio_request(max17135->gpio_pmic_intr, "epdc-pmic-int");
	gpio_direction_input(max17135->gpio_pmic_intr);

	gpio_request(max17135->gpio_pmic_pwrgood, "epdc-pwrstat");
	gpio_direction_input(max17135->gpio_pmic_pwrgood);

	max17135->vcom_setup = false;
	max17135->init_done = false;

	for (i = 0; i < MAX17135_NUM_REGULATORS; i++) {
		ret = max17135_register_regulator(max17135, i,
			&pdata->regulator_init[i]);
		if (ret != 0) {
			printk(KERN_ERR"max17135 regulator init failed: %d\n",
				ret);
			return ret;
		}
	}

	/*
	 * TODO: We cannot enable full constraints for now, since
	 * it results in the PFUZE regulators being disabled
	 * at the end of boot, which disables critical regulators.
	 */
	/*regulator_has_full_constraints();*/
#endif
	return 0;
}

static int mx6_ntx_spi_cs[] = {
	MX6_BRD_ECSPI1_CS0,
};

static const struct spi_imx_master mx6_ntx_spi_data __initconst = {
	.chipselect     = mx6_ntx_spi_cs,
	.num_chipselect = ARRAY_SIZE(mx6_ntx_spi_cs),
};

#if defined(CONFIG_MTD_M25P80) || defined(CONFIG_MTD_M25P80_MODULE)
static struct mtd_partition m25p32_partitions[] = {
	{
		.name	= "bootloader",
		.offset	= 0,
		.size	= 0x00100000,
	}, {
		.name	= "kernel",
		.offset	= MTDPART_OFS_APPEND,
		.size	= MTDPART_SIZ_FULL,
	},
};

static struct flash_platform_data m25p32_spi_flash_data = {
	.name		= "m25p32",
	.parts		= m25p32_partitions,
	.nr_parts	= ARRAY_SIZE(m25p32_partitions),
	.type		= "m25p32",
};

static struct spi_board_info m25p32_spi0_board_info[] __initdata = {
	{
	/* The modalias must be the same as spi device driver name */
	.modalias	= "m25p80",
	.max_speed_hz	= 20000000,
	.bus_num	= 0,
	.chip_select	= 0,
	.platform_data	= &m25p32_spi_flash_data,
	},
};
#endif

static void spi_device_init(void)
{
#if defined(CONFIG_MTD_M25P80) || defined(CONFIG_MTD_M25P80_MODULE)
	spi_register_board_info(m25p32_spi0_board_info,
				ARRAY_SIZE(m25p32_spi0_board_info));
#endif
}

static struct imx_ssi_platform_data mx6_sabresd_ssi_pdata = {
	.flags = IMX_SSI_DMA | IMX_SSI_SYN,
};

#if 1
static struct mxc_audio_platform_data alc5640_data;

static struct platform_device mx6_ntx_audio5640_device = {
	.name = "imx-alc5640",
};

#if 0
static struct alc5640_pdata alc5640_config_data = {

};
#endif

struct clk *ntx_extern_audio_root;

static int alc5640_clk_enable(int enable)
{
	if (enable)
		clk_enable(ntx_extern_audio_root);
	else
		clk_disable(ntx_extern_audio_root);

	return 0;
}

static int mxc_alc5640_init(void)
{
	struct clk *pll4;
	int rate;

	ntx_extern_audio_root = clk_get(NULL, "extern_audio_clk");
	if (IS_ERR(ntx_extern_audio_root)) {
		pr_err("can't get ntx_extern_audio_root clock.\n");
		return PTR_ERR(ntx_extern_audio_root);
	}

	pll4 = clk_get(NULL, "pll4");
	if (IS_ERR(pll4)) {
		pr_err("can't get pll4 clock.\n");
		return PTR_ERR(pll4);
	}

	clk_set_parent(ntx_extern_audio_root, pll4);

	rate = 4000000;
	clk_set_rate(ntx_extern_audio_root, rate);

	alc5640_data.sysclk = rate;
	mxc_iomux_v3_setup_pad( MX6SL_PAD_AUD_MCLK__AUDMUX_AUDIO_CLK_OUT );
	mxc_iomux_v3_setup_pad( MX6SL_PAD_AUD_RXD__AUDMUX_AUD3_RXD );
	mxc_iomux_v3_setup_pad( MX6SL_PAD_AUD_TXC__AUDMUX_AUD3_TXC );
	mxc_iomux_v3_setup_pad( MX6SL_PAD_AUD_TXD__AUDMUX_AUD3_TXD );
	mxc_iomux_v3_setup_pad( MX6SL_PAD_AUD_TXFS__AUDMUX_AUD3_TXFS );

	return 0;
}

static struct mxc_audio_platform_data alc5640_data = {
	.ssi_num = 1,
	.src_port = 2,
	.ext_port = 3,
	.hp_gpio = IMX_GPIO_NR(3, 24),
	.hp_active_low = 1,
	.mic_gpio = -1,
	.mic_active_low = 1,
	.init = mxc_alc5640_init,
	.clock_enable = alc5640_clk_enable,
};

#if 0
static struct regulator_consumer_supply sabresd_valc5640_consumers[] = {
	REGULATOR_SUPPLY("SPKVDD1", "1-001a"),
	REGULATOR_SUPPLY("SPKVDD2", "1-001a"),
};

static struct regulator_init_data sabresd_valc5640_init = {
	.constraints = {
		.name = "SPKVDD",
		.valid_ops_mask =  REGULATOR_CHANGE_STATUS,
		.boot_on = 1,
	},
	.num_consumer_supplies = ARRAY_SIZE(sabresd_valc5640_consumers),
	.consumer_supplies = sabresd_valc5640_consumers,
};

static struct fixed_voltage_config sabresd_valc5640_reg_config = {
	.supply_name	= "SPKVDD",
	.microvolts		= 4325000,
	.gpio			= -1,
	.enabled_at_boot = 1,
	.init_data		= &sabresd_valc5640_init,
};

static struct platform_device sabresd_valc5640_reg_devices = {
	.name	= "reg-fixed-voltage",
	.id		= 4,
	.dev	= {
		.platform_data = &sabresd_valc5640_reg_config,
	},
};
#endif

static int __init imx6q_init_audio(void)
{
#if 0
	platform_device_register(&sabresd_valc5640_reg_devices);
	mxc_register_device(&mx6_sabresd_audio_alc5640_device,
			    &wm8962_data);
#endif
	// ALC5640 codec .
	mxc_register_device(&mx6_ntx_audio5640_device,
		 &alc5640_data);
	imx6q_add_imx_ssi(1, &mx6_sabresd_ssi_pdata);

	return 0;
}

static int spdif_clk_set_rate(struct clk *clk, unsigned long rate)
{
	unsigned long rate_actual;
	rate_actual = clk_round_rate(clk, rate);
	clk_set_rate(clk, rate_actual);
	return 0;
}

static struct mxc_spdif_platform_data mxc_spdif_data = {
	.spdif_tx		= 1,
	.spdif_rx		= 0,
	.spdif_clk_44100	= 1,
	.spdif_clk_48000	= -1,
	.spdif_div_44100	= 23,
	.spdif_clk_set_rate	= spdif_clk_set_rate,
	.spdif_clk		= NULL,
};
#endif


enum DISPLAY_PANEL_MODE {
	PANEL_MODE_LCD,
	PANEL_MODE_HDMI,
	PANEL_MODE_EINK,
};

static int display_panel_mode = PANEL_MODE_EINK;

#if 0
static iomux_v3_cfg_t mx6sl_sii902x_hdmi_pads_enabled[] = {
	MX6SL_PAD_LCD_RESET__GPIO_2_19,
	MX6SL_PAD_EPDC_PWRCTRL3__GPIO_2_10,
};

static int sii902x_get_pins(void)
{
	/* Sii902x HDMI controller */
	mxc_iomux_v3_setup_multiple_pads(mx6sl_sii902x_hdmi_pads_enabled, \
		ARRAY_SIZE(mx6sl_sii902x_hdmi_pads_enabled));

	/* Reset Pin */
	gpio_request(MX6_BRD_LCD_RESET, "disp0-reset");
	gpio_direction_output(MX6_BRD_LCD_RESET, 1);

	/* Interrupter pin GPIO */
	gpio_request(MX6SL_BRD_EPDC_PWRCTRL3, "disp0-detect");
	gpio_direction_input(MX6SL_BRD_EPDC_PWRCTRL3);
       return 1;
}

static void sii902x_put_pins(void)
{
	gpio_free(MX6_BRD_LCD_RESET);
	gpio_free(MX6SL_BRD_EPDC_PWRCTRL3);
}

static void sii902x_hdmi_reset(void)
{
	gpio_set_value(MX6_BRD_LCD_RESET, 0);
	msleep(10);
	gpio_set_value(MX6_BRD_LCD_RESET, 1);
	msleep(10);
}

static struct fsl_mxc_lcd_platform_data sii902x_hdmi_data = {
       .ipu_id = 0,
       .disp_id = 0,
       .reset = sii902x_hdmi_reset,
       .get_pins = sii902x_get_pins,
       .put_pins = sii902x_put_pins,
};

static void mx6sl_csi_io_init(void)
{
	mxc_iomux_v3_setup_multiple_pads(mx6sl_brd_csi_enable_pads,	\
				ARRAY_SIZE(mx6sl_brd_csi_enable_pads));

	/* Camera reset */
	gpio_request(MX6SL_BRD_CSI_RST, "cam-reset");
	gpio_direction_output(MX6SL_BRD_CSI_RST, 1);

	/* Camera power down */
	gpio_request(MX6SL_BRD_CSI_PWDN, "cam-pwdn");
	gpio_direction_output(MX6SL_BRD_CSI_PWDN, 1);
	msleep(5);
	gpio_set_value(MX6SL_BRD_CSI_PWDN, 0);
	msleep(5);
	gpio_set_value(MX6SL_BRD_CSI_RST, 0);
	msleep(1);
	gpio_set_value(MX6SL_BRD_CSI_RST, 1);
	msleep(5);
	gpio_set_value(MX6SL_BRD_CSI_PWDN, 1);
}

static void mx6sl_csi_cam_powerdown(int powerdown)
{
	if (powerdown)
		gpio_set_value(MX6SL_BRD_CSI_PWDN, 1);
	else
		gpio_set_value(MX6SL_BRD_CSI_PWDN, 0);

	msleep(2);
}

static struct fsl_mxc_camera_platform_data camera_data = {
	.mclk = 24000000,
	.io_init = mx6sl_csi_io_init,
	.pwdn = mx6sl_csi_cam_powerdown,
	.core_regulator = "VGEN2_1V5",
	.analog_regulator = "VGEN6_2V8",
};
#endif

static struct imxi2c_platform_data mx6_ntx_i2c0_data = {
	.bitrate = 100000,
};

static struct imxi2c_platform_data mx6_ntx_i2c1_data = {
	.bitrate = 100000,
};

static struct imxi2c_platform_data mx6_ntx_i2c2_data = {
	.bitrate = 100000,
};

static struct i2c_board_info i2c_zforce_ir_touch_binfo = {
	.type = "zforce-ir-touch",
	.addr = 0x50,
 	//.platform_data = MX6SL_IR_TOUCH_INT,
 	//.irq = gpio_to_irq(MX6SL_IR_TOUCH_INT),
};

static struct i2c_board_info i2c_elan_touch_binfo = {
	 .type = "elan-touch",
	 .addr = 0x15,
};

static int fts_power_on (bool on) {return 0;}
static int fts_power_init (bool on) {return 0;}

static struct fts_ts_platform_data fts_data = {
	.info.delay_aa = 50,
	.info.delay_55 = 30,			
	.info.upgrade_id_1 = 0x79,
	.info.upgrade_id_2 = 0x03,
	.info.delay_readid = 10,
	.info.delay_erase_flash = 2000,
	.power_on = fts_power_on,
	.power_init = fts_power_init,
	.soft_rst_dly = 150,
	.hard_rst_dly = 20,
	.x_max = 1448,
	.y_max = 1072,
};

static struct i2c_board_info i2c_fts_touch_binfo = {
	 .type = "fts_ts",
	 .addr = 0x38,
	 .platform_data = &fts_data,
};

static struct i2c_board_info i2c_alc5640_codec_binfo = {
	.type = "rt5640",
	.addr = 0x1C,
};

static struct lm3630a_platform_data lm3630a_data = {
	/* led a config. */
	.leda_init_brt=0,
	.leda_max_brt=LM3630A_MAX_BRIGHTNESS,
	.leda_full_scale=LM3630A_DEF_FULLSCALE,
	.leda_ctrl=LM3630A_LEDA_ENABLE,
	/* led b config. */
	.ledb_init_brt=0,
	.ledb_max_brt=LM3630A_MAX_BRIGHTNESS,
	.ledb_full_scale=LM3630A_DEF_FULLSCALE,
	.ledb_ctrl=LM3630A_LEDB_ENABLE,

	.pwm_ctrl=LM3630A_PWM_DISABLE,
//	.pwm_ctrl=LM3630A_PWM_BANK_ALL,
};

static struct i2c_board_info i2c_lm3630a_bl_binfo = {
	 .type = LM3630A_NAME,
	 .addr = 0x36,
	 .platform_data = &lm3630a_data,
};
static struct i2c_board_info i2c_lm3630a_bl_binfo2 = {
	 .type = LM3630A_NAME,
	 .addr = 0x38,
	 .platform_data = &lm3630a_data,
};

/* cyttsp */
#define CYTTSP5_USE_I2C
/* #define CYTTSP5_USE_SPI */

#ifdef CYTTSP5_USE_I2C
#define CYTTSP5_I2C_TCH_ADR 0x24
#define CYTTSP5_LDR_TCH_ADR 0x24
#define CYTTSP5_I2C_IRQ_GPIO 134  // 5_6
#define CYTTSP5_I2C_RST_GPIO 141  // 5_13
#endif


#ifndef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP5_DEVICETREE_SUPPORT

#define CYTTSP5_HID_DESC_REGISTER 1

#define CY_VKEYS_X 720
#define CY_VKEYS_Y 1280
#define CY_MAXX 880
#define CY_MAXY 1280
#define CY_MINX 0
#define CY_MINY 0

#define CY_ABS_MIN_X CY_MINX
#define CY_ABS_MIN_Y CY_MINY
#define CY_ABS_MAX_X CY_MAXX
#define CY_ABS_MAX_Y CY_MAXY
#define CY_ABS_MIN_P 0
#define CY_ABS_MAX_P 255
#define CY_ABS_MIN_W 0
#define CY_ABS_MAX_W 255
#define CY_PROXIMITY_MIN_VAL	0
#define CY_PROXIMITY_MAX_VAL	1

#define CY_ABS_MIN_T 0

#define CY_ABS_MAX_T 15

/* Button to keycode conversion */
static u16 cyttsp5_btn_keys[] = {
	/* use this table to map buttons to keycodes (see input.h) */
	KEY_HOMEPAGE,		/* 172 */ /* Previously was KEY_HOME (102) */
				/* New Android versions use KEY_HOMEPAGE */
	KEY_MENU,		/* 139 */
	KEY_BACK,		/* 158 */
	KEY_SEARCH,		/* 217 */
	KEY_VOLUMEDOWN,		/* 114 */
	KEY_VOLUMEUP,		/* 115 */
	KEY_CAMERA,		/* 212 */
	KEY_POWER		/* 116 */
};

static struct touch_settings cyttsp5_sett_btn_keys = {
	.data = (uint8_t *)&cyttsp5_btn_keys[0],
	.size = ARRAY_SIZE(cyttsp5_btn_keys),
	.tag = 0,
};

static struct cyttsp5_core_platform_data _cyttsp5_core_platform_data = {
	.irq_gpio = CYTTSP5_I2C_IRQ_GPIO,
	.rst_gpio = CYTTSP5_I2C_RST_GPIO,
	.hid_desc_register = CYTTSP5_HID_DESC_REGISTER,
	.xres = cyttsp5_xres,
	.init = cyttsp5_init,
	.power = cyttsp5_power,
	.detect = cyttsp5_detect,
	.irq_stat = cyttsp5_irq_stat,
	.sett = {
		NULL,	/* Reserved */
		NULL,	/* Command Registers */
		NULL,	/* Touch Report */
		NULL,	/* Parade Data Record */
		NULL,	/* Test Record */
		NULL,	/* Panel Configuration Record */
		NULL,   /* &cyttsp5_sett_param_regs, */
		NULL,	/* &cyttsp5_sett_param_size, */
		NULL,	/* Reserved */
		NULL,	/* Reserved */
		NULL,	/* Operational Configuration Record */
		NULL, /* &cyttsp5_sett_ddata, *//* Design Data Record */
		NULL, /* &cyttsp5_sett_mdata, *//* Manufacturing Data Record */
		NULL,	/* Config and Test Registers */
		&cyttsp5_sett_btn_keys,	/* button-to-keycode table */
	},
	.flags = CY_CORE_FLAG_RESTORE_PARAMETERS,
	.easy_wakeup_gesture = CY_CORE_EWG_NONE,
};

static const int16_t cyttsp5_abs[] = {
	ABS_MT_POSITION_X, CY_ABS_MIN_X, CY_ABS_MAX_X, 0, 0,
	ABS_MT_POSITION_Y, CY_ABS_MIN_Y, CY_ABS_MAX_Y, 0, 0,
	ABS_MT_PRESSURE, CY_ABS_MIN_P, CY_ABS_MAX_P, 0, 0,
	CY_IGNORE_VALUE, CY_ABS_MIN_W, CY_ABS_MAX_W, 0, 0,
	ABS_MT_TRACKING_ID, CY_ABS_MIN_T, CY_ABS_MAX_T, 0, 0,
	ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0,
	ABS_MT_TOUCH_MINOR, 0, 255, 0, 0,
	ABS_MT_ORIENTATION, -127, 127, 0, 0,
	ABS_MT_TOOL_TYPE, 0, MT_TOOL_MAX, 0, 0,
	ABS_MT_DISTANCE, 0, 255, 0, 0,	/* Used with hover */
};

struct touch_framework cyttsp5_framework = {
	.abs = (uint16_t *)&cyttsp5_abs[0],
	.size = ARRAY_SIZE(cyttsp5_abs),
	.enable_vkeys = 0,
};

static struct cyttsp5_mt_platform_data _cyttsp5_mt_platform_data = {
	.frmwrk = &cyttsp5_framework,
	.flags = CY_MT_FLAG_INV_X,
	.inp_dev_name = CYTTSP5_MT_NAME,
	.vkeys_x = CY_VKEYS_X,
	.vkeys_y = CY_VKEYS_Y,
};

static struct cyttsp5_btn_platform_data _cyttsp5_btn_platform_data = {
	.inp_dev_name = CYTTSP5_BTN_NAME,
};

static const int16_t cyttsp5_prox_abs[] = {
	ABS_DISTANCE, CY_PROXIMITY_MIN_VAL, CY_PROXIMITY_MAX_VAL, 0, 0,
};

struct touch_framework cyttsp5_prox_framework = {
	.abs = (uint16_t *)&cyttsp5_prox_abs[0],
	.size = ARRAY_SIZE(cyttsp5_prox_abs),
};

static struct cyttsp5_proximity_platform_data
		_cyttsp5_proximity_platform_data = {
	.frmwrk = &cyttsp5_prox_framework,
	.inp_dev_name = CYTTSP5_PROXIMITY_NAME,
};

static struct cyttsp5_platform_data _cyttsp5_platform_data = {
	.core_pdata = &_cyttsp5_core_platform_data,
	.mt_pdata = &_cyttsp5_mt_platform_data,
	.loader_pdata = &_cyttsp5_loader_platform_data,
	.btn_pdata = &_cyttsp5_btn_platform_data,
	.prox_pdata = &_cyttsp5_proximity_platform_data,
};

static ssize_t cyttsp5_virtualkeys_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf,
		__stringify(EV_KEY) ":"
		__stringify(KEY_BACK) ":1360:90:160:180"
		":" __stringify(EV_KEY) ":"
		__stringify(KEY_MENU) ":1360:270:160:180"
		":" __stringify(EV_KEY) ":"
		__stringify(KEY_HOMEPAGE) ":1360:450:160:180"
		":" __stringify(EV_KEY) ":"
		__stringify(KEY_SEARCH) ":1360:630:160:180"
		"\n");
}

static struct kobj_attribute cyttsp5_virtualkeys_attr = {
	.attr = {
		.name = "virtualkeys.cyttsp5_mt",
		.mode = S_IRUGO,
	},
	.show = &cyttsp5_virtualkeys_show,
};

static struct attribute *cyttsp5_properties_attrs[] = {
	&cyttsp5_virtualkeys_attr.attr,
	NULL
};

static struct attribute_group cyttsp5_properties_attr_group = {
	.attrs = cyttsp5_properties_attrs,
};
#endif /* !CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP5_DEVICETREE_SUPPORT */

static void __init ntx_cyttsp5_init(void)
#ifndef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP5_DEVICETREE_SUPPORT
{
	struct kobject *properties_kobj;
	int ret = 0;

	/* Initialize muxes for GPIO pins */
	mxc_iomux_v3_setup_pad(MX6SL_PAD_SD1_DAT3__GPIO_5_6_PULLHIGH);

	properties_kobj = kobject_create_and_add("board_properties", NULL);
	if (properties_kobj)
		ret = sysfs_create_group(properties_kobj,
				&cyttsp5_properties_attr_group);
	if (!properties_kobj || ret)
		pr_err("%s: failed to create board_properties\n", __func__);
}
#else /* CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP5_DEVICETREE_SUPPORT */
{
	mxc_iomux_v3_setup_pad(MX6SL_PAD_SD1_DAT3__GPIO_5_6_PULLHIGH);
}
#endif /* CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP5_DEVICETREE_SUPPORT */

static struct i2c_board_info i2c_cyttsp_binfo = {
	.type = CYTTSP5_I2C_NAME,
	.addr = 0x24,
 	.irq = CYTTSP5_I2C_IRQ_GPIO,
	.platform_data = &_cyttsp5_platform_data,
};

int ricoh619_init_port(int irq_num) 
{
	printk("[%s-%d] ...\n",__func__,__LINE__);
	return 0;
}
			
static struct ricoh619_rtc_platform_data ricoh_rtc_data = {
	.time = {
		.tm_year = 1970,
		.tm_mon = 0,
		.tm_mday = 1,
		.tm_hour = 0,
		.tm_min = 0,
		.tm_sec = 0,
	},
};

static struct ricoh619_battery_platform_data ricoh_battery_data = {
//	.irq 		= RICOH619_IRQ_BASE,
	.alarm_vol_mv 	= 3400,
	// .adc_channel = RICOH619_ADC_CHANNEL_VBAT,
	.multiple	= 100, //100%
	.monitor_time 	= 60,
		/* some parameter is depend of battery type */
	.type[0] = {
		.ch_vfchg 	= 0xFF,	/* VFCHG	= 0 - 4 (4.05v, 4.10v, 4.15v, 4.20v, 4.35v) */
		.ch_vrchg 	= 0xFF,	/* VRCHG	= 0 - 4 (3.85v, 3.90v, 3.95v, 4.00v, 4.10v) */
		.ch_vbatovset 	= 0xFF,	/* VBATOVSET	= 0 or 1 (0 : 4.38v(up)/3.95v(down) 1: 4.53v(up)/4.10v(down)) */
		.ch_ichg 	= 0xFF,	/* ICHG		= 0 - 0x1D (100mA - 3000mA) */
		.ch_ilim_adp 	= 0xFF,	/* ILIM_ADP	= 0 - 0x1D (100mA - 3000mA) */
		.ch_ilim_usb 	= 0xFF,	/* ILIM_USB	= 0 - 0x1D (100mA - 3000mA) */
		.ch_icchg 	= 0,	/* ICCHG	= 0 - 3 (50mA 100mA 150mA 200mA) */
		.fg_target_vsys = 0,	/* This value is the target one to DSOC=0% */
		.fg_target_ibat = 0, /* This value is the target one to DSOC=0% */
		.fg_poff_vbat 	= 3520, 	/* setting value of 0 per Vbat */
		.jt_en 		= 0,	/* JEITA Enable	  = 0 or 1 (1:enable, 0:disable) */
		.jt_hw_sw 	= 1,	/* JEITA HW or SW = 0 or 1 (1:HardWare, 0:SoftWare) */
		.jt_temp_h 	= 50,	/* degree C */
		.jt_temp_l 	= 12,	/* degree C */
		.jt_vfchg_h 	= 0x03,	/* VFCHG High  	= 0 - 4 (4.05v, 4.10v, 4.15v, 4.20v, 4.35v) */
		.jt_vfchg_l 	= 0,	/* VFCHG High  	= 0 - 4 (4.05v, 4.10v, 4.15v, 4.20v, 4.35v) */
		.jt_ichg_h 	= 0x0D,	/* VFCHG Low  	= 0 - 4 (4.05v, 4.10v, 4.15v, 4.20v, 4.35v) */
		.jt_ichg_l 	= 0x09,	/* ICHG Low   	= 0 - 0x1D (100mA - 3000mA) */
	},
	/*
	.type[1] = {
		.ch_vfchg = 0x0,
		.ch_vrchg = 0x0,
		.ch_vbatovset = 0x0,
		.ch_ichg = 0x0,
		.ch_ilim_adp = 0x0,
		.ch_ilim_usb = 0x0,
		.ch_icchg = 0x00,
		.fg_target_vsys = 3300,//3000,
		.fg_target_ibat = 1000,//1000,
		.jt_en = 0,
		.jt_hw_sw = 1,
		.jt_temp_h = 40,
		.jt_temp_l = 10,
		.jt_vfchg_h = 0x0,
		.jt_vfchg_l = 0,
		.jt_ichg_h = 0x01,
		.jt_ichg_l = 0x01,
	},
	*/

/*  JEITA Parameter
*
*          VCHG  
*            |     
* jt_vfchg_h~+~~~~~~~~~~~~~~~~~~~+
*            |                   |
* jt_vfchg_l-| - - - - - - - - - +~~~~~~~~~~+
*            |    Charge area    +          |               
*  -------0--+-------------------+----------+--- Temp
*            !                   +
*          ICHG     
*            |                   +
*  jt_ichg_h-+ - -+~~~~~~~~~~~~~~+~~~~~~~~~~+
*            +    |              +          |
*  jt_ichg_l-+~~~~+   Charge area           |
*            |    +              +          |
*         0--+----+--------------+----------+--- Temp
*            0   jt_temp_l      jt_temp_h   55
*/	
};

#define RICOH_REG(_id, _name, _sname)				\
{								\
	.id		= RICOH619_ID_##_id,			\
	.name		= "ricoh61x-regulator",			\
	.platform_data	= &pdata_##_name##_##_sname,		\
}

static struct regulator_consumer_supply ricoh619_dc1_supply_0[] = {
	REGULATOR_SUPPLY("vdd_core2_1v3_soc", NULL),
};
static struct regulator_consumer_supply ricoh619_dc2_supply_0[] = {
	REGULATOR_SUPPLY("vdd_core1_3v3", NULL),
};
static struct regulator_consumer_supply ricoh619_dc3_supply_0[] = {
	REGULATOR_SUPPLY("vdd_core2_1v3_arm", NULL),
};
static struct regulator_consumer_supply ricoh619_dc4_supply_0[] = {
	REGULATOR_SUPPLY("vdd_core4_1v2", NULL),
};
static struct regulator_consumer_supply ricoh619_dc5_supply_0[] = {
	REGULATOR_SUPPLY("vdd_core4_1v8", NULL),
};
static struct regulator_consumer_supply ricoh619_ldo1_supply_0[] = {
	REGULATOR_SUPPLY("vdd_ir_3v3", NULL),
};
static struct regulator_consumer_supply ricoh619_ldo2_supply_0[] = {
};
static struct regulator_consumer_supply ricoh619_ldo3_supply_0[] = {
	REGULATOR_SUPPLY("vdd_core5_1v2", NULL),
};
static struct regulator_consumer_supply ricoh619_ldo4_supply_0[] = {
};
static struct regulator_consumer_supply ricoh619_ldo5_supply_0[] = {
	REGULATOR_SUPPLY("vdd_spd_3v3", NULL),
//	REGULATOR_SUPPLY("vmmc", "sdhci-esdhc-imx.3"),
};
static struct regulator_consumer_supply ricoh619_ldo6_supply_0[] = {
	REGULATOR_SUPPLY("vdd_ddr_0v6", NULL),
};
static struct regulator_consumer_supply ricoh619_ldo7_supply_0[] = {
	REGULATOR_SUPPLY("vdd_pwm", "0-0007"),
	REGULATOR_SUPPLY("vdd_fl_0_pwm", "0-0043"),
	REGULATOR_SUPPLY("vdd_fl_pwm", "2-0043"),
	REGULATOR_SUPPLY("vdd_fl_lm3630a", "0-0036"),
};
static struct regulator_consumer_supply ricoh619_ldo8_supply_0[] = {
	REGULATOR_SUPPLY("ldo_1v8", NULL),
};
static struct regulator_consumer_supply ricoh619_ldo9_supply_0[] = {
};
static struct regulator_consumer_supply ricoh619_ldo10_supply_0[] = {
};
static struct regulator_consumer_supply ricoh619_ldortc1_supply_0[] = {
	REGULATOR_SUPPLY("vdd_rtc_3v3", NULL),
};
static struct regulator_consumer_supply ricoh619_ldortc2_supply_0[] = {
};

#define RICOH_PDATA_INIT(_name, _sname, _minmv, _maxmv, _supply_reg, _always_on, \
	_boot_on, _apply_uv, _init_uV, _init_enable, _init_apply, _flags,      \
	_ext_contol, _sleep_mV) \
	static struct ricoh619_regulator_platform_data pdata_##_name##_##_sname = \
	{								\
		.regulator = {						\
			.constraints = {				\
				.min_uV = (_minmv)*1000,		\
				.max_uV = (_maxmv)*1000,		\
				.valid_modes_mask = (REGULATOR_MODE_NORMAL |  \
						     REGULATOR_MODE_STANDBY), \
				.valid_ops_mask = (REGULATOR_CHANGE_MODE |    \
						   REGULATOR_CHANGE_STATUS |  \
						   REGULATOR_CHANGE_VOLTAGE), \
				.always_on = _always_on,		\
				.boot_on = _boot_on,			\
				.apply_uV = _apply_uv,			\
			},						\
			.num_consumer_supplies =			\
				ARRAY_SIZE(ricoh619_##_name##_supply_##_sname),	\
			.consumer_supplies = ricoh619_##_name##_supply_##_sname, \
			.supply_regulator = _supply_reg,		\
		},							\
		.init_uV =  _init_uV * 1000,				\
		.init_enable = _init_enable,				\
		.init_apply = _init_apply,				\
		.sleep_uV = (_sleep_mV)*1000,				\
		.flags = _flags,					\
		.ext_pwr_req = _ext_contol,				\
	}

RICOH_PDATA_INIT(dc1, 0,	0,  0, 0, 1, 1, 0, -1, 1, 1, 0, 0, 950);	// Core2_1V3
RICOH_PDATA_INIT(dc2, 0,	0,  0, 0, 1, 1, 0, -1, 1, 1, 0, 0, 0);	// Core3_3V3
RICOH_PDATA_INIT(dc3, 0,	950,  1425, 0, 1, 1, 0, 1425, 1, 1, 0, 0, 950);	// Core2_1V3_ARM
RICOH_PDATA_INIT(dc4, 0,	0,  0, 0, 1, 1, 0, -1, 1, 1, 0, 0, 0);	// Core4_1V2
RICOH_PDATA_INIT(dc5, 0,	0,  0, 0, 1, 1, 0, -1, 1, 1, 0, 0, 0);	// Core4_1V8

RICOH_PDATA_INIT(ldo1, 0,	0, 0, 0, 1, 1, 0, -1, 1, 1, 0, 0, 0);		// IR_3V3
RICOH_PDATA_INIT(ldo2, 0,	0, 0, 0, 1, 1, 0, -1, 1, 1, 0, 0, 0);		// Core1_3V3*
RICOH_PDATA_INIT(ldo3, 0,	0, 0, 0, 1, 1, 0, -1, 1, 1, 0, 0, 0);		// Core5_1V2
RICOH_PDATA_INIT(ldo4, 0,	0, 0, 0, 0, 0, 0, -1, 0, 1, 0, 0, 0);		// Reserved
RICOH_PDATA_INIT(ldo5, 0,	0, 0, 0, 1, 1, 0, -1, 1, 1, 0, 0, 0);		// SPD_3V3
RICOH_PDATA_INIT(ldo6, 0,	0, 0, 0, 1, 1, 0, -1, 1, 1, 0, 0, 0);		// DDR_0V6
RICOH_PDATA_INIT(ldo7, 0,	0, 0, 0, 0, 0, 0, -1, 0, 1, 0, 0, 0); 		// VDD_PWM
RICOH_PDATA_INIT(ldo8, 0,	0, 0, 0, 0, 0, 0, -1, 0, 1, 0, 0, 0);		// ldo_1v8
//RICOH_PDATA_INIT(ldo8, 0,	3300, 3300, 0, 1, 1, 0, 3300, 1, 1, 0, 0, 3300);		// ldo_1v8 output 3V3 
RICOH_PDATA_INIT(ldo9, 0,	0, 0, 0, 0, 0, 0, -1, 0, 1, 0, 0, 0);		// Reserved
RICOH_PDATA_INIT(ldo10, 0,	0, 0, 0, 0, 0, 0, -1, 0, 1, 0, 0, 0);		// Reserved
RICOH_PDATA_INIT(ldortc1, 0,0, 0, 0, 1, 1, 0, -1, 1, 1, -1, -1, -1);
RICOH_PDATA_INIT(ldortc2, 0,0, 0, 0, 0, 0, 0, -1, 0, 1, -1, -1, -1);

#define RICOH619_DEV_REG 		\
	RICOH_REG(DC1, dc1, 0),		\
	RICOH_REG(DC2, dc2, 0),		\
	RICOH_REG(DC3, dc3, 0),		\
	RICOH_REG(DC4, dc4, 0),		\
	RICOH_REG(DC5, dc5, 0),		\
	RICOH_REG(LDO1, ldo1, 0),	\
	RICOH_REG(LDO2, ldo2, 0),	\
	RICOH_REG(LDO3, ldo3, 0),	\
	RICOH_REG(LDO4, ldo4, 0),	\
	RICOH_REG(LDO5, ldo5, 0),	\
	RICOH_REG(LDO6, ldo6, 0),	\
	RICOH_REG(LDO7, ldo7, 0),	\
	RICOH_REG(LDO8, ldo8, 0),	\
	RICOH_REG(LDO9, ldo9, 0),	\
	RICOH_REG(LDO10, ldo10, 0),	\
	RICOH_REG(LDORTC1, ldortc1, 0),	\
	RICOH_REG(LDORTC2, ldortc2, 0)

struct	ricoh619_subdev_info ricoh619_sub_data[] = {
	RICOH619_DEV_REG,
	{
		.name           = "ricoh619-battery",
		.id				= -1,
		.platform_data = &ricoh_battery_data,
	},
	{
		.name           = "rtc_ricoh619",
		.id				= 0,
		.platform_data = &ricoh_rtc_data,
	},
};


#define RICOH_GPIO_INIT(_init_apply, _output_mode, _output_val, _led_mode, _led_func) \
	{									\
		.output_mode_en = _output_mode,	\
		.output_val		= _output_val,	\
		.init_apply		= _init_apply,	\
		.led_mode		= _led_mode,	\
		.led_func		= _led_func,	\
	}
struct ricoh619_gpio_init_data ricoh_gpio_data[] = {
	RICOH_GPIO_INIT(false, false, 0, 0, 0),
	RICOH_GPIO_INIT(false, false, 0, 0, 0),
	RICOH_GPIO_INIT(false, false, 0, 0, 0),
	RICOH_GPIO_INIT(false, false, 0, 0, 0),
};

static struct ricoh619_platform_data ntx_ricoh_data = {
	.num_subdevs = ARRAY_SIZE(ricoh619_sub_data),
	.subdevs = ricoh619_sub_data, 
	.gpio_base		= 7*32,
	.gpio_init_data		= ricoh_gpio_data,
	.num_gpioinit_data	= ARRAY_SIZE(ricoh_gpio_data),
	.enable_shutdown_pin 	= true,
};

static struct i2c_board_info i2c_sysmp_ricoh619_binfo = {
	.type = "ricoh619",
	.addr = 0x32,
	.platform_data = &ntx_ricoh_data,
};

static struct i2c_board_info i2c_sysmp_msp430_binfo = {
	.type = "msp430",
	.addr = 0x43,
	//.irq = gpio_to_irq(MX6SL_MSP_INT),
};

static struct i2c_board_info i2c_ht68f20_binfo = {
	.type = "ht68f20",
	.addr = 0x07,
};

static struct si114x_platform_data si114x_data = {
	/* Interrupt */
	.pfd_gpio_int_no = MX6SL_SI114X_INT,
	.pfd_meas_rate = 0x0,
	.pfd_als_rate = 0x00,
	.pfd_ps_rate = 0x00,
	.pfd_ps_led1 = 0,
	.pfd_ps_led2 = 0,
	.pfd_ps_led3 = 0,
	/* input subsystem */
	.pfd_als_poll_interval = 0,
	.pfd_ps_poll_interval = 0,
};

static struct i2c_board_info i2c_si114x_binfo = {
	.type = "si114x",
	.addr = 0x5A,
	.irq = gpio_to_irq(MX6SL_SI114X_INT),
	.platform_data = &si114x_data,
};

static struct i2c_board_info i2c_kl25_binfo = {
	.type = "kl25",
	.addr = 0x5A,
	.irq = gpio_to_irq(MX6SL_KL25_INT2),
};

static struct i2c_board_info i2c_kl25_2_binfo = {
	.type = "kl25_2",
	.addr = 0x5B,
	.irq = gpio_to_irq(MX6SL_P2_KL25_INT2),
};

static struct i2c_board_info i2c_mma8652_binfo = {
	.type = "mma8652",
	.addr = 0x1D,
	.irq = gpio_to_irq(MX6SL_KL25_INT2),
};

static struct i2c_board_info i2c_wacom_binfo = {
	.type = "wacom_i2c",
	.addr = 0x09,
	.irq = gpio_to_irq(MX6SL_WACOM_INT),
};

static struct i2c_board_info __initdata i2c_waltop_binfo = {
	.type = "waltop_i2c",
	.addr = 0x37,
	.platform_data = MX6SL_WALTOP_RST,
	.irq = gpio_to_irq(MX6SL_WALTOP_INT),
};
#if 0
static struct i2c_board_info mxc_i2c1_board_info[] __initdata = {
};
static struct i2c_board_info mxc_i2c2_board_info[] __initdata = {
#if 0
	{
		I2C_BOARD_INFO("ov5640", 0x3c),
		.platform_data = (void *)&camera_data,
	},
#endif
	{
	 	.type = "msp430",
	 	.addr = 0x43,
	 	.irq = gpio_to_irq(MX6SL_MSP_INT),
	},
};
#endif

static struct ntx_misc_platform_data ntx_misc_info;

static struct platform_device ntx_charger = {
	.name	= "pmic_battery",
	.id = 1,
	.dev            = {
		.platform_data = &ntx_misc_info,
	}
};

static struct platform_device ntx_light_ldm = {
	.name = "pmic_light",
	.id = 1,
};

static struct mxc_dvfs_platform_data mx6sl_ntx_dvfscore_data = {
#ifdef CONFIG_MX6_INTER_LDO_BYPASS
	.reg_id			= "VDDCORE",
	.soc_id			= "VDDSOC",
#else
	.reg_id			= "cpu_vddgp",
	.soc_id			= "cpu_vddsoc",
	.pu_id			= "cpu_vddvpu",
#endif
	.clk1_id		= "cpu_clk",
	.clk2_id		= "gpc_dvfs_clk",
	.gpc_cntr_offset	= MXC_GPC_CNTR_OFFSET,
	.ccm_cdcr_offset	= MXC_CCM_CDCR_OFFSET,
	.ccm_cacrr_offset	= MXC_CCM_CACRR_OFFSET,
	.ccm_cdhipr_offset	= MXC_CCM_CDHIPR_OFFSET,
	.prediv_mask		= 0x1F800,
	.prediv_offset		= 11,
	.prediv_val		= 3,
	.div3ck_mask		= 0xE0000000,
	.div3ck_offset		= 29,
	.div3ck_val		= 2,
	.emac_val		= 0x08,
	.upthr_val		= 25,
	.dnthr_val		= 9,
	.pncthr_val		= 33,
	.upcnt_val		= 10,
	.dncnt_val		= 10,
	.delay_time		= 80,
};

static struct viv_gpu_platform_data imx6q_gpu_pdata __initdata = {
	.reserved_mem_size = SZ_32M,
};

void __init early_console_setup(unsigned long base, struct clk *clk);

static const struct imxuart_platform_data mx6sl_ntx_uart1_data __initconst = {
	.flags      = IMXUART_HAVE_RTSCTS | IMXUART_SDMA,
	.dma_req_rx = MX6Q_DMA_REQ_UART2_RX,
	.dma_req_tx = MX6Q_DMA_REQ_UART2_TX,
};

static inline void mx6_ntx_init_uart(void)
{
	imx6q_add_imx_uart(0, NULL); /* DEBUG UART1 */
}

#if 0
static int mx6sl_ntx_fec_phy_init(struct phy_device *phydev)
{
	int val;

	/* power on FEC phy and reset phy */
	gpio_request(MX6_BRD_FEC_PWR_EN, "fec-pwr");
	gpio_direction_output(MX6_BRD_FEC_PWR_EN, 0);
	/* wait RC ms for hw reset */
	msleep(1);
	gpio_direction_output(MX6_BRD_FEC_PWR_EN, 1);

	/* check phy power */
	val = phy_read(phydev, 0x0);
	if (val & BMCR_PDOWN)
		phy_write(phydev, 0x0, (val & ~BMCR_PDOWN));

	return 0;
}

static struct fec_platform_data fec_data __initdata = {
	.init = mx6sl_ntx_fec_phy_init,
	.phy = PHY_INTERFACE_MODE_RMII,
};
#endif

static int epdc_get_pins(void)
{
	int ret = 0;

	/* Claim GPIOs for EPDC pins - used during power up/down */
	ret |= gpio_request(MX6SL_BRD_EPDC_SDDO_0, "epdc_d0");
	ret |= gpio_request(MX6SL_BRD_EPDC_SDDO_1, "epdc_d1");
	ret |= gpio_request(MX6SL_BRD_EPDC_SDDO_2, "epdc_d2");
	ret |= gpio_request(MX6SL_BRD_EPDC_SDDO_3, "epdc_d3");
	ret |= gpio_request(MX6SL_BRD_EPDC_SDDO_4, "epdc_d4");
	ret |= gpio_request(MX6SL_BRD_EPDC_SDDO_5, "epdc_d5");
	ret |= gpio_request(MX6SL_BRD_EPDC_SDDO_6, "epdc_d6");
	ret |= gpio_request(MX6SL_BRD_EPDC_SDDO_7, "epdc_d7");
	ret |= gpio_request(MX6SL_BRD_EPDC_SDDO_8 , "epdc_d8");
	ret |= gpio_request(MX6SL_BRD_EPDC_SDDO_9 , "epdc_d9");
	ret |= gpio_request(MX6SL_BRD_EPDC_SDDO_10, "epdc_d10");
	ret |= gpio_request(MX6SL_BRD_EPDC_SDDO_11, "epdc_d11");
	ret |= gpio_request(MX6SL_BRD_EPDC_SDDO_12, "epdc_d12");
	ret |= gpio_request(MX6SL_BRD_EPDC_SDDO_13, "epdc_d13");
	ret |= gpio_request(MX6SL_BRD_EPDC_SDDO_14, "epdc_d14");
	ret |= gpio_request(MX6SL_BRD_EPDC_SDDO_15, "epdc_d15");
	ret |= gpio_request(MX6SL_BRD_EPDC_GDCLK, "epdc_gdclk");
	ret |= gpio_request(MX6SL_BRD_EPDC_GDSP, "epdc_gdsp");
	ret |= gpio_request(MX6SL_BRD_EPDC_GDOE, "epdc_gdoe");
	ret |= gpio_request(MX6SL_BRD_EPDC_GDRL, "epdc_gdrl");
	ret |= gpio_request(MX6SL_BRD_EPDC_SDCLK, "epdc_sdclk");
	ret |= gpio_request(MX6SL_BRD_EPDC_SDOE, "epdc_sdoe");
	ret |= gpio_request(MX6SL_BRD_EPDC_SDLE, "epdc_sdle");
	ret |= gpio_request(MX6SL_BRD_EPDC_SDSHR, "epdc_sdshr");
	ret |= gpio_request(MX6SL_BRD_EPDC_BDR0, "epdc_bdr0");
	ret |= gpio_request(MX6SL_BRD_EPDC_SDCE0, "epdc_sdce0");
	ret |= gpio_request(MX6SL_BRD_EPDC_SDCE1, "epdc_sdce1");
//	ret |= gpio_request(MX6SL_BRD_EPDC_SDCE2, "epdc_sdce2");

	return ret;
}

static void epdc_put_pins(void)
{
	gpio_free(MX6SL_BRD_EPDC_SDDO_0);
	gpio_free(MX6SL_BRD_EPDC_SDDO_1);
	gpio_free(MX6SL_BRD_EPDC_SDDO_2);
	gpio_free(MX6SL_BRD_EPDC_SDDO_3);
	gpio_free(MX6SL_BRD_EPDC_SDDO_4);
	gpio_free(MX6SL_BRD_EPDC_SDDO_5);
	gpio_free(MX6SL_BRD_EPDC_SDDO_6);
	gpio_free(MX6SL_BRD_EPDC_SDDO_7);
	gpio_free(MX6SL_BRD_EPDC_SDDO_8); 
	gpio_free(MX6SL_BRD_EPDC_SDDO_9); 
	gpio_free(MX6SL_BRD_EPDC_SDDO_10);
	gpio_free(MX6SL_BRD_EPDC_SDDO_11);
	gpio_free(MX6SL_BRD_EPDC_SDDO_12);
	gpio_free(MX6SL_BRD_EPDC_SDDO_13);
	gpio_free(MX6SL_BRD_EPDC_SDDO_14);
	gpio_free(MX6SL_BRD_EPDC_SDDO_15);
	gpio_free(MX6SL_BRD_EPDC_GDCLK);
	gpio_free(MX6SL_BRD_EPDC_GDSP);
	gpio_free(MX6SL_BRD_EPDC_GDOE);
	gpio_free(MX6SL_BRD_EPDC_GDRL);
	gpio_free(MX6SL_BRD_EPDC_SDCLK);
	gpio_free(MX6SL_BRD_EPDC_SDOE);
	gpio_free(MX6SL_BRD_EPDC_SDLE);
	gpio_free(MX6SL_BRD_EPDC_SDSHR);
	gpio_free(MX6SL_BRD_EPDC_BDR0);
	gpio_free(MX6SL_BRD_EPDC_SDCE0);
	gpio_free(MX6SL_BRD_EPDC_SDCE1);
//	gpio_free(MX6SL_BRD_EPDC_SDCE2);
}

static void mxc_pads_lve_setup(iomux_v3_cfg_t *pad_list, unsigned count,int iIsLVE) 
{
	iomux_v3_cfg_t tPadctrlOld;
	iomux_v3_cfg_t tPadctrl;
	iomux_v3_cfg_t *p = pad_list;

	int i;

	for(i=0;i<count;i++) {
		tPadctrlOld = tPadctrl = *p ;

#if 0
		if(tPadctrl&NO_PAD_CTRL) {
			mxc_iomux_v3_get_pad(&tPadctrlOld);
			tPadctrl = tPadctrlOld;
			//tPadctrl &= ~((iomux_v3_cfg_t)NO_PAD_CTRL<<MUX_PAD_CTRL_SHIFT);
		}
#endif

		if(iIsLVE) {
			tPadctrl |=  ((iomux_v3_cfg_t)PAD_CTL_LVE<<MUX_PAD_CTRL_SHIFT);
		}
		else {
			tPadctrl &= ~((iomux_v3_cfg_t)PAD_CTL_LVE<<MUX_PAD_CTRL_SHIFT);
		}

		#if 0
		printk("PAD LVE 0x%llx:0x%llx->0x%llx \n",
				pad_list[i],tPadctrlOld,tPadctrl);
		#endif

		mxc_iomux_v3_setup_pad(tPadctrl);
		p++;
	}
}





static void epdc_enable_pins(void)
{
	/* Configure MUX settings to enable EPDC use */

	if(NTXHWCFG_TST_FLAG(gptHWCFG->m_val.bPCB_Flags,4)) {
		// Panel is designed for low voltage .
		mxc_pads_lve_setup(mx6sl_brd_epdc_enable_pads,\
			ARRAY_SIZE(mx6sl_brd_epdc_enable_pads),1);
	}
	else {
#if 0
		mxc_pads_lve_setup(mx6sl_brd_epdc_enable_pads,\
			ARRAY_SIZE(mx6sl_brd_epdc_enable_pads),0);
#else
		mxc_iomux_v3_setup_multiple_pads(mx6sl_brd_epdc_enable_pads, \
			ARRAY_SIZE(mx6sl_brd_epdc_enable_pads));
#endif
			
	}

	gpio_direction_input(MX6SL_BRD_EPDC_SDDO_0);
	gpio_direction_input(MX6SL_BRD_EPDC_SDDO_1);
	gpio_direction_input(MX6SL_BRD_EPDC_SDDO_2);
	gpio_direction_input(MX6SL_BRD_EPDC_SDDO_3);
	gpio_direction_input(MX6SL_BRD_EPDC_SDDO_4);
	gpio_direction_input(MX6SL_BRD_EPDC_SDDO_5);
	gpio_direction_input(MX6SL_BRD_EPDC_SDDO_6);
	gpio_direction_input(MX6SL_BRD_EPDC_SDDO_7);
	//if( 1==gptHWCFG->m_val.bDisplayBusWidth || 
	//		3==gptHWCFG->m_val.bDisplayBusWidth) 
	{
		// 16 bits .
		gpio_direction_input(MX6SL_BRD_EPDC_SDDO_8);
		gpio_direction_input(MX6SL_BRD_EPDC_SDDO_9);
		gpio_direction_input(MX6SL_BRD_EPDC_SDDO_10);
		gpio_direction_input(MX6SL_BRD_EPDC_SDDO_11);
		gpio_direction_input(MX6SL_BRD_EPDC_SDDO_12);
		gpio_direction_input(MX6SL_BRD_EPDC_SDDO_13);
		gpio_direction_input(MX6SL_BRD_EPDC_SDDO_14);
		gpio_direction_input(MX6SL_BRD_EPDC_SDDO_15);
	}
	gpio_direction_input(MX6SL_BRD_EPDC_GDCLK);
	gpio_direction_input(MX6SL_BRD_EPDC_GDSP);
	gpio_direction_input(MX6SL_BRD_EPDC_GDOE);
	gpio_direction_input(MX6SL_BRD_EPDC_GDRL);
	gpio_direction_input(MX6SL_BRD_EPDC_SDCLK);
	gpio_direction_input(MX6SL_BRD_EPDC_SDOE);
	gpio_direction_input(MX6SL_BRD_EPDC_SDLE);
	gpio_direction_input(MX6SL_BRD_EPDC_SDSHR);
	gpio_direction_input(MX6SL_BRD_EPDC_BDR0);
	gpio_direction_input(MX6SL_BRD_EPDC_SDCE0);
	gpio_direction_input(MX6SL_BRD_EPDC_SDCE1);
//	gpio_direction_input(MX6SL_BRD_EPDC_SDCE2);
}

static void epdc_disable_pins(void)
{
	/* Configure MUX settings for EPDC pins to
	 * GPIO and drive to 0. */
	mxc_iomux_v3_setup_multiple_pads(mx6sl_brd_epdc_disable_pads, \
				ARRAY_SIZE(mx6sl_brd_epdc_disable_pads));

	gpio_direction_output(MX6SL_BRD_EPDC_SDDO_0, 0);
	gpio_direction_output(MX6SL_BRD_EPDC_SDDO_1, 0);
	gpio_direction_output(MX6SL_BRD_EPDC_SDDO_2, 0);
	gpio_direction_output(MX6SL_BRD_EPDC_SDDO_3, 0);
	gpio_direction_output(MX6SL_BRD_EPDC_SDDO_4, 0);
	gpio_direction_output(MX6SL_BRD_EPDC_SDDO_5, 0);
	gpio_direction_output(MX6SL_BRD_EPDC_SDDO_6, 0);
	gpio_direction_output(MX6SL_BRD_EPDC_SDDO_7, 0);
	//if( 1==gptHWCFG->m_val.bDisplayBusWidth || 
	//		3==gptHWCFG->m_val.bDisplayBusWidth) 
	{
		// 16 bits .
		gpio_direction_output(MX6SL_BRD_EPDC_SDDO_8, 0);
		gpio_direction_output(MX6SL_BRD_EPDC_SDDO_9, 0);
		gpio_direction_output(MX6SL_BRD_EPDC_SDDO_10, 0);
		gpio_direction_output(MX6SL_BRD_EPDC_SDDO_11, 0);
		gpio_direction_output(MX6SL_BRD_EPDC_SDDO_12, 0);
		gpio_direction_output(MX6SL_BRD_EPDC_SDDO_13, 0);
		gpio_direction_output(MX6SL_BRD_EPDC_SDDO_14, 0);
		gpio_direction_output(MX6SL_BRD_EPDC_SDDO_15, 0);
	}
	gpio_direction_output(MX6SL_BRD_EPDC_GDCLK, 0);
	gpio_direction_output(MX6SL_BRD_EPDC_GDSP, 0);
	gpio_direction_output(MX6SL_BRD_EPDC_GDOE, 0);
	gpio_direction_output(MX6SL_BRD_EPDC_GDRL, 0);
	gpio_direction_output(MX6SL_BRD_EPDC_SDCLK, 0);
	gpio_direction_output(MX6SL_BRD_EPDC_SDOE, 0);
	gpio_direction_output(MX6SL_BRD_EPDC_SDLE, 0);
	gpio_direction_output(MX6SL_BRD_EPDC_SDSHR, 0);
	gpio_direction_output(MX6SL_BRD_EPDC_BDR0, 0);
	gpio_direction_output(MX6SL_BRD_EPDC_SDCE0, 0);
	gpio_direction_output(MX6SL_BRD_EPDC_SDCE1, 0);
//	gpio_direction_output(MX6SL_BRD_EPDC_SDCE2, 0);
}

#if 1 //[
static struct fb_videomode ed060sct_mode = {
.name = "E60SCT",
.refresh = 85,
.xres = 800,
.yres = 600,
.pixclock = 26680000,
.left_margin = 8,
.right_margin = 96,
.upper_margin = 4,
.lower_margin = 13,
.hsync_len = 4,
.vsync_len = 1,
.sync = 0,
.vmode = FB_VMODE_NONINTERLACED,
.flag = 0,
};

static struct fb_videomode ed060scq_mode = {
.name = "E60SCQ",
.refresh = 85,
.xres = 800,
.yres = 600,
.pixclock = 25000000,
.left_margin = 8,
.right_margin = 60,
.upper_margin = 4,
.lower_margin = 10,
.hsync_len = 8,
.vsync_len = 4,
.sync = 0,
.vmode = FB_VMODE_NONINTERLACED,
.flag = 0,
};

static struct fb_videomode ed060sc8_mode = {
.name = "E60SC8",
.refresh = 85,
.xres = 800,
.yres = 600,
.pixclock = 30000000,
.left_margin = 8,
.right_margin = 164,
.upper_margin = 4,
.lower_margin = 18,
.hsync_len = 4,
.vsync_len = 1,
.sync = 0,
.vmode = FB_VMODE_NONINTERLACED,
.flag = 0,
};

// for ED060XC5 release by Freescale Grace 20120726 .

static struct fb_videomode ed060xc1_mode = {
.name = "E60XC1",
.refresh = 85,
.xres = 1024,
.yres = 768,
.pixclock = 40000000,
.left_margin = 12,
.right_margin = 72,
.upper_margin = 4,
.lower_margin = 5,
.hsync_len = 8,
.vsync_len = 2,
.sync = 0,
.vmode = FB_VMODE_NONINTERLACED,
.flag = 0,
};

static struct fb_videomode ed060xc5_mode = {
.name = "E60XC5",
.refresh = 85,
.xres = 1024,
.yres = 758,
.pixclock = 40000000,
.left_margin = 12,
.right_margin = 76,
.upper_margin = 4,
.lower_margin = 5,
.hsync_len = 12,
.vsync_len = 2,
.sync = 0,
.vmode = FB_VMODE_NONINTERLACED,
.flag = 0,
};

static struct fb_videomode e60_v110_mode = {
.name = "E60_V110",
.refresh = 50,
.xres = 800,
.yres = 600,
.pixclock = 18604700,
.left_margin = 8,
.right_margin = 176,
.upper_margin = 4,
.lower_margin = 2,
.hsync_len = 4,
.vsync_len = 1,
.sync = 0,
.vmode = FB_VMODE_NONINTERLACED,
.flag = 0,
};

static struct fb_videomode ed050xxx_mode = {
	.name="ED050XXXX",
	.refresh=85,
	.xres=800,
	.yres=600,
	.pixclock=26666667,
	.left_margin=4,
	.right_margin=98,
	.upper_margin=4,
	.lower_margin=9,
	.hsync_len=8,
	.vsync_len=2,
	.sync=0,
	.vmode=FB_VMODE_NONINTERLACED,
	.flag=0,
};


#ifdef EPD_TIMING_ED068TG1 //[
static struct fb_videomode ed068tg1_mode = {
.name = "ED068TG1",
.refresh=85,
.xres=1440,
.yres=1080,
.pixclock=96000000,
.left_margin=24,
.right_margin=267,
.upper_margin=4,
.lower_margin=5,
.hsync_len=24,
.vsync_len=2,
.sync=0,
.vmode=FB_VMODE_NONINTERLACED,
.flag=0,
};
#elif defined(EPD_TIMING_ED068OG1_NUMCE3) //][
/* i.MX508 waveform data timing data structures for ed068og1_numce3 */
/* Created on - Monday, October 15, 2012 10:36:24
   Warning: this pixel clock is derived from 480 MHz parent! */

static struct fb_videomode ed068og1_numce3_mode = {
.name="ED068OG1_NUMCE3",
.refresh=85,
.xres=1440,
.yres=1080,
.pixclock=96000000,
.left_margin=24,
.right_margin=267,
.upper_margin=4,
.lower_margin=5,
.hsync_len=24,
.vsync_len=2,
.sync=0,
.vmode=FB_VMODE_NONINTERLACED,
.flag=0,
};
#else
static struct fb_videomode ed068og1_mode = {
.name = "E68OG1",
.refresh=85,
.xres=1440,
.yres=1080,
.pixclock=120000000,
.left_margin=32,
.right_margin=508,
.upper_margin=4,
.lower_margin=5,
.hsync_len=32,
.vsync_len=2,
.sync=0,
.vmode=FB_VMODE_NONINTERLACED,
.flag=0,
};
#endif//]EPD_TIMING_ED068OG1_NUMCE3

static struct fb_videomode peng060d_mode = {
.name = "PENG060D",
.refresh=85,
.xres=1448,
.yres=1072,
.pixclock=80000000,
.left_margin=16,
.right_margin=102,
.upper_margin=4,
.lower_margin=4,
.hsync_len=28,
.vsync_len=2,
.sync=0,
.vmode=FB_VMODE_NONINTERLACED,
.flag=0,
};

static struct fb_videomode ef133ut1sce_mode = {
.name="EF133UT1SCE",
.refresh=65,
.xres=1600,
.yres=1200,
.pixclock=72222223,
.left_margin=8,
.right_margin=97,
.upper_margin=4,
.lower_margin=7,
.hsync_len=12,
.vsync_len=1,
.sync=0,
.vmode=FB_VMODE_NONINTERLACED,
.flag=0,
};

static struct fb_videomode ed078kh1_mode = {
.name = "ED078KH1",
.refresh=85,
.xres=1872,
.yres=1404,
.pixclock=133400000,
.left_margin=44,
.right_margin=89,
.upper_margin=4,
.lower_margin=5,
.hsync_len=44,
.vsync_len=1,
.sync=0,
.vmode=FB_VMODE_NONINTERLACED,
.flag=0,
};



static struct imx_epdc_fb_mode panel_modes[] = {
////////////////////
{
& ed060sc8_mode,
4,            /* vscan_holdoff */
10,          /* sdoed_width */
20,          /* sdoed_delay */
10,          /* sdoez_width */
20,          /* sdoez_delay */
465,        /* gdclk_hp_offs */
250,        /* gdsp_offs changed delay to 8.3 uS */
0,            /* gdoe_offs */
8,            /* gdclk_offs changed delay to 4.5 SDCLK */
1,            /* num_ce */
},

////////////////////
{
& e60_v110_mode,
4,            /* vscan_holdoff */
10,          /* sdoed_width */
20,          /* sdoed_delay */
10,          /* sdoez_width */
20,          /* sdoez_delay */
465,        /* gdclk_hp_offs */
250,        /* gdsp_offs changed delay to 8.3 uS */
0,            /* gdoe_offs */
8,            /* gdclk_offs changed delay to 4.5 SDCLK */
1,            /* num_ce */
},

////////////////////
{
& ed060xc5_mode,
4,            /* vscan_holdoff */
10,          /* sdoed_width */
20,          /* sdoed_delay */
10,          /* sdoez_width */
20,          /* sdoez_delay */
524,        /* gdclk_hp_offs */
25,        /* gdsp_offs changed delay to 8.3 uS */
0,            /* gdoe_offs */
19,            /* gdclk_offs changed delay to 4.5 SDCLK */
1,            /* num_ce */
},

////////////////////
{
& ed060xc1_mode,
4,            /* vscan_holdoff */
10,          /* sdoed_width */
20,          /* sdoed_delay */
10,          /* sdoez_width */
20,          /* sdoez_delay */
492,        /* gdclk_hp_offs */
29,        /* gdsp_offs changed delay to 8.3 uS */
0,            /* gdoe_offs */
23,            /* gdclk_offs changed delay to 4.5 SDCLK */
1,            /* num_ce */
},

////////////////////
{
	&ed050xxx_mode, 	/* struct fb_videomode *mode */
		4, 	/* vscan_holdoff */
		10, 	/* sdoed_width */
		20, 	/* sdoed_delay */
		10, 	/* sdoez_width */
		20, 	/* sdoez_delay */
		420, 	/* GDCLK_HP */
		20, 	/* GDSP_OFF */
		0, 	/* GDOE_OFF */
		11, 	/* gdclk_offs */
		3, 	/* num_ce */
},	

#ifdef EPD_TIMING_ED068TG1 //[
{
& ed068tg1_mode,
4,            /* vscan_holdoff */
10,          /* sdoed_width */
20,          /* sdoed_delay */
10,          /* sdoez_width */
20,          /* sdoez_delay */
665,        /* GDCLK_HP */
718,        /* GDSP_OFF */
0,            /* GDOE_OFF */
199,        /* gdclk_offs */
1,            /* num_ce */
},
#elif defined(EPD_TIMING_ED068OG1_NUMCE3)//][
{
& ed068og1_numce3_mode,
4,            /* vscan_holdoff */
10,          /* sdoed_width */
20,          /* sdoed_delay */
10,          /* sdoez_width */
20,          /* sdoez_delay */
665,        /* GDCLK_HP */
210,        /* GDSP_OFF */
0,            /* GDOE_OFF */
199,        /* gdclk_offs */
3,            /* num_ce */
},
#else//][ !EPD_TIMING_ED068OG1_NUMCE3
{
& ed068og1_mode,
4,            /* vscan_holdoff */
10,          /* sdoed_width */
20,          /* sdoed_delay */
10,          /* sdoez_width */
20,          /* sdoez_delay */
831,        /* GDCLK_HP */
285,        /* GDSP_OFF */
0,            /* GDOE_OFF */
271,        /* gdclk_offs */
1,            /* num_ce */
},
#endif//] EPD_TIMING_ED068OG1_NUMCE3

{
&peng060d_mode,
4,            /* vscan_holdoff */
10,          /* sdoed_width */
20,          /* sdoed_delay */
10,          /* sdoez_width */
20,          /* sdoez_delay */
562,        /* GDCLK_HP */
662,        /* GDSP_OFF */
0,            /* GDOE_OFF */
225,        /* gdclk_offs */
3,            /* num_ce */
},
{
&ef133ut1sce_mode,
4,      /* vscan_holdoff */
10,     /* sdoed_width */
20,     /* sdoed_delay */
10,     /* sdoez_width */
20,     /* sdoez_delay */
743,    /* GDCLK_HP */
475,    /* GDSP_OFF */
0,      /* GDOE_OFF */
15,     /* gdclk_offs */
1,      /* num_ce */
},
{
&ed060scq_mode,
4,      /* vscan_holdoff */
10,     /* sdoed_width */
20,     /* sdoed_delay */
10,     /* sdoez_width */
20,     /* sdoez_delay */
438,    /* GDCLK_HP */
263,    /* GDSP_OFF */
0,      /* GDOE_OFF */
23,     /* gdclk_offs */
3,      /* num_ce */
},
{
&ed078kh1_mode,
4,      /* vscan_holdoff */
10,     /* sdoed_width */
20,     /* sdoed_delay */
10,     /* sdoez_width */
20,     /* sdoez_delay */
772,    /* GDCLK_HP */
757,    /* GDSP_OFF */
0,      /* GDOE_OFF */
199,     /* gdclk_offs */
1,      /* num_ce */
},
{
&ed060sct_mode,
4,      /* vscan_holdoff */
10,     /* sdoed_width */
20,     /* sdoed_delay */
10,     /* sdoez_width */
20,     /* sdoez_delay */
372,    /* GDCLK_HP */
367,    /* GDSP_OFF */
0,      /* GDOE_OFF */
111,     /* gdclk_offs */
1,      /* num_ce */
},
};
 

#else //][!
static struct fb_videomode e60_v110_mode = {
	.name = "E60_V110",
	.refresh = 50,
	.xres = 800,
	.yres = 600,
	.pixclock = 18604700,
	.left_margin = 8,
	.right_margin = 178,
	.upper_margin = 4,
	.lower_margin = 10,
	.hsync_len = 20,
	.vsync_len = 4,
	.sync = 0,
	.vmode = FB_VMODE_NONINTERLACED,
	.flag = 0,
};
static struct fb_videomode e60_v220_mode = {
	.name = "E60_V220",
	.refresh = 85,
	.xres = 800,
	.yres = 600,
	.pixclock = 30000000,
	.left_margin = 8,
	.right_margin = 164,
	.upper_margin = 4,
	.lower_margin = 8,
	.hsync_len = 4,
	.vsync_len = 1,
	.sync = 0,
	.vmode = FB_VMODE_NONINTERLACED,
	.flag = 0,
	.refresh = 85,
	.xres = 800,
	.yres = 600,
};
static struct fb_videomode e060scm_mode = {
	.name = "E060SCM",
	.refresh = 85,
	.xres = 800,
	.yres = 600,
	.pixclock = 26666667,
	.left_margin = 8,
	.right_margin = 100,
	.upper_margin = 4,
	.lower_margin = 8,
	.hsync_len = 4,
	.vsync_len = 1,
	.sync = 0,
	.vmode = FB_VMODE_NONINTERLACED,
	.flag = 0,
};
static struct fb_videomode e97_v110_mode = {
	.name = "E97_V110",
	.refresh = 50,
	.xres = 1200,
	.yres = 825,
	.pixclock = 32000000,
	.left_margin = 12,
	.right_margin = 128,
	.upper_margin = 4,
	.lower_margin = 10,
	.hsync_len = 20,
	.vsync_len = 4,
	.sync = 0,
	.vmode = FB_VMODE_NONINTERLACED,
	.flag = 0,
};

static struct imx_epdc_fb_mode panel_modes[] = {
	{
		&e60_v110_mode,
		4,      /* vscan_holdoff */
		10,     /* sdoed_width */
		20,     /* sdoed_delay */
		10,     /* sdoez_width */
		20,     /* sdoez_delay */
		428,    /* gdclk_hp_offs */
		20,     /* gdsp_offs */
		0,      /* gdoe_offs */
		1,      /* gdclk_offs */
		1,      /* num_ce */
	},
	{
		&e60_v220_mode,
		4,      /* vscan_holdoff */
		10,     /* sdoed_width */
		20,     /* sdoed_delay */
		10,     /* sdoez_width */
		20,     /* sdoez_delay */
		465,    /* gdclk_hp_offs */
		20,     /* gdsp_offs */
		0,      /* gdoe_offs */
		9,      /* gdclk_offs */
		1,      /* num_ce */
	},
	{
		&e060scm_mode,
		4,      /* vscan_holdoff */
		10,     /* sdoed_width */
		20,     /* sdoed_delay */
		10,     /* sdoez_width */
		20,     /* sdoez_delay */
		419,    /* gdclk_hp_offs */
		20,     /* gdsp_offs */
		0,      /* gdoe_offs */
		5,      /* gdclk_offs */
		1,      /* num_ce */
	},
	{
		&e97_v110_mode,
		8,      /* vscan_holdoff */
		10,     /* sdoed_width */
		20,     /* sdoed_delay */
		10,     /* sdoez_width */
		20,     /* sdoez_delay */
		632,    /* gdclk_hp_offs */
		20,     /* gdsp_offs */
		0,      /* gdoe_offs */
		1,      /* gdclk_offs */
		3,      /* num_ce */
	}
};
#endif //]

static struct imx_epdc_fb_platform_data epdc_data = {
	.epdc_mode = panel_modes,
	.num_modes = ARRAY_SIZE(panel_modes),
	.get_pins = epdc_get_pins,
	.put_pins = epdc_put_pins,
	.enable_pins = epdc_enable_pins,
	.disable_pins = epdc_disable_pins,
};

typedef void (*usb_insert_handler) (char inserted);
usb_insert_handler mxc_misc_report_usb;

static void mxc_register_usb_plug(void* pk_cb)
{
	pmic_event_callback_t usb_plug_event;

//	usb_plug_event.param = (void *)1;
//	usb_plug_event.func = (void *)pk_cb;
//	pmic_event_subscribe(EVENT_VBUSVI, usb_plug_event);
	if (gIsCustomerUi)
		mxc_misc_report_usb = pk_cb;
	DBG_MSG("%s(),pk_cb=%p\n",__FUNCTION__,pk_cb);

}


extern int mxc_usb_plug_getstatus (void);


#define SW_USBPLUG	0x0C
static struct usbplug_event_platform_data usbplug_data = {
	.usbevent.type = EV_SW,
	.usbevent.code = SW_USBPLUG,
	.register_usbplugevent = mxc_register_usb_plug,
	.get_key_status = mxc_usb_plug_getstatus,
};

struct platform_device mxc_usb_plug_device = {
		.name = "usb_plug",
		.id = 0,
	};



static int spdc_get_pins(void)
{
	int ret = 0;

	/* Claim GPIOs for SPDC pins - used during power up/down */
	ret |= gpio_request(MX6SL_BRD_EPDC_SDDO_0, "SPDC_D0");
	ret |= gpio_request(MX6SL_BRD_EPDC_SDDO_1, "SPDC_D1");
	ret |= gpio_request(MX6SL_BRD_EPDC_SDDO_2, "SPDC_D2");
	ret |= gpio_request(MX6SL_BRD_EPDC_SDDO_3, "SPDC_D3");
	ret |= gpio_request(MX6SL_BRD_EPDC_SDDO_4, "SPDC_D4");
	ret |= gpio_request(MX6SL_BRD_EPDC_SDDO_5, "SPDC_D5");
	ret |= gpio_request(MX6SL_BRD_EPDC_SDDO_6, "SPDC_D6");
	ret |= gpio_request(MX6SL_BRD_EPDC_SDDO_7, "SPDC_D7");

	ret |= gpio_request(MX6SL_BRD_EPDC_GDOE, "SIPIX_YOE");
	ret |= gpio_request(MX6SL_BRD_EPDC_SDDO_9, "SIPIX_PWR_RDY");

	ret |= gpio_request(MX6SL_BRD_EPDC_GDSP, "SIPIX_YDIO");

	ret |= gpio_request(MX6SL_BRD_EPDC_GDCLK, "SIPIX_YCLK");
	ret |= gpio_request(MX6SL_BRD_EPDC_SDSHR, "SIPIX_XDIO");

	ret |= gpio_request(MX6SL_BRD_EPDC_SDLE, "SIPIX_LD");
	ret |= gpio_request(MX6SL_BRD_EPDC_SDCE1, "SIPIX_SOE");

	ret |= gpio_request(MX6SL_BRD_EPDC_SDCLK, "SIPIX_XCLK");
	ret |= gpio_request(MX6SL_BRD_EPDC_SDDO_10, "SIPIX_SHD_N");
	ret |= gpio_request(MX6SL_BRD_EPDC_SDCE0, "SIPIX2_CE");

	return ret;
}

static void spdc_put_pins(void)
{
	gpio_free(MX6SL_BRD_EPDC_SDDO_0);
	gpio_free(MX6SL_BRD_EPDC_SDDO_1);
	gpio_free(MX6SL_BRD_EPDC_SDDO_2);
	gpio_free(MX6SL_BRD_EPDC_SDDO_3);
	gpio_free(MX6SL_BRD_EPDC_SDDO_4);
	gpio_free(MX6SL_BRD_EPDC_SDDO_5);
	gpio_free(MX6SL_BRD_EPDC_SDDO_6);
	gpio_free(MX6SL_BRD_EPDC_SDDO_7);

	gpio_free(MX6SL_BRD_EPDC_GDOE);
	gpio_free(MX6SL_BRD_EPDC_SDDO_9);
	gpio_free(MX6SL_BRD_EPDC_GDSP);
	gpio_free(MX6SL_BRD_EPDC_GDCLK);
	gpio_free(MX6SL_BRD_EPDC_SDSHR);
	gpio_free(MX6SL_BRD_EPDC_SDLE);
	gpio_free(MX6SL_BRD_EPDC_SDCE1);
	gpio_free(MX6SL_BRD_EPDC_SDCLK);
	gpio_free(MX6SL_BRD_EPDC_SDDO_10);
	gpio_free(MX6SL_BRD_EPDC_SDCE0);
}

static void spdc_enable_pins(void)
{
	/* Configure MUX settings to enable SPDC use */
	mxc_iomux_v3_setup_multiple_pads(mx6sl_brd_spdc_enable_pads, \
				ARRAY_SIZE(mx6sl_brd_spdc_enable_pads));

	gpio_direction_input(MX6SL_BRD_EPDC_SDDO_0);
	gpio_direction_input(MX6SL_BRD_EPDC_SDDO_1);
	gpio_direction_input(MX6SL_BRD_EPDC_SDDO_2);
	gpio_direction_input(MX6SL_BRD_EPDC_SDDO_3);
	gpio_direction_input(MX6SL_BRD_EPDC_SDDO_4);
	gpio_direction_input(MX6SL_BRD_EPDC_SDDO_5);
	gpio_direction_input(MX6SL_BRD_EPDC_SDDO_6);
	gpio_direction_input(MX6SL_BRD_EPDC_SDDO_7);
	gpio_direction_input(MX6SL_BRD_EPDC_SDDO_8);
	gpio_direction_input(MX6SL_BRD_EPDC_SDDO_9);
	gpio_direction_input(MX6SL_BRD_EPDC_SDDO_10);
	gpio_direction_input(MX6SL_BRD_EPDC_SDDO_11);
	gpio_direction_input(MX6SL_BRD_EPDC_SDDO_12);
	gpio_direction_input(MX6SL_BRD_EPDC_SDDO_13);
	gpio_direction_input(MX6SL_BRD_EPDC_SDDO_14);
	gpio_direction_input(MX6SL_BRD_EPDC_SDDO_15);
	gpio_direction_input(MX6SL_BRD_EPDC_GDOE);
	gpio_direction_input(MX6SL_BRD_EPDC_SDDO_9);
	gpio_direction_input(MX6SL_BRD_EPDC_GDSP);
	gpio_direction_input(MX6SL_BRD_EPDC_GDCLK);
	gpio_direction_input(MX6SL_BRD_EPDC_SDSHR);
	gpio_direction_input(MX6SL_BRD_EPDC_SDLE);
	gpio_direction_input(MX6SL_BRD_EPDC_SDCE1);
	gpio_direction_input(MX6SL_BRD_EPDC_SDCLK);
	gpio_direction_input(MX6SL_BRD_EPDC_SDDO_10);
	gpio_direction_input(MX6SL_BRD_EPDC_SDCE0);
}

static void spdc_disable_pins(void)
{
	/* Configure MUX settings for SPDC pins to
	 * GPIO and drive to 0. */
	mxc_iomux_v3_setup_multiple_pads(mx6sl_brd_spdc_disable_pads, \
				ARRAY_SIZE(mx6sl_brd_spdc_disable_pads));

	gpio_direction_output(MX6SL_BRD_EPDC_SDDO_0, 0);
	gpio_direction_output(MX6SL_BRD_EPDC_SDDO_1, 0);
	gpio_direction_output(MX6SL_BRD_EPDC_SDDO_2, 0);
	gpio_direction_output(MX6SL_BRD_EPDC_SDDO_3, 0);
	gpio_direction_output(MX6SL_BRD_EPDC_SDDO_4, 0);
	gpio_direction_output(MX6SL_BRD_EPDC_SDDO_5, 0);
	gpio_direction_output(MX6SL_BRD_EPDC_SDDO_6, 0);
	gpio_direction_output(MX6SL_BRD_EPDC_SDDO_7, 0);
	gpio_direction_output(MX6SL_BRD_EPDC_SDDO_8 , 0);
	gpio_direction_output(MX6SL_BRD_EPDC_SDDO_9 , 0);
	gpio_direction_output(MX6SL_BRD_EPDC_SDDO_10, 0);
	gpio_direction_output(MX6SL_BRD_EPDC_SDDO_11, 0);
	gpio_direction_output(MX6SL_BRD_EPDC_SDDO_12, 0);
	gpio_direction_output(MX6SL_BRD_EPDC_SDDO_13, 0);
	gpio_direction_output(MX6SL_BRD_EPDC_SDDO_14, 0);
	gpio_direction_output(MX6SL_BRD_EPDC_SDDO_15, 0);

	gpio_direction_output(MX6SL_BRD_EPDC_GDOE, 0);
	gpio_direction_output(MX6SL_BRD_EPDC_SDDO_9, 0);
	gpio_direction_output(MX6SL_BRD_EPDC_GDSP, 0);
	gpio_direction_output(MX6SL_BRD_EPDC_GDCLK, 0);
	gpio_direction_output(MX6SL_BRD_EPDC_SDSHR, 0);
	gpio_direction_output(MX6SL_BRD_EPDC_SDLE, 0);
	gpio_direction_output(MX6SL_BRD_EPDC_SDCE1, 0);
	gpio_direction_output(MX6SL_BRD_EPDC_SDCLK, 0);
	gpio_direction_output(MX6SL_BRD_EPDC_SDDO_10, 0);
	gpio_direction_output(MX6SL_BRD_EPDC_SDCE0, 0);
}

static struct imx_spdc_panel_init_set spdc_init_set = {
	.yoe_pol = false,
	.dual_gate = false,
	.resolution = 0,
	.ud = false,
	.rl = false,
	.data_filter_n = true,
	.power_ready = true,
	.rgbw_mode_enable = false,
	.hburst_len_en = true,
};

static struct fb_videomode erk_1_4_a01 = {
	.name = "ERK_1_4_A01",
	.refresh = 50,
	.xres = 800,
	.yres = 600,
	.pixclock = 40000000,
	.vmode = FB_VMODE_NONINTERLACED,
};

static struct imx_spdc_fb_mode spdc_panel_modes[] = {
	{
		&erk_1_4_a01,
		&spdc_init_set,
		.wave_timing = "pvi"
	},
};

static struct imx_spdc_fb_platform_data spdc_data = {
	.spdc_mode = spdc_panel_modes,
	.num_modes = ARRAY_SIZE(spdc_panel_modes),
	.get_pins = spdc_get_pins,
	.put_pins = spdc_put_pins,
	.enable_pins = spdc_enable_pins,
	.disable_pins = spdc_disable_pins,
};

static int __init early_use_spdc_sel(char *p)
{
	spdc_sel = 1;
	return 0;
}
early_param("spdc", early_use_spdc_sel);

static void setup_spdc(void)
{
	/* GPR0[8]: 0:EPDC, 1:SPDC */
	if (spdc_sel)
		mxc_iomux_set_gpr_register(0, 8, 1, 1);
}

static void imx6_ntx_usbotg_vbus(bool on)
{
#if 0
	if (on)
		gpio_set_value(MX6_BRD_USBOTG1_PWR, 1);
	else
		gpio_set_value(MX6_BRD_USBOTG1_PWR, 0);
#endif
}

static void __init mx6_ntx_init_usb(void)
{
	int ret = 0;

	imx_otg_base = MX6_IO_ADDRESS(MX6Q_USB_OTG_BASE_ADDR);

#if 0
	/* disable external charger detect,
	 * or it will affect signal quality at dp.
	 */

	ret = gpio_request(MX6_BRD_USBOTG1_PWR, "usbotg-pwr");
	if (ret) {
		pr_err("failed to get GPIO MX6_BRD_USBOTG1_PWR:%d\n", ret);
		return;
	}
	gpio_direction_output(MX6_BRD_USBOTG1_PWR, 0);

	ret = gpio_request(MX6_BRD_USBOTG2_PWR, "usbh1-pwr");
	if (ret) {
		pr_err("failed to get GPIO MX6_BRD_USBOTG2_PWR:%d\n", ret);
		return;
	}
	gpio_direction_output(MX6_BRD_USBOTG2_PWR, 1);
#endif

	mx6_set_otghost_vbus_func(imx6_ntx_usbotg_vbus);
#ifdef CONFIG_USB_EHCI_ARC_HSIC
	mx6_usb_h2_init();
#endif
}

static struct platform_device mxcbl_device = {
	.name = "mxc_msp430_fl",
};

static struct platform_pwm_backlight_data mx6_ntx_pwm_backlight_data = {
	.pwm_id		= 0,
	.max_brightness	= 255,
	.dft_brightness	= 128,
	.pwm_period_ns	= 50000,
};
static struct fb_videomode wvga_video_modes[] = {
	{
	 /* 800x480 @ 57 Hz , pixel clk @ 32MHz */
	 "SEIKO-WVGA", 60, 800, 480, 29850, 99, 164, 33, 10, 10, 10,
	 FB_SYNC_CLK_LAT_FALL,
	 FB_VMODE_NONINTERLACED,
	 0,},
};

static struct mxc_fb_platform_data wvga_fb_data[] = {
	{
	 .interface_pix_fmt = V4L2_PIX_FMT_RGB24,
	 .mode_str = "SEIKO-WVGA",
	 .mode = wvga_video_modes,
	 .num_modes = ARRAY_SIZE(wvga_video_modes),
	 .panel_type = "lcd",
	 },
};

static struct platform_device lcd_wvga_device = {
	.name = "lcd_seiko",
};

#if 0
static struct fb_videomode hdmi_video_modes[] = {
	{
	 /* 1920x1080 @ 60 Hz , pixel clk @ 148MHz */
	 "sii9022x_1080p60", 60, 1920, 1080, 6734, 148, 88, 36, 4, 44, 5,
	 FB_SYNC_CLK_LAT_FALL,
	 FB_VMODE_NONINTERLACED,
	 0,},
};

static struct mxc_fb_platform_data hdmi_fb_data[] = {
	{
	 .interface_pix_fmt = V4L2_PIX_FMT_RGB24,
	 .mode_str = "1920x1080M@60",
	 .mode = hdmi_video_modes,
	 .num_modes = ARRAY_SIZE(hdmi_video_modes),
	 .panel_type = "hdmi",
	 },
};
#endif

static int mx6sl_ntx_keymap[] = {
	KEY(0, 0, 90),
};

static struct platform_device ntx_device_rtc = {
	.name           = "ntx_misc_rtc",
	.id				= 0,
	.dev            = {
		.platform_data = -1,
	}
};

static const struct matrix_keymap_data mx6sl_ntx_map_data __initconst = {
	.keymap		= mx6sl_ntx_keymap,
	.keymap_size	= ARRAY_SIZE(mx6sl_ntx_keymap),
};
#if 0
static void __init elan_ts_init(void)
{
	mxc_iomux_v3_setup_multiple_pads(mx6sl_brd_elan_pads,
					ARRAY_SIZE(mx6sl_brd_elan_pads));

	/* ELAN Touchscreen */
	gpio_request(MX6SL_BRD_ELAN_INT, "elan-interrupt");
	gpio_direction_input(MX6SL_BRD_ELAN_INT);

	gpio_request(MX6SL_BRD_ELAN_CE, "elan-cs");
	gpio_direction_output(MX6SL_BRD_ELAN_CE, 1);
	gpio_direction_output(MX6SL_BRD_ELAN_CE, 0);

	gpio_request(MX6SL_BRD_ELAN_RST, "elan-rst");
	gpio_direction_output(MX6SL_BRD_ELAN_RST, 1);
	gpio_direction_output(MX6SL_BRD_ELAN_RST, 0);
	mdelay(1);
	gpio_direction_output(MX6SL_BRD_ELAN_RST, 1);
	gpio_direction_output(MX6SL_BRD_ELAN_CE, 1);
}
#endif

/*
 *Usually UOK and DOK should have separate
 *line to differentiate its behaviour (with different
 * GPIO irq),because connect max8903 pin UOK to
 *pin DOK from hardware design,cause software cannot
 *process and distinguish two interrupt, so default
 *enable dc_valid for ac charger
 */
static struct max8903_pdata charger1_data = {
	.dok = MX6_BRD_CHG_DOK,
	.uok = MX6_BRD_CHG_UOK,
	.chg = MX6_BRD_CHG_STATUS,
	.flt = MX6_BRD_CHG_FLT,
	.dcm_always_high = true,
	.dc_valid = true,
	.usb_valid = false,
	.feature_flag = 1,
};

static struct platform_device ntx_max8903_charger_1 = {
	.name	= "max8903-charger",
	.dev	= {
		.platform_data = &charger1_data,
	},
};

/*! Device Definition for csi v4l2 device */
static struct platform_device csi_v4l2_devices = {
	.name = "csi_v4l2",
	.id = 0,
};

#define SNVS_LPCR 0x38
static void mx6_snvs_poweroff(void)
{
	u32 value;
	void __iomem *mx6_snvs_base = MX6_IO_ADDRESS(MX6Q_SNVS_BASE_ADDR);

	value = readl(mx6_snvs_base + SNVS_LPCR);
	/* set TOP and DP_EN bit */
	writel(value | 0x60, mx6_snvs_base + SNVS_LPCR);
}

#if 0
static int uart2_enabled;
static int __init uart2_setup(char * __unused)
{
	uart2_enabled = 1;
	return 1;
}
__setup("bluetooth", uart2_setup);

static void __init uart2_init(void)
{
	mxc_iomux_v3_setup_multiple_pads(mx6sl_uart2_pads,
					ARRAY_SIZE(mx6sl_uart2_pads));
	imx6sl_add_imx_uart(1, &mx6sl_ntx_uart1_data);
}
#endif

#if defined(CONFIG_KEYBOARD_GPIO) || defined(CONFIG_KEYBOARD_GPIO_MODULE)

#define GPIO_BUTTON(gpio_num, ev_code, act_low, descr, wake, debounce)	\
{								\
	.gpio		= gpio_num,				\
	.type		= EV_KEY,				\
	.code		= ev_code,				\
	.active_low	= act_low,				\
	.desc		= "btn " descr,				\
	.wakeup		= wake,					\
	.debounce_interval = debounce,				\
}

static struct gpio_keys_button gpio_key_matrix_FL[] = {
	GPIO_BUTTON(GPIO_KB_ROW0, 90, 1, "front_light", 1,1),			// Front light
//#ifdef CONFIG_ANDROID //[
	GPIO_BUTTON(IMX_GPIO_NR(4, 25), KEY_POWER, 1, "power", 1, 1),
//#endif //]CONFIG_ANDROID
};

static struct gpio_keys_button gpio_key_HOME_FL[] = {
	GPIO_BUTTON(GPIO_KB_COL1, 90, 1, "front_light", 1,1),			// Front light
#ifdef CONFIG_ANDROID//[
	GPIO_BUTTON(GPIO_KB_COL0, KEY_HOME, 1, "home", 1,1),			// home
#else //][!CONFIG_ANDROID
	GPIO_BUTTON(GPIO_KB_COL0, 61, 1, "home", 1,1),			// home
#endif //] CONFIG_ANDROID
//#ifdef CONFIG_ANDROID //[
	GPIO_BUTTON(IMX_GPIO_NR(5, 8), KEY_POWER, 1, "power", 1, 1),
//#endif //]CONFIG_ANDROID
};

static struct gpio_keys_button gpio_key_HOME_FL2[] = {
	GPIO_BUTTON(GPIO_KB_COL1, 90, 1, "front_light", 1,1),			// Front light
#ifdef CONFIG_ANDROID//[
	GPIO_BUTTON(GPIO_KB_ROW0, KEY_HOME, 1, "home", 1,1),			// home
#else //][!CONFIG_ANDROID
	GPIO_BUTTON(GPIO_KB_ROW0, 61, 1, "home", 1,1),			// home
#endif //] CONFIG_ANDROID
//#ifdef CONFIG_ANDROID //[
	GPIO_BUTTON(IMX_GPIO_NR(5, 8), KEY_POWER, 1, "power", 1, 1),
//#endif //]CONFIG_ANDROID
};

static struct gpio_keys_button gpio_key_RETURN_HOME_MENU[] = {
#ifdef CONFIG_ANDROID//[
	GPIO_BUTTON(GPIO_KB_ROW0, KEY_HOME, 1, "home", 1,1),			// home
#else //][!CONFIG_ANDROID
	GPIO_BUTTON(GPIO_KB_ROW0, 61, 1, "home", 1,1),			// home
#endif //] CONFIG_ANDROID
	GPIO_BUTTON(GPIO_KB_ROW1, KEY_MENU, 1, "menu", 1,1),			// menu
	GPIO_BUTTON(GPIO_KB_ROW2, KEY_ESC, 1, "return", 1,1),			// return
//#ifdef CONFIG_ANDROID //[
	GPIO_BUTTON(IMX_GPIO_NR(5, 8), KEY_POWER, 1, "power", 1, 1),
//#endif //]CONFIG_ANDROID
};
static struct gpio_keys_button gpio_key_LEFT_RIGHT_HOME_MENU[] = {
#ifdef CONFIG_ANDROID//[
	GPIO_BUTTON(GPIO_KB_ROW1, KEY_HOME, 1, "home", 1,1),			// home
#else //][!CONFIG_ANDROID
	GPIO_BUTTON(GPIO_KB_ROW1, 61, 1, "home", 1,1),			// home
#endif //] CONFIG_ANDROID
	GPIO_BUTTON(GPIO_KB_ROW2, KEY_LEFT, 1, "left", 1,1),			// left
	GPIO_BUTTON(GPIO_KB_ROW3, KEY_RIGHT, 1, "right", 1,1),			// right
	GPIO_BUTTON(GPIO_KB_ROW0, KEY_MENU, 1, "menu", 1,1),			// menu
//#ifdef CONFIG_ANDROID //[
	GPIO_BUTTON(IMX_GPIO_NR(5, 8), KEY_POWER, 1, "power", 1, 1),
//#endif //]CONFIG_ANDROID
};

static struct gpio_keys_button gpio_key_HOME[] = {
#ifdef CONFIG_ANDROID//[
	GPIO_BUTTON(GPIO_KB_COL0, KEY_HOME, 1, "home", 1,1),			// home
#else //][!CONFIG_ANDROID
	GPIO_BUTTON(GPIO_KB_COL0, 61, 1, "home", 1, 50),			// home
#endif //] CONFIG_ANDROID
//#ifdef CONFIG_ANDROID //[
	GPIO_BUTTON(IMX_GPIO_NR(5, 8), KEY_POWER, 1, "power", 1, 1),
//#endif //]CONFIG_ANDROID
};
static struct gpio_keys_button gpio_key_HOME_ROW0[] = {
#ifdef CONFIG_ANDROID//[
	GPIO_BUTTON(GPIO_KB_ROW0, KEY_HOME, 1, "home", 1,1),			// home
#else //][!CONFIG_ANDROID
	GPIO_BUTTON(GPIO_KB_ROW0, 61, 1, "home", 1, 50),			// home
#endif //] CONFIG_ANDROID
//#ifdef CONFIG_ANDROID //[
	GPIO_BUTTON(IMX_GPIO_NR(5, 8), KEY_POWER, 1, "power", 1, 1),
//#endif //]CONFIG_ANDROID
};

static struct gpio_keys_button gpio_key_FL[] = {
	GPIO_BUTTON(GPIO_KB_COL1, 90, 1, "front_light", 1, 10),			// Front light
//#ifdef CONFIG_ANDROID //[
	GPIO_BUTTON(IMX_GPIO_NR(5, 8), KEY_POWER, 1, "power", 1, 1),
//#endif //]CONFIG_ANDROID
};

static struct gpio_keys_button gpio_key_None[] = {
//#ifdef CONFIG_ANDROID //[
	GPIO_BUTTON(IMX_GPIO_NR(5, 8), KEY_POWER, 1, "power", 1, 1),
//#endif //]CONFIG_ANDROID
};

static struct gpio_keys_button *gptGPIO_HOME_KEY;

static struct gpio_keys_platform_data ntx_gpio_key_data = {
	.buttons	= gpio_key_matrix_FL,
	.nbuttons	= ARRAY_SIZE(gpio_key_matrix_FL),
};

static struct platform_device ntx_gpio_key_device = {
	.name		= "gpio-keys",
	.id		= -1,
	.num_resources  = 0,
	.dev		= {
		.platform_data = &ntx_gpio_key_data,
	}
};

#ifdef TOUCH_HOME_LED//[

extern int ntx_get_homepad_enabled_status(void);

static int giHomeLED_Delay_Ticks=100;
static volatile int giCurHomeLED_state;
static struct delayed_work homeled_pwrdwn_work,homepad_check_work;
/*
 * giHomePad_enable :
 * 0 = disabled .
 * 1 = enabled home pad/led action .
 * 2 = enabled home pad/led action and send event to application layer .
 */
static int giHomePad_enable=2;


static void _homeled_onoff_force(int iIsON) 
{
	//int iOldHomeLED_state=giCurHomeLED_state;
	int iIsHOMELED_gpio=1;

	if( 0==gptHWCFG->m_val.bHOME_LED_PWM && \
			( 36!=gptHWCFG->m_val.bPCB && 40!=gptHWCFG->m_val.bPCB) ) 
	{
		// HOME_LED_PWM==NO && !=E60Q3X&&E60Q5X
		return ;
	}

	if(1==gptHWCFG->m_val.bHOME_LED_PWM) {
		printk("MSP430 ");
		iIsHOMELED_gpio=0;
	}
	else {
		printk("GPIO ");
	}

	if(iIsON) {
		giCurHomeLED_state=1;
		printk("HOME LED [ON]\n");
		if(iIsHOMELED_gpio) {
			gpio_direction_output (gMX6SL_HOME_LED, 0);
		}
		else {
		}
	}
	else {
		giCurHomeLED_state=0;
		printk("HOME LED [OFF]\n");
		if(iIsHOMELED_gpio) {
			gpio_direction_output (gMX6SL_HOME_LED, 1);
		}
		else {
		}
	}
}


static ssize_t homepad_enable_write(struct device *dev,
		struct device_attribute *attr,const char *buf,size_t count)
{
	int iParam;
	extern void gpiokeys_enable_button(struct gpio_keys_button *button,int iIsEnable);

	iParam = simple_strtol(buf,NULL,0);

	if(iParam!=giHomePad_enable) {
		switch (iParam) {
		case 0:
			if(1==gptHWCFG->m_val.bHOME_LED_PWM) {
				// HOME LED is controlled by MSP430 .
				//msp430_homeled_enable(0);
			}
			//gpiokeys_enable_button(gptGPIO_HOME_KEY,0);
			_homeled_onoff_force(0);
			msp430_homepad_enable(0);
			giHomePad_enable = iParam;
			break;
		case 1:
		case 2:
			//gpiokeys_enable_button(gptGPIO_HOME_KEY,1);
			if(1==gptHWCFG->m_val.bHOME_LED_PWM) {
				// HOME LED is controlled by MSP430 .
				//msp430_homeled_enable(1);
			}
			msp430_homepad_enable(1);
			giHomePad_enable = iParam;
			break;
		default :
			printk(KERN_ERR"invalid parameter !\n");
			return -1;
		}
	}
	else {
		
	}

	return strlen(buf);
}
static ssize_t homepad_enable_read(struct device *dev,
		struct device_attribute *attr,const char *buf,size_t count)
{
	ssize_t szChk=0;
	sprintf(buf,"%d\n",giHomePad_enable);
	szChk = strlen(buf);
	return szChk;
}
static DEVICE_ATTR(homepad_enable,0666,
		homepad_enable_read,
		homepad_enable_write);

static ssize_t homeled_delayms_show(struct device *dev,
		struct device_attribute *attr,const char *buf,size_t count)
{
	ssize_t szChk=0;
	sprintf(buf,"%d\n",giHomeLED_Delay_Ticks*(1000/HZ));
	szChk = strlen(buf);
	return szChk;
}
static ssize_t homeled_delayms_store(struct device *dev,
		struct device_attribute *attr,const char *buf,size_t count)
{
	int iDelayms;
	ssize_t szChk;
	iDelayms = simple_strtol(buf,NULL,0);
	if(1==gptHWCFG->m_val.bHOME_LED_PWM) {
		// HOME LED is controlled by MSP430 .
		int iChk = msp430_set_homeled_delayms(iDelayms);
		if(iChk>=0) {
			giHomeLED_Delay_Ticks = msecs_to_jiffies(iChk);
			szChk = strlen(buf);
		}
		else {
			szChk = -1;
		}
	}
	else if (36==gptHWCFG->m_val.bPCB || 2==gptHWCFG->m_val.bHOME_LED_PWM) 
	{
		// HOME LED is controlled by SOC .
		giHomeLED_Delay_Ticks = msecs_to_jiffies(iDelayms);
		szChk = strlen(buf);
	}
	return szChk;
}
static DEVICE_ATTR(homeled_delayms,0666,
		homeled_delayms_show,
		homeled_delayms_store);

static ssize_t homeled_delaylevel_show(struct device *dev,
		struct device_attribute *attr,const char *buf,size_t count)
{
	ssize_t szChk=0;
	if(1==gptHWCFG->m_val.bHOME_LED_PWM) {
		// HOME LED is controlled by MSP430 .
		if(MSP430_HOMELED_TYPE_PWM==msp430_homeled_type_get(0)) {
			sprintf(buf,"%d\n",msp430_get_homeled_pwm_delaylevel());
		}
		else {
			sprintf(buf,"%d\n",msp430_get_homeled_gpio_delaylevel());
		}
		szChk = strlen(buf);
	}
	else {
	}
	return szChk;
}
static ssize_t homeled_delaylevel_store(struct device *dev,
		struct device_attribute *attr,const char *buf,size_t count)
{
	int iDelayLevel;

	iDelayLevel = simple_strtol(buf,NULL,0);
	if(1==gptHWCFG->m_val.bHOME_LED_PWM) {
		// HOME LED is controlled by MSP430 .
		if(MSP430_HOMELED_TYPE_PWM==msp430_homeled_type_get(0)) {
			msp430_set_homeled_pwm_delaylevel(iDelayLevel);
		}
		else {
			msp430_set_homeled_gpio_delaylevel(iDelayLevel);
		}
		return strlen(buf);
	}
	else {
		return (ssize_t)(-1);
	}
}
static DEVICE_ATTR(homeled_delaylevel,0666,
		homeled_delaylevel_show,
		homeled_delaylevel_store);


static ssize_t homeled_type_show(struct device *dev,
		struct device_attribute *attr,const char *buf,size_t count)
{
	if(1==gptHWCFG->m_val.bHOME_LED_PWM) {
		// HOME LED is controlled by MSP430 .
		char *pszHomeLedType;

		msp430_homeled_type_get(&pszHomeLedType);
		sprintf(buf,"%s\n",pszHomeLedType);

		return strlen(buf);
	}
	else {
		return 0;
	}
}

static ssize_t homeled_type_store(struct device *dev,
		struct device_attribute *attr,const char *buf,size_t count)
{
	ssize_t szChk = -1;

	if(1==gptHWCFG->m_val.bHOME_LED_PWM) {
		// HOME LED is controlled by MSP430 .
		if( msp430_homeled_type_set_by_name(buf) >=0 ) {
			szChk = strlen(buf);
		}
	}
	else {
		printk(KERN_WARNING"Home led control type cannot changed on this hardware \n");
	}

	return szChk;
}

static DEVICE_ATTR(homeled_type,0666,
		homeled_type_show,
		homeled_type_store);


static ssize_t homepad_sensitivity_show(struct device *dev,
		struct device_attribute *attr,const char *buf,size_t count)
{
	ssize_t szChk=0;
	sprintf(buf,"%d\n",msp430_homepad_sensitivity_get());
	szChk = strlen(buf);
	return szChk;
}
static ssize_t homepad_sensitivity_store(struct device *dev,
		struct device_attribute *attr,const char *buf,size_t count)
{
	int iSensitivityLvl;

	iSensitivityLvl = simple_strtol(buf,NULL,0);
	if(msp430_homepad_sensitivity_set(iSensitivityLvl)>=0) {
		return strlen(buf);
	}
	else {
		return (ssize_t)(-1);
	}
}
static DEVICE_ATTR(homepad_sensitivity,0666,
		homepad_sensitivity_show,
		homepad_sensitivity_store);



static const struct attribute *sysfs_homepad_attrs[] = {
	&dev_attr_homepad_enable.attr,
	&dev_attr_homepad_sensitivity.attr,
	NULL
};
static const struct attribute *sysfs_homeled_msp430_attrs[] = {
	&dev_attr_homeled_type.attr,
	&dev_attr_homeled_delayms.attr,
	&dev_attr_homeled_delaylevel.attr,
	NULL
};
static const struct attribute *sysfs_homeled_socgpio_attrs[] = {
	&dev_attr_homeled_delayms.attr,
	NULL
};



static void _homeled_onoff(int iIsON) 
{
	if(2==gptHWCFG->m_val.bUIStyle) {
		if(gSleep_Mode_Suspend) {
			// skip home led control when system want to enter fake hibernation .
			_homeled_onoff_force(0);
			return ;
		}
	}
	_homeled_onoff_force(iIsON);
}

static void _homepad_work_func(struct work_struct *work)
{
	int iIsHomePressing,iHomeGPIOVal;

	iHomeGPIOVal=gpio_get_value(gptGPIO_HOME_KEY->gpio);
	iIsHomePressing = (iHomeGPIOVal?1:0)^gptGPIO_HOME_KEY->active_low;
	//printk("%s(),\"%s\" gpio=%d,down=%d\n",__FUNCTION__,
	//		gptGPIO_HOME_KEY->desc,iHomeGPIOVal,iIsHomePressing);
	if(iIsHomePressing) {
		msp430_homepad_enable(2);
	}
}
static void _homeled_work_func(struct work_struct *work)
{
	
	_homeled_onoff(0);
	
}

static int ntx_touch_home_key_hook(struct gpio_keys_button *I_gpio_btn_data,int state)
{
	int iRet = 0;
	//printk("%s(%p,%d)\n",__FUNCTION__,I_gpio_btn_data,state);
	/*if(0x03==gptHWCFG->m_val.bUIConfig) {
		// RD/MP mode .
		if(!state) {
			printk("%s():current home led=%d\n",__FUNCTION__,giCurHomeLED_state);
			_homeled_onoff(!giCurHomeLED_state);
		}
	}
	else*/ 
	{
		if(state) {

			if( 36==gptHWCFG->m_val.bPCB || 2==gptHWCFG->m_val.bHOME_LED_PWM ) {
				// E60Q3X or HOME LED is GPIO controlled by SOC .
				cancel_delayed_work_sync(&homeled_pwrdwn_work);
				cancel_delayed_work_sync(&homepad_check_work);
				schedule_delayed_work(&homepad_check_work, giHomeLED_Delay_Ticks+400);
				_homeled_onoff(1);
			}
		}
		else {

			if( 36==gptHWCFG->m_val.bPCB || 2==gptHWCFG->m_val.bHOME_LED_PWM ) {
				// E60Q3X or HOME LED is GPIO controlled by SOC .
				cancel_delayed_work_sync(&homeled_pwrdwn_work);
				schedule_delayed_work(&homeled_pwrdwn_work, giHomeLED_Delay_Ticks);
			}
		}

		if(giHomePad_enable<=1) {
			iRet = -1;
		}
	}
	return iRet;
}

void homeled_onoff(int iIsON) {
	_homeled_onoff_force(iIsON);
}	

int ntx_get_homepad_enabled_status(void)
{
	return giHomePad_enable;
}

int ntx_get_homeled_delay_ms(void) 
{
	return jiffies_to_msecs(giHomeLED_Delay_Ticks);
}

void ntx_create_homepad_sys_attrs(struct kobject *kobj)
{
	int iChk;

	if(36==gptHWCFG->m_val.bPCB || 40==gptHWCFG->m_val.bPCB ||
		16==gptHWCFG->m_val.bKeyPad || 18==gptHWCFG->m_val.bKeyPad )
	{
		// Q32/Q52/keypad with HOMEPAD .
		iChk = sysfs_create_files(kobj,sysfs_homepad_attrs);
		if(iChk) {
			pr_err("Can't create homepad attr sysfs !\n");
		}
	}



	if(36==gptHWCFG->m_val.bPCB || 40==gptHWCFG->m_val.bPCB || 0!=gptHWCFG->m_val.bHOME_LED_PWM) {
		if(36==gptHWCFG->m_val.bPCB||2==gptHWCFG->m_val.bHOME_LED_PWM) {
			// E60Q3X or HOME LED PWM controlled by SOC .
			iChk = sysfs_create_files(kobj,sysfs_homeled_socgpio_attrs);
			if(iChk) {
				pr_err("Can't create homepad soc gpio attr sysfs !\n");
			}
		}
		else if(1==gptHWCFG->m_val.bHOME_LED_PWM) {
			// HOME LED PWM is controlled by MSP430 .
			iChk = sysfs_create_files(kobj,sysfs_homeled_msp430_attrs);
			if(iChk) {
				pr_err("Can't create homepad msp430 pwm attr sysfs !\n");
			}
		}
	}
}
#else //][!TOUCH_HOME_LED
static void _homeled_onoff(int iIsON) {}
void homeled_onoff(int iIsON) {}
#endif //]TOUCH_HOME_LED

#endif



void *g_wifi_sd_host;
irq_handler_t g_cd_irq;

void ntx_register_wifi_cd (irq_handler_t handler, void *data)
{
	printk ("[%s-%d] register g_cd_irq \n",__func__,__LINE__);
	g_wifi_sd_host = data;
	g_cd_irq = handler;
}


static DEFINE_MUTEX(ntx_wifi_power_mutex);
static int gi_wifi_power_status = -1;

int _ntx_get_wifi_power_status(void)
{
	int iWifiPowerStatus;

	mutex_lock(&ntx_wifi_power_mutex);
	iWifiPowerStatus = gi_wifi_power_status;
	mutex_unlock(&ntx_wifi_power_mutex);

	return iWifiPowerStatus;
}

int _ntx_wifi_power_ctrl (int isWifiEnable)
{
	int iHWID;
	int iOldStatus;

	mutex_lock(&ntx_wifi_power_mutex);
	iOldStatus = gi_wifi_power_status;
	printk("Wifi / BT power control %d\n", isWifiEnable);
	if(isWifiEnable == 0){
		gpio_direction_output (gMX6SL_WIFI_RST, 0);
		gpio_direction_input (gMX6SL_WIFI_3V3);	// turn off Wifi_3V3_on

		msleep(10);
// DO NOT switch pin functions to GPIO
/*
		// sdio port disable ...
		if(33==gptHWCFG->m_val.bPCB) {
			//E60Q2X .
			mxc_iomux_v3_setup_multiple_pads(mx6sl_ntx_sd3_gpio_pads, ARRAY_SIZE(mx6sl_ntx_sd3_gpio_pads));
				gpio_request (MX6SL_SD3_CLK	, "MX6SL_SD3_CLK" );
			gpio_request (MX6SL_SD3_CMD	, "MX6SL_SD3_CMD" );
			gpio_request (MX6SL_SD3_DAT0, "MX6SL_SD3_DAT0");
			gpio_request (MX6SL_SD3_DAT1, "MX6SL_SD3_DAT1");
			gpio_request (MX6SL_SD3_DAT2, "MX6SL_SD3_DAT2");
			gpio_request (MX6SL_SD3_DAT3, "MX6SL_SD3_DAT3");
			gpio_direction_output (MX6SL_SD3_CLK , 0);
			gpio_direction_output (MX6SL_SD3_CMD , 0);
			gpio_direction_output (MX6SL_SD3_DAT0, 0);
			gpio_direction_output (MX6SL_SD3_DAT1, 0);
			gpio_direction_output (MX6SL_SD3_DAT2, 0);
			gpio_direction_output (MX6SL_SD3_DAT3, 0);
		}
		else {
			mxc_iomux_v3_setup_multiple_pads(mx6sl_ntx_sd2_gpio_pads, ARRAY_SIZE(mx6sl_ntx_sd2_gpio_pads));
			gpio_request (MX6SL_SD2_CLK	, "MX6SL_SD2_CLK" );
			gpio_request (MX6SL_SD2_CMD	, "MX6SL_SD2_CMD" );
			gpio_request (MX6SL_SD2_DAT0, "MX6SL_SD2_DAT0");
			gpio_request (MX6SL_SD2_DAT1, "MX6SL_SD2_DAT1");
			gpio_request (MX6SL_SD2_DAT2, "MX6SL_SD2_DAT2");
			gpio_request (MX6SL_SD2_DAT3, "MX6SL_SD2_DAT3");
			gpio_direction_input (MX6SL_SD2_CLK );
			gpio_direction_input (MX6SL_SD2_CMD );
			gpio_direction_input (MX6SL_SD2_DAT0);
			gpio_direction_input (MX6SL_SD2_DAT1);
			gpio_direction_input (MX6SL_SD2_DAT2);
			gpio_direction_input (MX6SL_SD2_DAT3);
		}
*/

#ifdef _WIFI_ALWAYS_ON_
		disable_irq_wake(gpio_to_irq(gMX6SL_WIFI_INT));
#endif
		gi_wifi_power_status=0;
	}
	else {
		if (iOldStatus) {
			printk ("Wifi already on.\n");
			msleep(600);
			mutex_unlock(&ntx_wifi_power_mutex);
			return iOldStatus;
		}

		// sdio port process ...
		if(31==gptHWCFG->m_val.bPCB||32==gptHWCFG->m_val.bPCB) {
			// E60Q0X/E60Q1X
			gpio_free (MX6SL_SD2_CLK );
			gpio_free (MX6SL_SD2_CMD );
			gpio_free (MX6SL_SD2_DAT0);
			gpio_free (MX6SL_SD2_DAT1);
			gpio_free (MX6SL_SD2_DAT2);
			gpio_free (MX6SL_SD2_DAT3);
			mxc_iomux_v3_setup_multiple_pads(mx6sl_ntx_sd2_wifi_pads, ARRAY_SIZE(mx6sl_ntx_sd2_wifi_pads));
		}
		else {
			gpio_free (MX6SL_SD3_CLK );
			gpio_free (MX6SL_SD3_CMD );
			gpio_free (MX6SL_SD3_DAT0);
			gpio_free (MX6SL_SD3_DAT1);
			gpio_free (MX6SL_SD3_DAT2);
			gpio_free (MX6SL_SD3_DAT3);
			mxc_iomux_v3_setup_multiple_pads(mx6sl_ntx_sd3_wifi_pads, ARRAY_SIZE(mx6sl_ntx_sd3_wifi_pads));			
		}
		//msleep(10);

		gpio_direction_output (gMX6SL_WIFI_3V3, 0);	// turn on Wifi_3V3_on
		//schedule_timeout(HZ/50);
		msleep(10);

		gpio_direction_input (gMX6SL_WIFI_INT);
		//msleep(10);
		gpio_direction_output (gMX6SL_WIFI_RST, 1);	// turn on wifi_RST
		//schedule_timeout(HZ/10);
		msleep(30);
#ifdef _WIFI_ALWAYS_ON_
		enable_irq_wake(gpio_to_irq(gMX6SL_WIFI_INT));
#endif
		gi_wifi_power_status=1;
	}

	if (g_cd_irq) {
		struct sdhci_host *host;

		host = (struct sdhci_host *) g_wifi_sd_host;
		//g_cd_irq (0, g_wifi_sd_host);
		//schedule_timeout (100);
		//msleep(1000);
		mmc_detect_change(host->mmc, msecs_to_jiffies(500));
		msleep(600);
		//schedule_timeout(500);
	}
	else {
		printk ("[%s-%d] not registered.\n",__func__,__LINE__);
	}

	if(isWifiEnable == 0){ // switch PIN function to GPIO
		// sdio port disable ...
		if(31==gptHWCFG->m_val.bPCB||32==gptHWCFG->m_val.bPCB) {
			// E60Q0X/E60Q1X.
			mxc_iomux_v3_setup_multiple_pads(mx6sl_ntx_sd2_gpio_pads, ARRAY_SIZE(mx6sl_ntx_sd2_gpio_pads));
			gpio_request (MX6SL_SD2_CLK	, "MX6SL_SD2_CLK" );
			gpio_request (MX6SL_SD2_CMD	, "MX6SL_SD2_CMD" );
			gpio_request (MX6SL_SD2_DAT0, "MX6SL_SD2_DAT0");
			gpio_request (MX6SL_SD2_DAT1, "MX6SL_SD2_DAT1");
			gpio_request (MX6SL_SD2_DAT2, "MX6SL_SD2_DAT2");
			gpio_request (MX6SL_SD2_DAT3, "MX6SL_SD2_DAT3");
			gpio_direction_input (MX6SL_SD2_CLK );
			gpio_direction_input (MX6SL_SD2_CMD );
			gpio_direction_input (MX6SL_SD2_DAT0);
			gpio_direction_input (MX6SL_SD2_DAT1);
			gpio_direction_input (MX6SL_SD2_DAT2);
			gpio_direction_input (MX6SL_SD2_DAT3);
		}
		else {
			mxc_iomux_v3_setup_multiple_pads(mx6sl_ntx_sd3_gpio_pads, ARRAY_SIZE(mx6sl_ntx_sd3_gpio_pads));
			gpio_request (MX6SL_SD3_CLK	, "MX6SL_SD3_CLK" );
			gpio_request (MX6SL_SD3_CMD	, "MX6SL_SD3_CMD" );
			gpio_request (MX6SL_SD3_DAT0, "MX6SL_SD3_DAT0");
			gpio_request (MX6SL_SD3_DAT1, "MX6SL_SD3_DAT1");
			gpio_request (MX6SL_SD3_DAT2, "MX6SL_SD3_DAT2");
			gpio_request (MX6SL_SD3_DAT3, "MX6SL_SD3_DAT3");
			gpio_direction_output (MX6SL_SD3_CLK , 0);
			gpio_direction_output (MX6SL_SD3_CMD , 0);
			gpio_direction_output (MX6SL_SD3_DAT0, 0);
			gpio_direction_output (MX6SL_SD3_DAT1, 0);
			gpio_direction_output (MX6SL_SD3_DAT2, 0);
			gpio_direction_output (MX6SL_SD3_DAT3, 0);
		}
	}
	printk("%s() end.\n",__FUNCTION__);
	mutex_unlock(&ntx_wifi_power_mutex);
	return iOldStatus;
}

void ntx_wifi_power_ctrl(int iIsWifiEnable)
{
	_ntx_wifi_power_ctrl(iIsWifiEnable);
}

EXPORT_SYMBOL(ntx_wifi_power_ctrl);

static iomux_v3_cfg_t mx6sl_ntx_suspend_pads[] = {
	MX6SL_PAD_I2C2_SCL__GPIO_3_14,
	MX6SL_PAD_I2C2_SDA__GPIO_3_15,
};

static iomux_v3_cfg_t mx6sl_ntx_resume_pads[] = {
	MX6SL_PAD_I2C2_SCL__I2C2_SCL,
	MX6SL_PAD_I2C2_SDA__I2C2_SDA,
};

extern void ntx_gpio_suspend (void);
extern void ntx_gpio_resume (void);

static void ntx_suspend_enter(void)
{
//	mxc_iomux_v3_setup_multiple_pads(mx6sl_ntx_suspend_pads, ARRAY_SIZE(mx6sl_ntx_suspend_pads));
//	gpio_request(MX6SL_I2C2_SCL, "MX6SL_I2C2_SCL");
//	gpio_request(MX6SL_I2C2_SDA, "MX6SL_I2C2_SDA");
//	gpio_direction_output (MX6SL_I2C2_SCL, 0);
//	gpio_direction_output (MX6SL_I2C2_SDA, 0);

#if 0
	gpio_direction_output (MX6SL_EP_PWRALL, 1);
	gpio_direction_output (MX6SL_EP_WAKEUP, 0);
	gpio_direction_output (MX6SL_EP_PWRUP, 0);
	gpio_direction_output (MX6SL_EP_VCOM, 0);
	gpio_direction_input (MX6SL_EP_INT);
	gpio_direction_input (MX6SL_EP_PWRSTAT);
#endif
	
	_homeled_onoff_force(0);

	ntx_gpio_suspend ();

#if 0
	{
		unsigned int *pIomux = IO_ADDRESS(MX6Q_IOMUXC_BASE_ADDR)+0x4C;
		unsigned int value;
		
		printk ("Addr , value\n");
		while (IO_ADDRESS(MX6Q_IOMUXC_BASE_ADDR)+0x5A8 >= pIomux) {
			value = *pIomux;
			printk ("0x%08X, 0x%08X, %s%s%s\n",pIomux, value,((value&0x2000)?"PUE - ":""),\
			((value&0x2000)?((value&0xC000)?"Pull Up - ":"Pull Down - "):""),((value&0x1000)?"PKE":""));
			++pIomux;
		}
	}
#endif
#if 0
{
       void __iomem *base = IO_ADDRESS(MX6Q_IOMUXC_BASE_ADDR);
       unsigned int offset = 0x4C;
       unsigned int addr = 0x20E0000;
       unsigned int value;

       for(offset=0x4C; offset <=0x884; offset+=4) {
               value = __raw_readl( base + offset);
               printk(KERN_DEBUG "addr %08x = %08x\n", addr+offset, value);
       }

       base = IO_ADDRESS(GPIO1_BASE_ADDR);
       addr = 0x209C000;
       for(offset=0; offset<=0x1C; offset+=4) {
               value = __raw_readl( base + offset);
               printk(KERN_DEBUG "addr %08x = %08x\n", addr+offset, value);
       }

    base = IO_ADDRESS(GPIO2_BASE_ADDR);
    addr = 0x20A0000;
    for(offset=0; offset<=0x1C; offset+=4) {
        value = __raw_readl( base + offset);
        printk(KERN_DEBUG "addr %08x = %08x\n", addr+offset, value);
    }

    base = IO_ADDRESS(GPIO3_BASE_ADDR);
    addr = 0x20A4000;
    for(offset=0; offset<=0x1C; offset+=4) {
        value = __raw_readl( base + offset);
        printk(KERN_DEBUG "addr %08x = %08x\n", addr+offset, value);
    }

    base = IO_ADDRESS(GPIO4_BASE_ADDR);
    addr = 0x20A8000;
    for(offset=0; offset<=0x1C; offset+=4) {
        value = __raw_readl( base + offset);
        printk(KERN_DEBUG "addr %08x = %08x\n", addr+offset, value);
    }

    base = IO_ADDRESS(GPIO5_BASE_ADDR);
    addr = 0x20AC000;
    for(offset=0; offset<=0x1C; offset+=4) {
        value = __raw_readl( base + offset);
        printk(KERN_DEBUG "addr %08x = %08x\n", addr+offset, value);
    }
}
#endif
}

static void ntx_suspend_exit(void)
{
#if 0
	gpio_direction_output (MX6SL_EP_WAKEUP, 1);
#endif

//	gpio_free(MX6SL_I2C2_SCL);
//	gpio_free(MX6SL_I2C2_SDA);
//	mxc_iomux_v3_setup_multiple_pads(mx6sl_ntx_resume_pads, ARRAY_SIZE(mx6sl_ntx_resume_pads));
	ntx_gpio_resume ();

}

static const struct pm_platform_data mx6sl_ntx_pm_data __initconst = {
	.name = "imx_pm",
	.suspend_enter = ntx_suspend_enter,
	.suspend_exit = ntx_suspend_exit,
};

void ntx_wacom_reset(bool on) {
	gpio_direction_output (MX6SL_WACOM_RST, on);
}

int ntx_check_suspend (void)
{
	return gpio_get_value(gMX6SL_IR_TOUCH_INT)?0:1;
}

static void ntx_gpio_init(void)
{
	mxc_iomux_v3_setup_multiple_pads(mx6sl_brd_ntx_pads,
					ARRAY_SIZE(mx6sl_brd_ntx_pads));



	if(31==gptHWCFG->m_val.bPCB||32==gptHWCFG->m_val.bPCB) {
		// E60Q0X|E60Q1X.
		mxc_iomux_v3_setup_multiple_pads(mx6sl_brd_ntx_sd4_gpio_pads,
					ARRAY_SIZE(mx6sl_brd_ntx_sd4_gpio_pads));
		mxc_iomux_v3_setup_multiple_pads(mx6sl_brd_ntx_sd1_pads,
					ARRAY_SIZE(mx6sl_brd_ntx_sd1_pads));
#if 1
		mxc_iomux_v3_setup_multiple_pads(mx6sl_ntx_q12_wifictrl_pads,
					ARRAY_SIZE(mx6sl_ntx_q12_wifictrl_pads));
#endif
		giISD_3V3_ON_Ctrl=-1;
	}	
	else {
		mxc_iomux_v3_setup_multiple_pads(mx6sl_brd_ntx_sd4_pads,
					ARRAY_SIZE(mx6sl_brd_ntx_sd4_pads));
		mxc_iomux_v3_setup_multiple_pads(mx6sl_brd_ntx_sd1_gpio_pads,
					ARRAY_SIZE(mx6sl_brd_ntx_sd1_gpio_pads));

#if 1
		mxc_iomux_v3_setup_multiple_pads(mx6sl_ntx_q22_wifictrl_pads,
					ARRAY_SIZE(mx6sl_ntx_q22_wifictrl_pads));
#endif
		if(1==gptHWCFG->m_val.bLed) {
			// RGB/G type LED .
			// ON_LED# 
			mxc_iomux_v3_setup_pad(MX6SL_PAD_SD1_DAT2__GPIO_5_13_PULLHIGH);
		}

		if(!NTXHWCFG_TST_FLAG(gptHWCFG->m_val.bPCB_Flags,0)) {
			// Key matrix ON .
		}
		else {
			// GPIO key .
			mxc_iomux_v3_setup_multiple_pads(mx6sl_brd_ntx_kb_gpio_pads,
				ARRAY_SIZE(mx6sl_brd_ntx_kb_gpio_pads));
			//udelay(1);
		}
		
		gMX6SL_NTX_ACIN_PG = IMX_GPIO_NR(5, 14);	
		gMX6SL_NTX_CHG = IMX_GPIO_NR(5, 15);	
		gMX6SL_MSP_INT = IMX_GPIO_NR(5, 11);	
		gMX6SL_PWR_SW = IMX_GPIO_NR(5, 8);	

		// TP INT / TP RST
		gMX6SL_IR_TOUCH_INT = IMX_GPIO_NR(5, 6);
		gMX6SL_IR_TOUCH_RST = IMX_GPIO_NR(5, 9);
		if(3==gptHWCFG->m_val.bTouchType||54==gptHWCFG->m_val.bPCB)
		{
			// C touch PCB design .
			// ED0Q1X|E60QLX|E60QMX
			gMX6SL_IR_TOUCH_RST = IMX_GPIO_NR(5, 13);
		}
		if(14==gptHWCFG->m_val.bTouchCtrl) { 
			/* CYTTSP */
			ntx_cyttsp5_init();
		}

		if(49==gptHWCFG->m_val.bPCB) {
			// E60QDX
			mxc_iomux_v3_setup_pad(MX6SL_PAD_WDOG_B__WDOG1_WDOG_B);
			gpio_request (MX6SL_USB_ID, "MX6SL_USB_ID");
			gpio_direction_input (MX6SL_USB_ID);
		}

		gMX6SL_HALL_EN = IMX_GPIO_NR(5, 12);	

		//
		// LED assign ...
		//
		gMX6SL_CHG_LED = IMX_GPIO_NR(5, 10);	
		if(37==gptHWCFG->m_val.bPCB) {
			// E60QB0 .
			gMX6SL_ACT_LED = IMX_GPIO_NR(5, 13);	
			gMX6SL_ON_LED = IMX_GPIO_NR(5, 7);	
		}
		else if(46==gptHWCFG->m_val.bPCB||48==gptHWCFG->m_val.bPCB||
				50==gptHWCFG->m_val.bPCB||58==gptHWCFG->m_val.bPCB||
				61==gptHWCFG->m_val.bPCB)
		{
			// E60Q9X/E60QAX/E60QFX/E60QJX/E60QKX .
			 
			// ON_LED# pull high .
			mxc_iomux_v3_setup_pad(MX6SL_PAD_SD1_DAT2__GPIO_5_13_PULLHIGH);

			gMX6SL_ACT_LED = IMX_GPIO_NR(5, 7);
			gMX6SL_ON_LED = IMX_GPIO_NR(5, 13);
			gMX6SL_CHG_LED = IMX_GPIO_NR(5, 13);
		}
		else if(42==gptHWCFG->m_val.bPCB){
			// E60Q6X 
			mxc_iomux_v3_setup_pad(MX6SL_PAD_SD1_DAT2__GPIO_5_13_PULLHIGH);
			gMX6SL_ACT_LED = IMX_GPIO_NR(5, 13);	
			gMX6SL_ON_LED = IMX_GPIO_NR(5, 13);
			gMX6SL_CHG_LED = IMX_GPIO_NR(5, 13);
		}
		else if(55==gptHWCFG->m_val.bPCB){
			// E70Q0X .
			mxc_iomux_v3_setup_pad(MX6SL_PAD_SD1_DAT7__GPIO_5_10_OUPUT);// blue led .
			gMX6SL_ACT_LED = IMX_GPIO_NR(5, 10);
			gMX6SL_ON_LED = IMX_GPIO_NR(5, 7);
			gMX6SL_CHG_LED = IMX_GPIO_NR(5, 9);
			ricoh_battery_data.alarm_vol_mv = 3670;		// set battery critical to 3.76mV
			ricoh_battery_data.type[0].fg_poff_vbat = 3730;		// set battery 0% to 3.73V
		}
		else {
			gMX6SL_ACT_LED = IMX_GPIO_NR(5, 7);	
			gMX6SL_ON_LED = IMX_GPIO_NR(5, 7);	
		}



 		gMX6SL_WIFI_3V3 = IMX_GPIO_NR(4, 29);
 		gMX6SL_WIFI_RST = IMX_GPIO_NR(5, 0);
 		gMX6SL_WIFI_INT = IMX_GPIO_NR(4, 31);
 		mx6_ntx_sd_wifi_data.cd_gpio = gMX6SL_WIFI_3V3;

	 	if(37==gptHWCFG->m_val.bPCB) {
			// E60QBX.
			gpio_request (GPIO_ESD_3V3_ON, "ESD_3V3_ON");
			gpio_direction_input (GPIO_ESD_3V3_ON);
			gpio_free (GPIO_ESD_3V3_ON);
			giISD_3V3_ON_Ctrl = -1;
		}
		else {
			if(36==gptHWCFG->m_val.bPCB || 40==gptHWCFG->m_val.bPCB) {
				// E60Q3X/E60Q5X
				gMX6SL_CHG_LED = IMX_GPIO_NR(5, 7);	
				gpio_request (MX6SL_KL25_INT2, "MX6SL_KL25_INT2");
				gpio_direction_input (MX6SL_KL25_INT2);
			}
			else if(50==gptHWCFG->m_val.bPCB) {
				// E60QFX 
				gpio_request (IMX_GPIO_NR(3, 31), "MX6SL_KL25_INT2");
				gpio_direction_input (IMX_GPIO_NR(3, 31));
				i2c_kl25_binfo.irq = gpio_to_irq(IMX_GPIO_NR(3, 31));

				gpio_request (MX6SL_P2_KL25_INT2, "MX6SL_P2_KL25_INT2");
				gpio_direction_input (MX6SL_P2_KL25_INT2);

				gpio_request (GPIO_ESD_3V3_ON, "ESD_3V3_ON");
				gpio_direction_input (GPIO_ESD_3V3_ON);
				gpio_free (GPIO_ESD_3V3_ON);
				giISD_3V3_ON_Ctrl = -1;
			}	
			else {
				if(0==gptHWCFG->m_val.bPMIC) {
					//若有RICOH PMIC則ESD_3V3_ON都會交由PMIC控制.
					gpio_request (GPIO_ESD_3V3_ON, "ESD_3V3_ON");
					gpio_direction_output (GPIO_ESD_3V3_ON, 1);
				}
			}

			if(0==gptHWCFG->m_val.bPMIC) {
				gpio_request (GPIO_ISD_3V3_ON, "ISD_3V3_ON");
				if(33==gptHWCFG->m_val.bPCB)  {
					// E60Q2X .
					giISD_3V3_ON_Ctrl = 0;
				}
				else {
					giISD_3V3_ON_Ctrl = 1;
				}
			}
		}


	 	if(1!=gptHWCFG->m_val.bPMIC) {
			gpio_request (GPIO_IR_3V3_ON, "IR_3V3_ON");
			gpio_direction_output (GPIO_IR_3V3_ON, 1);
			//gpio_request (GPIO_EP_3V3_ON, "EP_3V3_ON");
			//gpio_direction_output (GPIO_EP_3V3_ON, 1);
	 	}
	
		if(2==gptHWCFG->m_val.bTouch2Ctrl) {
			// Wacom Digitizer
			gpio_request (MX6SL_WACOM_PDCT, "MX6SL_WACOM_PDCT");
			gpio_direction_input (MX6SL_WACOM_PDCT);
			gpio_request (MX6SL_WACOM_FWE, "MX6SL_WACOM_FWE");
			gpio_direction_output (MX6SL_WACOM_FWE, 0);
			gpio_request (MX6SL_WACOM_RST, "MX6SL_WACOM_RST");
			ntx_wacom_reset(0);

	 		if(42==gptHWCFG->m_val.bPCB) {
				// Wacom GPIOs
				gpio_request (MX6SL_WACOM_INT, "MX6SL_WACOM_INT");
				gpio_direction_input (MX6SL_WACOM_INT);
				i2c_register_board_info(1,&i2c_wacom_binfo,1);
			}
			else {
				gpio_request (MX6SL_WACOM_INT_4_1, "MX6SL_WACOM_INT");
				gpio_direction_input (MX6SL_WACOM_INT_4_1);
				i2c_register_board_info(0,&i2c_wacom_binfo,1);
			}
		}
		else if(3==gptHWCFG->m_val.bTouch2Ctrl) {
			if(42==gptHWCFG->m_val.bPCB) {
				gpio_request (MX6SL_WALTOP_INT_4_0, "MX6SL_WALTOP_INT_4_0");
				gpio_direction_input (MX6SL_WALTOP_INT_4_0);
				gpio_request (MX6SL_WALTOP_RST_3_30, "MX6SL_WALTOP_RST_3_30");
				gpio_direction_output (MX6SL_WALTOP_RST_3_30, 1);

				i2c_waltop_binfo.platform_data = MX6SL_WALTOP_RST_3_30;
				i2c_waltop_binfo.irq = gpio_to_irq(MX6SL_WALTOP_INT_4_0);
				i2c_register_board_info(1,&i2c_waltop_binfo,1);
			} else {
				gpio_request (MX6SL_WALTOP_INT, "MX6SL_WALTOP_INT");
				gpio_direction_input (MX6SL_WALTOP_INT);
				gpio_request (MX6SL_WALTOP_RST, "MX6SL_WALTOP_RST");
				gpio_direction_output (MX6SL_WALTOP_RST, 1);
				i2c_register_board_info(0,&i2c_waltop_binfo,1);
			}
		}

		if(2==gptHWCFG->m_val.bLightSensor) {
			gpio_request (MX6SL_SI114X_INT, "MX6SL_SI114X_INT");
			gpio_direction_input (MX6SL_SI114X_INT);
			i2c_register_board_info(0,&i2c_si114x_binfo,1);
		} 
	}

	if(-1!=giISD_3V3_ON_Ctrl) {
		gpio_direction_output (GPIO_ISD_3V3_ON, giISD_3V3_ON_Ctrl?1:0);
	}

 	i2c_zforce_ir_touch_binfo.platform_data = gMX6SL_IR_TOUCH_INT;
 	i2c_zforce_ir_touch_binfo.irq = gpio_to_irq(gMX6SL_IR_TOUCH_INT);

	i2c_elan_touch_binfo.platform_data = gMX6SL_IR_TOUCH_INT;
 	i2c_elan_touch_binfo.irq = gpio_to_irq(gMX6SL_IR_TOUCH_INT);
	if(47==gptHWCFG->m_val.bPCB || 54==gptHWCFG->m_val.bPCB) { //ED0Q02, ED0Q1X
		i2c_elan_touch_binfo.addr = 0x10;
	}

	fts_data.irq_gpio = gMX6SL_IR_TOUCH_INT;
	fts_data.reset_gpio = MX6SL_PAD_SD1_DAT2__GPIO_5_13;
	i2c_fts_touch_binfo.irq = gpio_to_irq(gMX6SL_IR_TOUCH_INT);

	if (1==gptHWCFG->m_val.bPMIC) {
		if (46==gptHWCFG->m_val.bPCB||0 == gptHWCFG->m_val.bMicroP)	{
			// MSP430 or E60Q9X(wrong config) 
			i2c_sysmp_msp430_binfo.irq = gpio_to_irq(GPIO_KB_ROW1);
		}
		i2c_sysmp_ricoh619_binfo.irq = gpio_to_irq(gMX6SL_MSP_INT);
	}
	else
		i2c_sysmp_msp430_binfo.irq = gpio_to_irq(gMX6SL_MSP_INT);

	ntx_misc_info.acin_gpio     = gMX6SL_NTX_ACIN_PG;
	ntx_misc_info.chg_gpio      = gMX6SL_NTX_CHG;

	if(37!=gptHWCFG->m_val.bPCB) {
		gpio_request (MX6SL_HW_ID0, "MX6SL_HW_ID0");
		gpio_request (MX6SL_HW_ID1, "MX6SL_HW_ID1");
		gpio_request (MX6SL_HW_ID2, "MX6SL_HW_ID2");
		gpio_request (MX6SL_HW_ID3, "MX6SL_HW_ID3");
		gpio_request (MX6SL_HW_ID4, "MX6SL_HW_ID4");
		gpio_direction_input (MX6SL_HW_ID0);
		gpio_direction_input (MX6SL_HW_ID1);
		gpio_direction_input (MX6SL_HW_ID2);
		gpio_direction_input (MX6SL_HW_ID3);
		gpio_direction_input (MX6SL_HW_ID4);
	}
	
	gpio_request (gMX6SL_ON_LED, "MX6SL_ON_LED");
	gpio_request (gMX6SL_ACT_LED, "MX6SL_ACT_LED");
	gpio_direction_input (gMX6SL_ACT_LED);
	gpio_direction_output (gMX6SL_ON_LED, 0);

	if(gMX6SL_CHG_LED!=gMX6SL_ON_LED) {
		gpio_request (gMX6SL_CHG_LED, "MX6SL_CHG_LED");
		gpio_direction_input (gMX6SL_CHG_LED);
	}

	if(36==gptHWCFG->m_val.bPCB || 40==gptHWCFG->m_val.bPCB || 0!=gptHWCFG->m_val.bHOME_LED_PWM ) {
    // E60Q3X/E60Q5X or HOME_LED_PWM enabled .
		gpio_request (gMX6SL_HOME_LED, "MX6SL_HOME_LED");
	}
	
	gpio_request (gMX6SL_NTX_ACIN_PG, "MX6SL_NTX_ACIN_PG");
	gpio_direction_input (gMX6SL_NTX_ACIN_PG);
	
	gpio_request (gMX6SL_NTX_CHG, "MX6SL_NTX_CHG");
	gpio_direction_input (gMX6SL_NTX_CHG);
	
	gpio_request (gMX6SL_MSP_INT, "MX6SL_MSP_INT");
	gpio_direction_input (gMX6SL_MSP_INT);
	//#ifndef CONFIG_ANDROID //[
	if(0==gptHWCFG->m_val.bUIStyle) {
		gpio_request (gMX6SL_PWR_SW, "MX6SL_PWR_SW");
		gpio_direction_input (gMX6SL_PWR_SW);
	}
	//#endif //]CONFIG_ANDROID
	
	gpio_request (gMX6SL_IR_TOUCH_INT, "MX6SL_IR_TOUCH_INT");
	gpio_direction_input (gMX6SL_IR_TOUCH_INT);
	
	gpio_request (gMX6SL_IR_TOUCH_RST, "MX6SL_IR_TOUCH_RST");
	gpio_direction_output (gMX6SL_IR_TOUCH_RST, 0);
	mdelay (10);
	gpio_direction_input (gMX6SL_IR_TOUCH_RST);
	if(0==gptHWCFG->m_val.bHallSensor) {
		gpio_request (gMX6SL_HALL_EN, "MX6SL_HALL_EN");
		gpio_direction_input (gMX6SL_HALL_EN);
	}
	
	mxc_iomux_v3_setup_pad(MX6SL_PAD_SD2_DAT5__GPIO_4_31);
	gpio_request (gMX6SL_WIFI_RST, "MX6SL_WIFI_RST");
	gpio_request (gMX6SL_WIFI_3V3, "MX6SL_WIFI_3V3");
	gpio_request (gMX6SL_WIFI_INT, "MX6SL_WIFI_INT");
	gpio_direction_input (gMX6SL_WIFI_INT);
	ntx_wifi_power_ctrl (0);
	
	//Front Light
	gpio_request (MX6SL_FL_EN, "MX6SL_FL_EN");
	gpio_direction_input (MX6SL_FL_EN);
	gpio_request (MX6SL_FL_R_EN, "MX6SL_FL_R_EN");
	gpio_direction_input (MX6SL_FL_R_EN);
	if(4==gptHWCFG->m_val.bFL_PWM||5==gptHWCFG->m_val.bFL_PWM) 
	{
		// FL PWM source is MSP430+LM3630 .
		gpio_request (MX6SL_FL_W_H_EN, "MX6SL_FL_W_H_EN");
		mxc_iomux_v3_setup_pad(MX6SL_PAD_EPDC_SDCE3__GPIO_1_30);
		gMX6SL_FL_W_H_EN = MX6SL_FL_W_H_EN; // front light high level enable gpio .
		gMX6SL_FL_PWR_EN = MX6SL_FL_PWR_ON; // front light high level enable gpio .
	}
#if 0	//[
	gpio_request (MX6SL_EP_PWRALL, "MX6SL_EP_PWRALL" );
	gpio_request (MX6SL_EP_WAKEUP	, "MX6SL_EP_WAKEUP" );
	gpio_request (MX6SL_EP_PWRUP	, "MX6SL_EP_PWRUP" );
	gpio_request (MX6SL_EP_INT	    , "MX6SL_EP_INT" );
	gpio_request (MX6SL_EP_PWRSTAT  , "MX6SL_EP_PWRSTAT" );
	gpio_request (MX6SL_EP_VCOM	    , "MX6SL_EP_VCOM" );
	gpio_direction_output (MX6SL_EP_PWRALL, 1);
	gpio_direction_output (MX6SL_EP_WAKEUP, 0);
	gpio_direction_output (MX6SL_EP_PWRUP, 0);
	gpio_direction_output (MX6SL_EP_VCOM, 0);
	gpio_direction_input (MX6SL_EP_INT);
	gpio_direction_input (MX6SL_EP_PWRSTAT);
#endif //]
	if(NTXHWCFG_TST_FLAG(gptHWCFG->m_val.bFrontLight_Flags,2)){
		// FL_EN invert .
		//printk("FL_EN inverted !\n",__FUNCTION__);
		ntx_fl_set_turnon_level(1);
	}

}

/*!
 * Board specific initialization.
 */
static void __init mx6_ntx_init(void)
{
	u32 i;
	int iMSP430_I2C_Chn;
	struct esdhc_platform_data *pt_esdhc_ntx_isd_data;

	_parse_cmdline();

	mxc_iomux_v3_setup_multiple_pads(mx6sl_brd_pads,
					ARRAY_SIZE(mx6sl_brd_pads));
					
	ntx_gpio_init ();

	gpiofn_init();

#ifdef CONFIG_MX6_INTER_LDO_BYPASS
	gp_reg_id = mx6sl_ntx_dvfscore_data.reg_id;
	soc_reg_id = mx6sl_ntx_dvfscore_data.soc_id;
#else
	gp_reg_id = mx6sl_ntx_dvfscore_data.reg_id;
	soc_reg_id = mx6sl_ntx_dvfscore_data.soc_id;
	pu_reg_id = mx6sl_ntx_dvfscore_data.pu_id;
	mx6_cpu_regulator_init();
#endif


	// MicroP channel definition . 
	if( (46==gptHWCFG->m_val.bPCB && gptHWCFG->m_val.bPCB_REV>=0x10) ||
			48==gptHWCFG->m_val.bPCB || 
			50==gptHWCFG->m_val.bPCB ||
			51==gptHWCFG->m_val.bPCB ||
			55==gptHWCFG->m_val.bPCB)
	{
		// E60Q9X rev >=A10 ...
		// E60QAX | E60QFX | E60QHX ...
		// MSP430 @ I2C1
		iMSP430_I2C_Chn = 0;//I2C1
	}
	else {
		iMSP430_I2C_Chn = 2;//I2C3 in curcuit .
		if(0==gptHWCFG->m_val.bPMIC) {
			// 當MSP430取代為PMIC工作時，它非常忙錄，高速的頻率有可能會使它出錯。
			mx6_ntx_i2c2_data.bitrate = 100000;
		}
	}
//	imx6q_add_imx_snvs_rtc();


	imx6q_add_imx_i2c(0, &mx6_ntx_i2c0_data);
	imx6q_add_imx_i2c(1, &mx6_ntx_i2c1_data);
	imx6q_add_imx_i2c(2, &mx6_ntx_i2c2_data);

	if (4==gptHWCFG->m_val.bTouchType) {
		// IR touch type 
		if ( 31==gptHWCFG->m_val.bPCB||32==gptHWCFG->m_val.bPCB|| 
				 33==gptHWCFG->m_val.bPCB||36==gptHWCFG->m_val.bPCB||
         40==gptHWCFG->m_val.bPCB||
			 (46==gptHWCFG->m_val.bPCB&&gptHWCFG->m_val.bPCB_REV<0x10) )
		{
			// E60Q2X/E60Q3X/E60Q1X/E60Q2X/E60Q5X/<E60Q9XA10
			i2c_register_board_info(0,&i2c_zforce_ir_touch_binfo,1);
		}
		else {
			// 
			i2c_register_board_info(1,&i2c_zforce_ir_touch_binfo,1);
		}
	}
	else if (3==gptHWCFG->m_val.bTouchType) {
		// C touch type .
		if(14==gptHWCFG->m_val.bTouchCtrl) { // CYTT21X
			i2c_register_board_info(1,&i2c_cyttsp_binfo,1);
		}
		else if (15==gptHWCFG->m_val.bTouchCtrl) {	// FocalTech C Touch
			i2c_register_board_info(0,&i2c_fts_touch_binfo,1);
		} else { // elan
			if(55==gptHWCFG->m_val.bPCB||59==gptHWCFG->m_val.bPCB) {
				// E70Q0X|E60QLX
			 
				// C touch @ I2C2 .
				i2c_register_board_info(1,&i2c_elan_touch_binfo,1);
			}
			else {
				// C touch @ I2C1 .
				i2c_register_board_info(0,&i2c_elan_touch_binfo,1);
			}
		}
	}
	else {
		printk("TouchType %d do not support yet ! no touch driver will be loaded \n",(int) gptHWCFG->m_val.bTouchType);
	}

	if(1==gptHWCFG->m_val.bPMIC){ // [ Ricoh PMIC
		// RC5T619 .
		
		if (4==gptHWCFG->m_val.bRamType || 10==gptHWCFG->m_val.bRamType) {
			// LPDDR3/DDR3 .
			
			// Core4_1V2 .
			pdata_dc4_0.regulator.constraints.always_on = 0;
			pdata_dc4_0.regulator.constraints.boot_on = 0;
			pdata_dc4_0.init_enable = 0;
			pdata_dc4_0.init_apply = 1;
			// Core5_1V2
			pdata_ldo3_0.regulator.constraints.always_on = 0;
			pdata_ldo3_0.regulator.constraints.boot_on = 0;
			pdata_ldo3_0.init_enable = 0;
			pdata_ldo3_0.init_apply = 1;
			// DDR_0V6
			pdata_ldo6_0.regulator.constraints.always_on = 0;
			pdata_ldo6_0.regulator.constraints.boot_on = 0;
			pdata_ldo6_0.init_enable = 0;
			pdata_ldo6_0.init_apply = 1;
		}

		if(!NTXHWCFG_TST_FLAG(gptHWCFG->m_val.bPCB_Flags,4)) {
			// Panel is designed for low voltage .
			printk("ldo8_1v8 ouput 3v3\n");
			pdata_ldo8_0.regulator.constraints.min_uV = 3300*1000;
			pdata_ldo8_0.regulator.constraints.max_uV = 3300*1000;
			//pdata_ldo8_0.regulator.constraints.always_on = 1;
			//pdata_ldo8_0.regulator.constraints.boot_on = 1;
			pdata_ldo8_0.init_uV = 3300*1000;
			//pdata_ldo8_0.init_enable = 1;
			pdata_ldo8_0.sleep_uV = 3300*1000;
		}
		if (50==gptHWCFG->m_val.bPCB || 47==gptHWCFG->m_val.bPCB || 54==gptHWCFG->m_val.bPCB) {
			// E60QFx , ED0Q0x , ED0Q1x
			pdata_dc2_0.sleep_uV = 3100*1000;	// core3_3v3
			pdata_dc4_0.sleep_uV = 1140*1000;	// core4_1v2
			pdata_dc5_0.sleep_uV = 1700*1000;	// core4_1v8
			pdata_ldo1_0.sleep_uV = 3100*1000;	// IR_3v3
			pdata_ldo2_0.sleep_uV = 3100*1000;	// core1_3v3
			pdata_ldo5_0.sleep_uV = 3100*1000;	// SPD_3v3
		}


		if(0==gptHWCFG->m_val.bFrontLight) {
			// FL_3V3 disabled .
			pdata_ldo7_0.regulator.constraints.always_on = 0;
			pdata_ldo7_0.regulator.constraints.boot_on = 0;
			pdata_ldo7_0.init_enable = 0;
			pdata_ldo7_0.init_apply = 1;
		}

		if (58==gptHWCFG->m_val.bPCB || 61==gptHWCFG->m_val.bPCB) {
			// E60QJX/E60QKX .

			// LDO_1V8 not used .
			pdata_ldo8_0.regulator.constraints.always_on = 0;
			pdata_ldo8_0.regulator.constraints.boot_on = 0;
			pdata_ldo8_0.init_enable = 0;
			pdata_ldo8_0.init_apply = 1;
			pdata_dc2_0.sleep_uV = 2800*1000;	// core3_3v3
			pdata_ldo2_0.sleep_uV = 2800*1000;	// core1_3v3 (VDD_SNVS_IN)

			if (58==gptHWCFG->m_val.bPCB) { // E60QJx
				ricoh_battery_data.alarm_vol_mv = 3100;		// set battery critical to 3.1mV
				ricoh_battery_data.type[0].fg_poff_vbat = 3400;		// set battery 0% to 3.4V
			}
		}

		if (50==gptHWCFG->m_val.bPCB)
			pdata_ldo7_0.init_enable = 1;

		// PCB is designed for low voltage .
		ntx_ricoh_data.irq_base = irq_alloc_descs (-1, 0, RICOH61x_NR_IRQS, "RICOH61x");
		if (0 < ntx_ricoh_data.irq_base) {
			ricoh_battery_data.irq = ntx_ricoh_data.irq_base;
			ricoh_rtc_data.irq = ntx_ricoh_data.irq_base;
			printk ("[%s-%d] irq_alloc_descs return %d for %d RICOH61x\n",__func__, __LINE__, ntx_ricoh_data.irq_base, RICOH61x_NR_IRQS);
		}
		else {
			printk ("[%s-%d] RICOH61x irq_alloc_descs failed with %d\n",__func__, __LINE__, ntx_ricoh_data.irq_base, RICOH61x_NR_IRQS);
		}
		if (54==gptHWCFG->m_val.bPCB) { // ED0Q1x
			ricoh_battery_data.type[0].jt_en = 1;
		}
		i2c_register_board_info(2,&i2c_sysmp_ricoh619_binfo,1);
	//	platform_device_register(&ricoh_device_rtc);
		pm_power_off = ricoh619_power_off;
		if (40==gptHWCFG->m_val.bPCB || 50==gptHWCFG->m_val.bPCB || 58==gptHWCFG->m_val.bPCB) {
			// I2C3 set 400kHz for E60Q5x & E60QFx for faster suspending .
			mx6_ntx_i2c2_data.bitrate = 400000;
		}
	}//] Ricoh PMIC


	if(38!=gptHWCFG->m_val.bPCB&&37!=gptHWCFG->m_val.bPCB)
	{
		if (46==gptHWCFG->m_val.bPCB||
				(40==gptHWCFG->m_val.bPCB&&0==gptHWCFG->m_val.bPCB_LVL)||
				0 == gptHWCFG->m_val.bMicroP)	{
			// MSP430 or E60Q9X/E60Q5XAX (wrong config) 
			i2c_register_board_info(iMSP430_I2C_Chn,&i2c_sysmp_msp430_binfo,1);
		}
	}

	if(0==gptHWCFG->m_val.bRTC) {
		// RTC use MSP430
		platform_device_register(&ntx_device_rtc);
	}



	///////////
	// dedicated FL controller ...
	//
	{

		if(1==gptHWCFG->m_val.bFL_PWM){
			// Front light PWM source is ht68f20
			i2c_register_board_info(0,&i2c_ht68f20_binfo,1);
		}
		else if(4==gptHWCFG->m_val.bFL_PWM || 5==gptHWCFG->m_val.bFL_PWM) 
		{
			// Front light PWM source is MSP430+LM3630 .
			mxc_iomux_v3_setup_pad(MX6SL_PAD_KEY_ROW2__GPIO_3_29_OUTPUT);
			gpio_request(MX6SL_FL_PWR_ON, "FL_pwr");
			fl_pwr_force_enable(2);
			i2c_register_board_info(1,&i2c_lm3630a_bl_binfo,1);
			if(5==gptHWCFG->m_val.bFL_PWM) {
				i2c_register_board_info(1,&i2c_lm3630a_bl_binfo2,1);
			}
		}
		else if (2==gptHWCFG->m_val.bFL_PWM||6==gptHWCFG->m_val.bFL_PWM||7==gptHWCFG->m_val.bFL_PWM) {
				// Front light PWM is lm3630a
			fl_pwr_force_enable(2);
			i2c_register_board_info(0,&i2c_lm3630a_bl_binfo,1);
		}
	}

	if(3==gptHWCFG->m_val.bRSensor) {
		// g-sensor with microP KL25 
		i2c_register_board_info(0,&i2c_kl25_binfo,1);
	}
	else if(4==gptHWCFG->m_val.bRSensor) {
		// g-sensor : MMA8652
		//if(36!=gptHWCFG->m_val.bPCB) 
		{
		/*  20140220 temporarily remove MMA8652 from E60Q32 to save power
		    since not currently used but installed (MMA8652 default mode: Standby) */
			i2c_register_board_info(0,&i2c_mma8652_binfo,1);
		}
	}
	

	// 2nd RSensor ...
	if(3==gptHWCFG->m_val.bRSensor2) {
		i2c_register_board_info(0,&i2c_kl25_2_binfo,1);
	}


	//i2c_register_board_info(0, mxc_i2c0_board_info,
	//		ARRAY_SIZE(mxc_i2c0_board_info));
	//i2c_register_board_info(1, mxc_i2c1_board_info,
	//		ARRAY_SIZE(mxc_i2c1_board_info));
	//i2c_register_board_info(2, mxc_i2c2_board_info,
	//		ARRAY_SIZE(mxc_i2c2_board_info));

	/* only camera on I2C2, that's why we can do so */
//	if (csi_enabled == 1) {
//		mxc_register_device(&csi_v4l2_devices, NULL);
//		imx6q_add_imx_i2c(2, &mx6_ntx_i2c2_data);
//	}
//	imx6q_add_imx_snvs_rtc();

	/* SPI */
//	imx6q_add_ecspi(0, &mx6_ntx_spi_data);
//	spi_device_init();

//	mx6sl_ntx_init_pfuze100(0);
	imx6q_add_anatop_thermal_imx(1, &mx6sl_anatop_thermal_data);
	imx6q_add_pm_imx(0, &mx6sl_ntx_pm_data);

	mx6_ntx_init_uart();
	imx6x_add_ram_console();
	/* get enet tx reference clk from FEC_REF_CLK pad.
	 * GPR1[14] = 0, GPR1[18:17] = 00
	 */
//	mxc_iomux_set_gpr_register(1, 14, 1, 0);
//	mxc_iomux_set_gpr_register(1, 17, 2, 0);

//	imx6_init_fec(fec_data);

	//platform_device_register(&ntx_vmmc_reg_devices);

	if(2==gptHWCFG->m_val.bIFlash) {
		// eMMC .

		if( NTXHWCFG_TST_FLAG(gptHWCFG->m_val.bPCB_Flags,7) ) {
			// ISD_1V8 .
			mx6_ntx_isd8bits_data.support_18v = 1;
		}

		pt_esdhc_ntx_isd_data = &mx6_ntx_isd8bits_data;
	}
	else if(gptHWCFG->m_val.bPCB>=61 && 0==gptHWCFG->m_val.bIFlash) {
		// internal flash is uSD .
		pt_esdhc_ntx_isd_data = &mx6_ntx_esd_nocd_data;
	}
	else {
		if( NTXHWCFG_TST_FLAG(gptHWCFG->m_val.bPCB_Flags,7) ) {
			// ISD_1V8 .
			mx6_ntx_isd_data.support_18v = 1;
		}
		pt_esdhc_ntx_isd_data = &mx6_ntx_isd_data;
	}

	switch(gptHWCFG->m_val.bPCB) {
	case 31: //E60Q0X .
  case 32: //E60Q1X .
		// SD1 = ISD
		// SD2 = ESD
		// SD3 = SDIO WIFI
		// SD4 = GPIO
		imx6q_add_sdhci_usdhc_imx(giBootPort, pt_esdhc_ntx_isd_data);
		imx6q_add_sdhci_usdhc_imx(2, &mx6_ntx_esd_data); // mmcblk1
		imx6q_add_sdhci_usdhc_imx(1, &mx6_ntx_sd_wifi_data);
		break;

	default:
		// \C5\FD\B7s\AA\BA\B3]\ADp\BA\FB\AB\F9\A6b\B3o\ADӰ϶\F4\A1A\A5H\BAɥi\AF\E0\B9F\A8줣\ADק\EFCODE\B4N\AF\E0\A5Ψ\EC\B7s\BE\F7\BAؤW\A1C
		// SD1 = GPIO
		// SD2 = ESD
		// SD3 = SDIO WIFI
		// SD4 = EMMC
		if(1==giBootPort) {
			// ESD is boot device .
			printk("add usdhc %d as mmcblk0\n",giBootPort+1);
			if(46==gptHWCFG->m_val.bPCB||
				48==gptHWCFG->m_val.bPCB|| 
				51==gptHWCFG->m_val.bPCB|| 
				50==gptHWCFG->m_val.bPCB||
				58==gptHWCFG->m_val.bPCB||
				( gptHWCFG->m_val.bPCB>=59 && 0==gptHWCFG->m_val.bIFlash )
				)
			{
				// Q9X/QAX/QFX/QHX/E60QJX | (new PCB(id>=59)&& internal flash is uSD)
				imx6q_add_sdhci_usdhc_imx(giBootPort, &mx6_ntx_esd_nocd_data);
			}
			else {
				imx6q_add_sdhci_usdhc_imx(giBootPort, &mx6_ntx_esd_data);
			}
			printk("add usdhc 4 as mmcblk1\n");
			imx6q_add_sdhci_usdhc_imx(3, pt_esdhc_ntx_isd_data); // mmcblk1
			printk("add usdhc 3 as sdio for wifi\n");
			imx6q_add_sdhci_usdhc_imx(2, &mx6_ntx_q22_sd_wifi_data); 
		}
		else {
			// SD4 is boot port .
			imx6q_add_sdhci_usdhc_imx(giBootPort, pt_esdhc_ntx_isd_data);
			imx6q_add_sdhci_usdhc_imx(1, &mx6_ntx_esd_data); // mmcblk1
			imx6q_add_sdhci_usdhc_imx(2, &mx6_ntx_q22_sd_wifi_data);
		}
		break;
	}

	mx6_ntx_init_usb();
	imx6q_add_otp();
//	imx6q_add_mxc_pwm(0);
//	imx6q_add_mxc_pwm_backlight(0, &mx6_ntx_pwm_backlight_data);


//		gpio_request(MX6_BRD_LCD_PWR_EN, "elcdif-power-on");
//		gpio_direction_output(MX6_BRD_LCD_PWR_EN, 1);
		//mxc_register_device(&lcd_wvga_device, NULL);

	imx6dl_add_imx_pxp();
	imx6dl_add_imx_pxp_client();

	imx6dl_add_imx_epdc(&epdc_data);
#ifdef CONFIG_IMX_HAVE_PLATFORM_IMX_ELCDIF //[
		imx6dl_add_imx_elcdif(&wvga_fb_data[0]);
#endif //]CONFIG_IMX_HAVE_PLATFORM_IMX_ELCDIF
	//imx6q_add_dvfs_core(&mx6sl_ntx_dvfscore_data);

	if (2 == gptHWCFG->m_val.bAudioCodec) {	// ALC5640 codec
		gpio_request (MX6SL_AD_LDO_EN, "AD_LDO_EN");
		gpio_direction_output (MX6SL_AD_LDO_EN, 0);
		gpio_request (MX6SL_AD_1V8_ON, "AD_1V8_ON");
		gpio_direction_output (MX6SL_AD_1V8_ON, 1);
		gpio_request (MX6SL_AD_3V3_ON, "AD_3V3_ON");
		gpio_direction_output (MX6SL_AD_3V3_ON, 0);
		gpio_direction_output (MX6SL_AD_LDO_EN, 1);

		i2c_register_board_info(0,&i2c_alc5640_codec_binfo,1);

		imx6q_init_audio();
	}
	imx6q_add_viim();
	imx6q_add_imx2_wdt(0, NULL);


#ifdef CONFIG_MXC_GPU_VIV//[
	if(2==gptHWCFG->m_val.bUIStyle||2==gptHWCFG->m_val.bGPU) 
	{
		// android models needs GPU || GPU specified 'MX6SL' . 
		imx_add_viv_gpu(&imx6_gpu_data, &imx6q_gpu_pdata);
	}
#endif//] CONFIG_MXC_GPU_VIV

	//imx6sl_add_device_buttons();
	if(!NTXHWCFG_TST_FLAG(gptHWCFG->m_val.bPCB_Flags,0)) {   
		// key matrix : ON
		
		//mxc_iomux_v3_setup_multiple_pads(mx6sl_brd_ntx_kb_pads,ARRAY_SIZE(mx6sl_brd_ntx_kb_pads));
		//mdelay(1);
		
		imx6sl_add_imx_keypad(&mx6sl_ntx_map_data);
	}
	//else 
	{
		// gpio keys 

		switch(gptHWCFG->m_val.bPCB) {
		case 32://E60Q1X
		case 31://E60Q0X
			// use gpio instead of keymatrix ...
			gpio_request (GPIO_KB_COL0, "KB_COL0");
			gpio_direction_output (GPIO_KB_COL0, 0);
			gpio_request (GPIO_KB_COL1, "KB_COL1");
			gpio_direction_output (GPIO_KB_COL1, 0);

			ntx_gpio_key_data.buttons = gpio_key_matrix_FL;
			ntx_gpio_key_data.nbuttons = ARRAY_SIZE(gpio_key_matrix_FL);
			break;
		case 33://E60Q2X
      		ntx_gpio_key_data.buttons = gpio_key_HOME;
			ntx_gpio_key_data.nbuttons = ARRAY_SIZE(gpio_key_HOME);
			break;
    /*case 36://E60Q3X
			ntx_gpio_key_data.buttons = gpio_key_HOME_FL;
			ntx_gpio_key_data.nbuttons = ARRAY_SIZE(gpio_key_HOME_FL);
			break;*/
		default:
		      switch(gptHWCFG->m_val.bKeyPad) { //key pad define through bKeyPad in hwconfig
		        case 12: // NO_Key
		          ntx_gpio_key_data.buttons = gpio_key_None;
		          ntx_gpio_key_data.nbuttons = ARRAY_SIZE(gpio_key_None);
							break;
		        case 11: // FL_Key
		          ntx_gpio_key_data.buttons = gpio_key_FL;
		          ntx_gpio_key_data.nbuttons = ARRAY_SIZE(gpio_key_FL);
		          break;
						case 16: // FL+HOME PAD
		        case 13: // FL+HOME KEY
							if(50==gptHWCFG->m_val.bPCB) {
								// E60QFX
		          	ntx_gpio_key_data.buttons = gpio_key_HOME_FL2;
		          	ntx_gpio_key_data.nbuttons = ARRAY_SIZE(gpio_key_HOME_FL2);
							}
							else {
		          	ntx_gpio_key_data.buttons = gpio_key_HOME_FL;
		          	ntx_gpio_key_data.nbuttons = ARRAY_SIZE(gpio_key_HOME_FL);
							}
		          break;
		        case 14: // HOME
			case 18: // HOMEPAD
							if(58==gptHWCFG->m_val.bPCB) {
								//E60QJX .
		          	ntx_gpio_key_data.buttons = gpio_key_HOME_ROW0;
		          	ntx_gpio_key_data.nbuttons = ARRAY_SIZE(gpio_key_HOME_ROW0);
							}
							else {
		          	ntx_gpio_key_data.buttons = gpio_key_HOME;
		          	ntx_gpio_key_data.nbuttons = ARRAY_SIZE(gpio_key_HOME);
							}
		          break;
		        case 17: // RETURN+HOME+MENU
		          ntx_gpio_key_data.buttons = gpio_key_RETURN_HOME_MENU;
		          ntx_gpio_key_data.nbuttons = ARRAY_SIZE(gpio_key_RETURN_HOME_MENU);
		          break;
		        case 22: // LEFT+RIGHT+HOME+MENU
		          ntx_gpio_key_data.buttons = gpio_key_LEFT_RIGHT_HOME_MENU;
		          ntx_gpio_key_data.nbuttons = ARRAY_SIZE(gpio_key_LEFT_RIGHT_HOME_MENU);
		          break;
		        default: // FL+HOME
		          ntx_gpio_key_data.buttons = gpio_key_HOME_FL;
					    ntx_gpio_key_data.nbuttons = ARRAY_SIZE(gpio_key_HOME_FL);
		          break;
		      }
			break;
		}

		for(i=0;i<ntx_gpio_key_data.nbuttons;i++) {
			if(0==strcmp(ntx_gpio_key_data.buttons[i].desc,"btn home")) {

#ifdef TOUCH_HOME_LED//[
				if(36==gptHWCFG->m_val.bPCB || 40==gptHWCFG->m_val.bPCB || 
					 0!=gptHWCFG->m_val.bHOME_LED_PWM ||
					 16==gptHWCFG->m_val.bKeyPad || 18==gptHWCFG->m_val.bKeyPad )
				{
					// E60Q32/E60Q5X/models with HOMELED/HOMEPAD .
					/*
				 	* prepare hook functions of keys .
		 			*/
					gptGPIO_HOME_KEY = &ntx_gpio_key_data.buttons[i];
					
					if( 36==gptHWCFG->m_val.bPCB || 2==gptHWCFG->m_val.bHOME_LED_PWM ) 
					{
						// E60Q3X or HOME LED is GPIO controlled by SOC .
						mxc_iomux_v3_setup_pad(MX6SL_PAD_SD1_DAT7__GPIO_5_10_OUPUT);
						INIT_DELAYED_WORK(&homeled_pwrdwn_work,_homeled_work_func);
						INIT_DELAYED_WORK(&homepad_check_work,_homepad_work_func);
					}

					ntx_gpio_key_data.buttons[i].hook = ntx_touch_home_key_hook;
					ntx_gpio_key_data.buttons[i].debounce_interval = 50;

				}


#endif //]TOUCH_HOME_LED

				if(2==gptHWCFG->m_val.bUIStyle) {
					ntx_gpio_key_data.buttons[i].code = KEY_HOME;
				}
			}
			else {
				if(0==gptHWCFG->m_val.bUIStyle) {
					if(0==strcmp(ntx_gpio_key_data.buttons[i].desc,"btn power")) {
						printk("%s(),Ebrmain remote power key in gpio keys \n",__FUNCTION__);
						ntx_gpio_key_data.nbuttons-=1;
					}
				}
			}
		}

		if( 1==gptHWCFG->m_val.bHOME_LED_PWM ) {
			// HOME LED is controled by MSP430 .
			if(MSP430_HOMELED_TYPE_PWM==msp430_homeled_type_get(0)) {
				giHomeLED_Delay_Ticks = 200; 
			}
			else {
				giHomeLED_Delay_Ticks = 100; 
			}
		}
		

		platform_device_register(&ntx_gpio_key_device);
	}

	imx6q_add_busfreq();
	imx6sl_add_dcp();
	imx6sl_add_rngb();
	imx6sl_add_imx_pxp_v4l2();

	imx6q_add_perfmon(0);
	imx6q_add_perfmon(1);
	imx6q_add_perfmon(2);
	mxc_register_device(&mxcbl_device, NULL);


	platform_device_register(&ntx_light_ldm);
#ifndef CONFIG_ANDROID //[
	mxc_register_device(&mxc_usb_plug_device, &usbplug_data);
#endif//]CONFIG_ANDROID
	if (1==gptHWCFG->m_val.bPMIC) {
		// RC5T619 .
	}
	else {
		/* Register charger chips */
		platform_device_register(&ntx_charger);

	}

	if(gptHWCFG) {

		if(1==gptHWCFG->m_val.bHallSensor) {
			// hall sensor enabled .
			tle4913_init();
		}
#ifdef CONFIG_SND_SOC_ALC5640//[
		if( NTXHWCFG_TST_FLAG(gptHWCFG->m_val.bPCB_Flags,6) )
		{
			// headphone detector enabled .
			headphone_detect_init();
		}
#endif //]CONFIG_SND_SOC_ALC5640

	}
	else {
		printk(KERN_ERR "missing ntx hwconfig !!\n");
	}

}

extern void __iomem *twd_base;
static void __init mx6_timer_init(void)
{
	struct clk *uart_clk;
#ifdef CONFIG_LOCAL_TIMERS
	twd_base = ioremap(LOCAL_TWD_ADDR, SZ_256);
	BUG_ON(!twd_base);
#endif
	mx6sl_clocks_init(32768, 24000000, 0, 0);

	uart_clk = clk_get_sys("imx-uart.0", NULL);
	early_console_setup(UART1_BASE_ADDR, uart_clk);
}

static struct sys_timer mxc_timer = {
	.init   = mx6_timer_init,
};

static void __init mx6_ntx_reserve(void)
{
#if defined(CONFIG_MXC_GPU_VIV) || defined(CONFIG_MXC_GPU_VIV_MODULE)
	phys_addr_t phys;

	if (imx6q_gpu_pdata.reserved_mem_size) {
		phys = memblock_alloc_base(imx6q_gpu_pdata.reserved_mem_size,
					   SZ_4K, MEMBLOCK_ALLOC_ACCESSIBLE);
		memblock_remove(phys, imx6q_gpu_pdata.reserved_mem_size);
		imx6q_gpu_pdata.reserved_mem_base = phys;
	}
#endif
}

static int __init display_panel_setup(char *options)
{
	if (!options || !*options) {
		pr_err("Error panel options\n");
		return 0;
	}

	if (!strcmp(options, "lcd"))
		display_panel_mode = PANEL_MODE_LCD;
	else if (!strcmp(options, "hdmi"))
		display_panel_mode = PANEL_MODE_HDMI;
	else if (!strcmp(options, "eink"))
		display_panel_mode = PANEL_MODE_EINK;
	else
		pr_warn("WARN: invalid display panel mode setting");

	return 1;
}

__setup("panel=", display_panel_setup);

MACHINE_START(MX6SL_NTX, "Freescale i.MX 6SoloLite NTX Board")
	.boot_params	= MX6SL_PHYS_OFFSET + 0x100,
	.map_io		= mx6_map_io,
	.init_irq	= mx6_init_irq,
	.init_machine	= mx6_ntx_init,
	.timer		= &mxc_timer,
	.reserve	= mx6_ntx_reserve,
MACHINE_END
