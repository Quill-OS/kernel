/*
 * Copyright 2008-2011 Freescale Semiconductor, Inc. All Rights Reserved.
 */

/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/fb.h>
#include <linux/backlight.h>

#include <linux/pmic_light.h>
#include <linux/pmic_external.h>

#include "../../../arch/arm/mach-mx6/ntx_hwconfig.h"
#include "../../../drivers/misc/ntx-misc.h"
#include "../../../arch/arm/mach-mx6/board-mx6sl_ntx.h"

extern volatile NTX_HWCONFIG *gptHWCFG ;
/* workaround for atlas hot issue */
#define MXC_MAX_INTENSITY 	100
#define MXC_DEFAULT_INTENSITY 	0

#define MXC_INTENSITY_OFF 	0

struct mxcfl_dev_data {
	int intensity;
	int suspend;
};


extern int fl_lm3630a_percentageEx (int iChipIdx,int iFL_Percentage);
extern int fl_pwr_enable (unsigned short wColorFlags,int isEnable,int iDelayMS);
extern int fl_set_percentage(int iFL_Percentage);
extern int FL_suspend(void);
extern int fl_set_color_percentage(unsigned short wColorFlags,int iFL_percentage);
extern int fl_get_color_percentage(unsigned short wColorFlags);
static ssize_t wduty_show(struct device *dev, struct device_attribute *attr,char *buf)
{
	sprintf(buf,"%d\n",msp430_fl_get_duty(MSP430_FL_IDX_W));
	return strlen(buf);
}

static ssize_t wduty_store(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	int iDuty;
	sscanf(buf,"%d\n",&iDuty);
	fl_pwr_enable(FL_COLOR_FLAGS_W,1,100);
	msp430_fl_set_duty(MSP430_FL_IDX_W,iDuty);
	return strlen(buf);
}

static ssize_t rduty_show(struct device *dev, struct device_attribute *attr,char *buf)
{
	sprintf(buf,"%d\n",msp430_fl_get_duty(MSP430_FL_IDX_R));
	return strlen(buf);
}

static ssize_t rduty_store(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	int iDuty;
	sscanf(buf,"%d\n",&iDuty);
	fl_pwr_enable(FL_COLOR_FLAGS_R,1,100);
	msp430_fl_set_duty(MSP430_FL_IDX_R,iDuty);
	return strlen(buf);
}

static ssize_t wenable_show(struct device *dev, struct device_attribute *attr,char *buf)
{
	sprintf(buf,"%d\n",msp430_fl_is_enable(MSP430_FL_IDX_W));
	return strlen(buf);
}
static ssize_t wenable_store(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	int iEnable;
	sscanf(buf,"%d\n",&iEnable);
	fl_pwr_enable(FL_COLOR_FLAGS_W,1,100);
	msp430_fl_enable(MSP430_FL_IDX_W,iEnable);
	return strlen(buf);
}
static ssize_t renable_show(struct device *dev, struct device_attribute *attr,char *buf)
{
	sprintf(buf,"%d\n",msp430_fl_is_enable(MSP430_FL_IDX_R));
	return strlen(buf);
}
static ssize_t renable_store(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	int iEnable;
	sscanf(buf,"%d\n",&iEnable);
	fl_pwr_enable(FL_COLOR_FLAGS_R,1,100);
	msp430_fl_enable(MSP430_FL_IDX_R,iEnable);
	return strlen(buf);
}

static ssize_t wfreq_show(struct device *dev, struct device_attribute *attr,char *buf)
{
	int iMSP430_PWM_DivFreq = msp430_fl_get_freq(MSP430_FL_IDX_W);
	int iMSP430_MAX_Freq = msp430_fl_get_freq_max(MSP430_FL_IDX_W);

	sprintf(buf,"%d\n",iMSP430_MAX_Freq/iMSP430_PWM_DivFreq);
	return strlen(buf);
}

static ssize_t wfreq_store(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	int iMSP430_MAX_Freq = msp430_fl_get_freq_max(MSP430_FL_IDX_W);
	int iFreq;
	sscanf(buf,"%d\n",&iFreq);
	fl_pwr_enable(FL_COLOR_FLAGS_W,1,100);
	msp430_fl_set_freq(MSP430_FL_IDX_W,iMSP430_MAX_Freq/iFreq);
	return strlen(buf);
}

static ssize_t wfreq_max_show(struct device *dev, struct device_attribute *attr,char *buf)
{
	int iMSP430_MAX_Freq = msp430_fl_get_freq_max(MSP430_FL_IDX_W);
	sprintf(buf,"%d\n",iMSP430_MAX_Freq);
	return strlen(buf);
}

