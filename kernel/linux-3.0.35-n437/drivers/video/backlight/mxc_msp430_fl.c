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

/* workaround for atlas hot issue */
#define MXC_MAX_INTENSITY 	100
#define MXC_DEFAULT_INTENSITY 	0

#define MXC_INTENSITY_OFF 	0

struct mxcfl_dev_data {
	int intensity;
	int suspend;
};

extern int fl_set_percentage(int iFL_Percentage);
extern int FL_suspend(void);
static int mxcfl_set_intensity(struct backlight_device *bd)
{
	int brightness = bd->props.brightness;
	struct mxcfl_dev_data *devdata = dev_get_drvdata(&bd->dev);

	fl_set_percentage (brightness);
	devdata->intensity = brightness;

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
