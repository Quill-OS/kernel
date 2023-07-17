/*
 * Copyright (C) 2010 Freescale Semiconductor, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */
/*
 * tps65185.c
 *
 * Based on the MAX1619 driver.
 * Copyright (C) 2003-2004 Alexey Fisher <fishor@mail.ru>
 *                         Jean Delvare <khali@linux-fr.org>
 *
 * The FP9928 is a sensor chip made by Texass Instruments.
 * It reports up to two temperatures (its own plus up to
 * one external one).
 */


#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/jiffies.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/sysfs.h>
#include <linux/platform_device.h>
#include <linux/mfd/fp9928.h>

/*
 * Conversions
 */
static int temp_from_reg(int val)
{
	return val;
}

/*
 * Functions declaration
 */
static int fp9928_sensor_probe(struct platform_device *pdev);
static int fp9928_sensor_remove(struct platform_device *pdev);

static const struct platform_device_id fp9928_sns_id[] = {
	{ "fp9928-sns", 0},
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(platform, fp9928_sns_id);

/*
 * Driver data (common to all clients)
 */
static struct platform_driver fp9928_sensor_driver = {
	.probe = fp9928_sensor_probe,
	.remove = fp9928_sensor_remove,
	.id_table = fp9928_sns_id,
	.driver = {
		.name = "fp9928_sensor",
	},
};

/*
 * Client data (each client gets its own)
 */
struct fp9928_data {
	struct device *hwmon_dev;
	struct fp9928 *fp9928;
};

int fp9928_get_temperature(struct fp9928 *fp9928,int *O_piTemperature)
{
	int iTemp;
	unsigned int reg_val;

	fp9928_reg_read(fp9928,REG_FP9928_TMST_VAL, &reg_val);
	iTemp = temp_from_reg(reg_val);

	if(O_piTemperature) {
		printk("%s():temperature = %d\n",__FUNCTION__,iTemp);
		*O_piTemperature = iTemp;
	}
	
	return 0;
}

/*
 * Sysfs stuff
 */
static ssize_t show_temp_input(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct fp9928_data *data = dev_get_drvdata(dev);
	int iTemp ;

	if(0==fp9928_get_temperature(data->fp9928,&iTemp)) {
		return snprintf(buf, PAGE_SIZE, "%d\n", iTemp);
	}
	else {
		return snprintf(buf, PAGE_SIZE, "?\n");
	}
}

int fp9928_set_vcom(struct fp9928 *fp9928,int iVCOMmV,int iIsWriteToFlash)
{

	if(!fp9928) {
		return -1;
	}

	if(iVCOMmV>0) {
		return -2;
	}

	{
		unsigned char bReg;
		int i10uV_Steps;
		int i10uV_Steps_mod10;
		int iVCOM_mV_ABS;

		int iOldPowerState ;


		if(iVCOMmV<0) {
			iVCOM_mV_ABS = -iVCOMmV;
		}
		else {
			iVCOM_mV_ABS = iVCOMmV;
		}

		i10uV_Steps = (int)(iVCOM_mV_ABS*10000/FP9928_VCOM_STEP_uV);
		i10uV_Steps_mod10 = (int)(i10uV_Steps%10);
		if(i10uV_Steps_mod10>=5) {
			bReg = (unsigned char) ((i10uV_Steps/10)+1);
		}
		else {
			bReg = (unsigned char) (i10uV_Steps/10);
		}
		printk("%s():want set VCOM %dmV,reg=0x%02X,output %dmV\n",\
				__FUNCTION__,iVCOMmV,bReg,bReg*FP9928_VCOM_STEP_uV/1000);
    fp9928_reg_write(fp9928,REG_FP9928_VCOM, bReg);
	}

	return 0;
}

int fp9928_get_vcom(struct fp9928 *fp9928,int *O_piVCOMmV)
{
	unsigned int vcom_reg_val;

	if(!fp9928) {
		return -1;
	}

    fp9928_reg_read(fp9928,REG_FP9928_VCOM, &vcom_reg_val);

	if(O_piVCOMmV) {
		*O_piVCOMmV = -vcom_reg_val*FP9928_VCOM_STEP_uV;
		*O_piVCOMmV /= 1000;
	}
	
	return 0;
}

static ssize_t show_vcom(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct fp9928_data *data = dev_get_drvdata(dev);
	int iVCOM_mV;

	fp9928_get_vcom(data->fp9928,&iVCOM_mV);
	return snprintf(buf, PAGE_SIZE, "%dmV\n",iVCOM_mV);
}

static ssize_t set_vcom(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	long vcom_reg_val = simple_strtol(buf,NULL,10);
	struct fp9928_data *data = dev_get_drvdata(dev);

	fp9928_set_vcom(data->fp9928,-vcom_reg_val,0);
	return count;
}


static DEVICE_ATTR(temp_input, S_IRUGO, show_temp_input, NULL);
static DEVICE_ATTR(vcom_value, S_IWUSR | S_IRUGO, show_vcom, set_vcom);

static struct attribute *fp9928_attributes[] = {
	&dev_attr_temp_input.attr,
	&dev_attr_vcom_value.attr,
	NULL
};

static const struct attribute_group fp9928_group = {
	.attrs = fp9928_attributes,
};

/*
 * Real code
 */
static int fp9928_sensor_probe(struct platform_device *pdev)
{
	struct fp9928_data *data;
	int err;
    printk("fp9928_sensor_probe starting\n");

	data = kzalloc(sizeof(struct fp9928_data), GFP_KERNEL);
	if (!data) {
		err = -ENOMEM;
		goto exit;
	}
	data->fp9928 = dev_get_drvdata(pdev->dev.parent);
	/* Register sysfs hooks */
	err = sysfs_create_group(&pdev->dev.kobj, &fp9928_group);
	if (err)
		goto exit_free;

	data->hwmon_dev = hwmon_device_register(&pdev->dev);
	if (IS_ERR(data->hwmon_dev)) {
		err = PTR_ERR(data->hwmon_dev);
		goto exit_remove_files;
	}

	platform_set_drvdata(pdev, data);

    printk("fp9928_sensor_probe success\n");
	return 0;

exit_remove_files:
	sysfs_remove_group(&pdev->dev.kobj, &fp9928_group);
exit_free:
	kfree(data);
exit:
	return err;
}

static int fp9928_sensor_remove(struct platform_device *pdev)
{
	struct fp9928_data *data = platform_get_drvdata(pdev);

	hwmon_device_unregister(data->hwmon_dev);
	sysfs_remove_group(&pdev->dev.kobj, &fp9928_group);

	kfree(data);
	return 0;
}

static int __init sensors_fp9928_init(void)
{
	return platform_driver_register(&fp9928_sensor_driver);
}
module_init(sensors_fp9928_init);

static void __exit sensors_fp9928_exit(void)
{
	platform_driver_unregister(&fp9928_sensor_driver);
}
module_exit(sensors_fp9928_exit);

MODULE_DESCRIPTION("FP9928 sensor driver");
MODULE_LICENSE("GPL");
