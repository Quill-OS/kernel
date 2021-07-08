/*
 * Driver for the IMX SNVS ON/OFF Power Key
 * Copyright (C) 2015 Freescale Semiconductor, Inc. All Rights Reserved.
 *
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#include <linux/device.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/mfd/syscon.h>


#define DEBOUNCE_TIME 30
#define REPEAT_INTERVAL 60

struct ntx_event0_drv_data {
	struct input_dev *input;
};

struct ntx_event0_drv_data *gptNTX_EVT0_drvdata;

struct input_dev * ntx_get_event0_inputdev(void)
{
	if(gptNTX_EVT0_drvdata) {
		return gptNTX_EVT0_drvdata->input;
	}
	else {
		return 0;
	}
}

void gpiokeys_report_event(unsigned int type, unsigned int code, int value)
{
	if (gptNTX_EVT0_drvdata) {
		input_event(gptNTX_EVT0_drvdata->input, type, code, value);
		input_sync(gptNTX_EVT0_drvdata->input);
	}
}

static int ntx_event0_probe(struct platform_device *pdev)
{
	struct ntx_event0_drv_data *pdata = NULL;
	struct input_dev *input = NULL;
	struct device_node *np;
	int error;

	/* Get SNVS register Page */
	np = pdev->dev.of_node;
	if (!np)
		return -ENODEV;

	pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;


	input = devm_input_allocate_device(&pdev->dev);
	if (!input) {
		dev_err(&pdev->dev, "failed to allocate the input device\n");
		return -ENOMEM;
	}

	input->name = pdev->name;
	input->phys = "ntx_key_event0/input0";
	input->id.bustype = BUS_HOST;

	//input_set_capability(input, EV_KEY, pdata->keycode);


	error = input_register_device(input);
	if (error < 0) {
		dev_err(&pdev->dev, "failed to register input device\n");
		input_free_device(input);
		return error;
	}

	pdata->input = input;
	platform_set_drvdata(pdev, pdata);

	gptNTX_EVT0_drvdata = pdata;

	return 0;
}

static int ntx_event0_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct ntx_event0_drv_data *pdata = platform_get_drvdata(pdev);


	return 0;
}

static int ntx_event0_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct ntx_event0_drv_data *pdata = platform_get_drvdata(pdev);


	return 0;
}

static const struct of_device_id ntx_event0_ids[] = {
	{ .compatible = "ntx,ntx_event0" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, ntx_event0_ids);

static SIMPLE_DEV_PM_OPS(ntx_event0_pm_ops, ntx_event0_suspend,
				ntx_event0_resume);

static struct platform_driver ntx_event0_driver = {
	.driver = {
		.name = "ntx_event0",
		.pm     = &ntx_event0_pm_ops,
		.of_match_table = ntx_event0_ids,
	},
	.probe = ntx_event0_probe,
};

module_platform_driver(ntx_event0_driver);

MODULE_AUTHOR("NetronixInc");
MODULE_DESCRIPTION("Netronix event0 key driver");
MODULE_LICENSE("GPL");