static ssize_t wfreq_max_store(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	int iFreq;
	sscanf(buf,"%d\n",&iFreq);
	return strlen(buf);
}
static ssize_t rpercent_show(struct device *dev, struct device_attribute *attr,char *buf)
{
	sprintf(buf,"%d\n",fl_get_color_percentage(FL_COLOR_FLAGS_R));
	return strlen(buf);
}
static ssize_t rpercent_store(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	int iPercent;
	sscanf(buf,"%d\n",&iPercent);
	fl_set_color_percentage(FL_COLOR_FLAGS_R,iPercent);
	return strlen(buf);
}

static ssize_t gpercent_show(struct device *dev, struct device_attribute *attr,char *buf)
{
	sprintf(buf,"%d\n",fl_get_color_percentage(FL_COLOR_FLAGS_G));
	return strlen(buf);
}
static ssize_t gpercent_store(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	int iPercent;
	sscanf(buf,"%d\n",&iPercent);
	fl_set_color_percentage(FL_COLOR_FLAGS_G,iPercent);
	return strlen(buf);
}

static ssize_t bpercent_show(struct device *dev, struct device_attribute *attr,char *buf)
{
	sprintf(buf,"%d\n",fl_get_color_percentage(FL_COLOR_FLAGS_B));
	return strlen(buf);
}

static ssize_t bpercent_store(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	int iPercent;
	sscanf(buf,"%d\n",&iPercent);
	fl_set_color_percentage(FL_COLOR_FLAGS_B,iPercent);
	return strlen(buf);
}






static struct device_attribute dev_attr_wduty = \
__ATTR(wduty,0644,wduty_show,wduty_store);
static struct device_attribute dev_attr_rduty = \
__ATTR(rduty,0644,rduty_show,rduty_store);
static struct device_attribute dev_attr_wenable = \
__ATTR(wenable,0644,wenable_show,wenable_store);
static struct device_attribute dev_attr_renable = \
__ATTR(renable,0644,renable_show,renable_store);
static struct device_attribute dev_attr_freq = \
__ATTR(freq,0644,wfreq_show,wfreq_store);
static struct device_attribute dev_attr_freq_max = \
__ATTR(freq_max,0644,wfreq_max_show,wfreq_max_store);
static struct device_attribute dev_attr_rpercent = \
__ATTR(rpercent,0644,rpercent_show,rpercent_store);
static struct device_attribute dev_attr_gpercent = \
__ATTR(gpercent,0644,gpercent_show,gpercent_store);
static struct device_attribute dev_attr_bpercent = \
__ATTR(bpercent,0644,bpercent_show,bpercent_store);

extern unsigned int last_FL_duty;

static int mxcfl_set_intensity(struct backlight_device *bd)
{
	int brightness = bd->props.brightness;
	struct mxcfl_dev_data *devdata = dev_get_drvdata(&bd->dev);

	if(gptHWCFG->m_val.bFrontLight) {
		if (2==gptHWCFG->m_val.bFL_PWM||6==gptHWCFG->m_val.bFL_PWM||7==gptHWCFG->m_val.bFL_PWM)
			fl_lm3630a_percentageEx (0,brightness);
		else if(5==gptHWCFG->m_val.bFL_PWM)
			fl_lm3630a_percentageEx (1,brightness);
		else
			fl_set_percentage (brightness);
	}
	last_FL_duty = devdata->intensity = brightness;
	return 0;
}

static int mxcfl_get_intensity(struct backlight_device *bd)
{
	struct mxcfl_dev_data *devdata = dev_get_drvdata(&bd->dev);
	return devdata->intensity;
}

static int mxcfl_check_fb(struct backlight_device *bldev, struct fb_info *info)
{
	return 0;
}

static struct backlight_ops bl_ops;

static int __devinit mxcfl_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct backlight_device *bd;
	struct mxcfl_dev_data *devdata;
	struct backlight_properties props;
	pmic_version_t pmic_version;
	int rval;
	int iMSP430_FL_type=0; // 0:disable,1:1 colors,2:2 colors .
	int iLM3630_FL_type=0; // 0:disable,1:2 colors,2:4 colors .
	int iRGB_color=0;

	if(0==gptHWCFG->m_val.bFrontLight) {
		return -EIO;
	}

	devdata = kzalloc(sizeof(struct mxcfl_dev_data), GFP_KERNEL);
	if (!devdata)
		return -ENOMEM;

	bl_ops.check_fb = mxcfl_check_fb;
	bl_ops.get_brightness = mxcfl_get_intensity;
	bl_ops.update_status = mxcfl_set_intensity;
	memset(&props, 0, sizeof(struct backlight_properties));
	props.max_brightness = MXC_MAX_INTENSITY;
 	props.type = BACKLIGHT_RAW;
	bd = backlight_device_register(dev_name(&pdev->dev), &pdev->dev, devdata,
				       &bl_ops, &props);
	if (IS_ERR(bd)) {
		ret = PTR_ERR(bd);
		goto err0;
	}

	platform_set_drvdata(pdev, bd);
	bd->props.brightness = MXC_DEFAULT_INTENSITY;
	backlight_update_status(bd);
	switch(gptHWCFG->m_val.bFL_PWM) {
	case 0:
		// MSP430 .
		iMSP430_FL_type=1;
		iLM3630_FL_type=0;
		break;
	case 1:
		// HT68F20 .
		iMSP430_FL_type=0;
		iLM3630_FL_type=0;
		break;
	case 4:
		// MSP430+LM3630
		iMSP430_FL_type=2;
		iLM3630_FL_type=1;
		iRGB_color=1;
		break;
	case 5:
		// LM3630 x2
		iMSP430_FL_type=0;
		iLM3630_FL_type=2;
		iRGB_color=1;
		break;
	case 2:
	case 6:
	case 7:
		// LM3630 x1
		iMSP430_FL_type=0;
		iLM3630_FL_type=1;
		break;
	default:
		iMSP430_FL_type=0;
		iLM3630_FL_type=0;
		break;
	}
	if(iMSP430_FL_type) {
		rval = device_create_file(&pdev->dev, &dev_attr_wduty);
		if (rval < 0) {
			dev_err(&pdev->dev, "fail : msp430 bl white duty create.\n");
			return rval;
		}
		rval = device_create_file(&pdev->dev, &dev_attr_wenable);
		if (rval < 0) {
			dev_err(&pdev->dev, "fail : msp430 bl white enable create.\n");
			return rval;
		}
		rval = device_create_file(&pdev->dev, &dev_attr_freq);
		if (rval < 0) {
			dev_err(&pdev->dev, "fail : msp430 bl freq .\n");
			return rval;
		}
		rval = device_create_file(&pdev->dev, &dev_attr_freq_max);
		if (rval < 0) {
			dev_err(&pdev->dev, "fail : msp430 bl freq max .\n");
			return rval;
		}
	}

	if(2==iMSP430_FL_type) {
		// msp430 second pwm source .
		rval = device_create_file(&pdev->dev, &dev_attr_rduty);
		if (rval < 0) {
			dev_err(&pdev->dev, "fail : msp430 bl red duty create.\n");
			return rval;
		}
		rval = device_create_file(&pdev->dev, &dev_attr_renable);
		if (rval < 0) {
			dev_err(&pdev->dev, "fail : msp430 bl red enable create.\n");
			return rval;
		}
	}

	if(iRGB_color) {
		//RGB colors .
		rval = device_create_file(&pdev->dev, &dev_attr_rpercent);
		if (rval < 0) {
			dev_err(&pdev->dev, "fail : msp430 bl red percent create.\n");
			return rval;
		}		
		rval = device_create_file(&pdev->dev, &dev_attr_gpercent);
		if (rval < 0) {
			dev_err(&pdev->dev, "fail : msp430 bl green percent create.\n");
			return rval;
		}
		rval = device_create_file(&pdev->dev, &dev_attr_bpercent);
		if (rval < 0) {
			dev_err(&pdev->dev, "fail : msp430 bl blue percent create.\n");
			return rval;
		}
	}
	pr_debug("MSP430 frontlight probed successfully\n");
	return 0;

      err0:
	kfree(devdata);
	return ret;
}

static int mxcfl_remove(struct platform_device *pdev)
{
	struct backlight_device *bd = platform_get_drvdata(pdev);
	struct mxcfl_dev_data *devdata = dev_get_drvdata(&bd->dev);

	kfree(devdata);
	backlight_device_unregister(bd);
	return 0;
}

static int mxcfl_suspend(struct platform_device *pdev, pm_message_t state)
{
	return FL_suspend ();
}

static int mxcfl_resume(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver mxcfl_driver = {
	.probe = mxcfl_probe,
	.remove = mxcfl_remove,
	.suspend = mxcfl_suspend,
	.resume = mxcfl_resume,
	.driver = {
		   .name = "mxc_msp430_fl",
		   },
};

static int __init mxcfl_init(void)
{
	return platform_driver_register(&mxcfl_driver);
}

static void __exit mxcfl_exit(void)
{
	platform_driver_unregister(&mxcfl_driver);
}

module_init(mxcfl_init);
module_exit(mxcfl_exit);

MODULE_DESCRIPTION("MSP430 Frontlight Driver");
MODULE_AUTHOR("Netronix, Inc.");
MODULE_LICENSE("GPL");
