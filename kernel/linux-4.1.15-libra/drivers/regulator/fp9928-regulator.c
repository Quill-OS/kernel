/*
 * Copyright (C) 2010 Freescale Semiconductor, Inc. All Rights Reserved.
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
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>
#include <linux/mfd/fp9928.h>
#include <linux/gpio.h>
#include <linux/pmic_status.h>
#include <linux/of_gpio.h>

struct fp9928_data {
	int num_regulators;
	struct fp9928 *fp9928;
	struct regulator_dev **rdev;
};


static long unsigned int fp9928_pass_num = { 1 };
static int fp9928_vcom = { -2500000 };

static int fp9928_is_power_good(struct fp9928 *fp9928);

static ssize_t fp9928_powerall_read(struct device *dev, struct device_attribute *attr,char *buf)
{
	struct fp9928 *fp9928 = dev_get_drvdata(dev);
	//struct fp9928_platform_data *pdata = fp9928->pdata;

	sprintf(buf,"%d\n",(int)gpio_get_value(fp9928->gpio_pmic_pwrall));
	return strlen(buf);
}
static ssize_t fp9928_powerall_write(struct device *dev, struct device_attribute *attr,
		       const char *buf, size_t count)
{
	struct fp9928 *fp9928 = dev_get_drvdata(dev);
	//struct fp9928_platform_data *pdata = fp9928->pdata;
	int iVal=-1;

	sscanf(buf,"%d\n",&iVal);
	if(1==iVal) {
		gpio_set_value(fp9928->gpio_pmic_pwrall,1);
	}
	else if(0==iVal) {
		gpio_set_value(fp9928->gpio_pmic_pwrall,0);
	}
	else {
		printk(KERN_ERR"%s() invalid parameter !!it must be [1|0] \n",__FUNCTION__);
	}
	return count;
}

static DEVICE_ATTR (powerall,0644,fp9928_powerall_read,fp9928_powerall_write);

static ssize_t fp9928_powerup_read(struct device *dev, struct device_attribute *attr,char *buf)
{
	struct fp9928 *fp9928 = dev_get_drvdata(dev);
	//struct fp9928_platform_data *pdata = fp9928->pdata;

	sprintf(buf,"%d\n",(int)gpio_get_value(fp9928->gpio_pmic_powerup));
	return strlen(buf);
}
static ssize_t fp9928_powerup_write(struct device *dev, struct device_attribute *attr,
		       const char *buf, size_t count)
{
	struct fp9928 *fp9928 = dev_get_drvdata(dev);
	//struct fp9928_platform_data *pdata = fp9928->pdata;
	int iVal=-1;

	sscanf(buf,"%d\n",&iVal);
	if(1==iVal) {
		gpio_set_value(fp9928->gpio_pmic_powerup,1);
	}
	else if(0==iVal) {
		gpio_set_value(fp9928->gpio_pmic_powerup,0);
	}
	else {
		printk(KERN_ERR"%s() invalid parameter !!it must be [1|0] \n",__FUNCTION__);
	}
	return count;
}

static DEVICE_ATTR (powerup,0644,fp9928_powerup_read,fp9928_powerup_write);

static ssize_t fp9928_vcom_read(struct device *dev, struct device_attribute *attr,char *buf)
{
	struct fp9928 *fp9928 = dev_get_drvdata(dev);
	//struct fp9928_platform_data *pdata = fp9928->pdata;

	sprintf(buf,"%d\n",(int)gpio_get_value(fp9928->gpio_pmic_vcom_ctrl));
	return strlen(buf);
}
static ssize_t fp9928_vcom_write(struct device *dev, struct device_attribute *attr,
		       const char *buf, size_t count)
{
	struct fp9928 *fp9928 = dev_get_drvdata(dev);
	//struct fp9928_platform_data *pdata = fp9928->pdata;
	int iVal=-1;

	sscanf(buf,"%d\n",&iVal);
	if(1==iVal) {
		gpio_set_value(fp9928->gpio_pmic_vcom_ctrl,1);
	}
	else if(0==iVal) {
		gpio_set_value(fp9928->gpio_pmic_vcom_ctrl,0);
	}
	else {
		printk(KERN_ERR"%s() invalid parameter !!it must be [1|0] \n",__FUNCTION__);
	}
	return count;
}

static DEVICE_ATTR (vcom,0644,fp9928_vcom_read,fp9928_vcom_write);

/*
 * Regulator operations
 */
/* Convert uV to the VCOM register bitfield setting */

static int vcom_rs_to_uV(unsigned int reg_setting)
{
	if (reg_setting <= FP9928_VCOM_MIN_SET)
		return FP9928_VCOM_MIN_uV;
	if (reg_setting >= FP9928_VCOM_MAX_SET)
		return FP9928_VCOM_MAX_uV;
	return -(reg_setting * FP9928_VCOM_STEP_uV);
}


static int vcom_uV_to_rs(int uV)
{
	if (uV <= FP9928_VCOM_MIN_uV)
		return FP9928_VCOM_MIN_SET;
	if (uV >= FP9928_VCOM_MAX_uV)
		return FP9928_VCOM_MAX_SET;
	return (-uV) / FP9928_VCOM_STEP_uV;
}

static int _fp9928_vcom_set_voltage(struct fp9928 *fp9928,
					int minuV, int uV, unsigned *selector)
{
	int retval;

	retval = fp9928_reg_write(fp9928,REG_FP9928_VCOM,
			vcom_uV_to_rs(uV) & 255);

	return retval;

}

static int fp9928_vcom_set_voltage(struct regulator_dev *reg,
					int minuV, int uV, unsigned *selector)
{
	struct fp9928 *fp9928 = rdev_get_drvdata(reg);

	return _fp9928_vcom_set_voltage(fp9928,minuV,uV,selector);
}

static int fp9928_vcom_get_voltage(struct regulator_dev *reg)
{
	struct fp9928 *fp9928 = rdev_get_drvdata(reg);
	unsigned int cur_reg_val; /* current register value */
	int vcomValue;

	fp9928_reg_read(fp9928,REG_FP9928_VCOM,&cur_reg_val);
	vcomValue = vcom_rs_to_uV(cur_reg_val);
	printk("%s() : vcom=%duV\n",__FUNCTION__,vcomValue);

	return vcomValue;

}

static int fp9928_vcom_enable(struct regulator_dev *reg)
{
	struct fp9928 *fp9928 = rdev_get_drvdata(reg);

	/*
	 * Check to see if we need to set the VCOM voltage.
	 * Should only be done one time. And, we can
	 * only change vcom voltage if we have been enabled.
	 */
	if (!fp9928->vcom_setup && fp9928_is_power_good(fp9928)) 
	{
		fp9928_vcom_set_voltage(reg,
			fp9928->vcom_uV,
			fp9928->vcom_uV,
			NULL);
		fp9928->vcom_setup = true;
	}
	gpio_set_value(fp9928->gpio_pmic_vcom_ctrl,1);

	return 0;
}

static int fp9928_vcom_disable(struct regulator_dev *reg)
{
	struct fp9928 *fp9928 = rdev_get_drvdata(reg);

	gpio_set_value(fp9928->gpio_pmic_vcom_ctrl,0);
	return 0;
}

static int fp9928_vcom_is_enabled(struct regulator_dev *reg)
{
	struct fp9928 *fp9928 = rdev_get_drvdata(reg);

	int gpio = gpio_get_value(fp9928->gpio_pmic_vcom_ctrl);
	if (gpio == 0)
		return 0;
	else
		return 1;
}


static int fp9928_is_power_good(struct fp9928 *fp9928)
{
	/*
	 * XOR of polarity (starting value) and current
	 * value yields whether power is good.
	 */
	return gpio_get_value(fp9928->gpio_pmic_pwrgood) ^
		fp9928->pwrgood_polarity;
}

static int fp9928_wait_power_good(struct fp9928 *fp9928)
{
	int i;

	msleep(1);
	for (i = 0; i < fp9928->max_wait * 3; i++) 
	//for (i = 0; i < fp9928->max_wait * 300; i++) 
	{
		if (fp9928_is_power_good(fp9928)) {
			printk("%s():cnt=%d,PG=%d\n",__FUNCTION__,i,gpio_get_value(fp9928->gpio_pmic_pwrgood));
			return 0;
		}

		msleep(1);
	}
	printk(KERN_ERR"%s():waiting(%d) for PG(%d) timeout\n",__FUNCTION__,i,gpio_get_value(fp9928->gpio_pmic_pwrgood));
	return -ETIMEDOUT;
}

static int fp9928_display_enable(struct regulator_dev *reg)
{
	struct fp9928 *fp9928 = rdev_get_drvdata(reg);

	gpio_set_value(fp9928->gpio_pmic_powerup,1);
	return fp9928_wait_power_good(fp9928);
}

static int fp9928_display_disable(struct regulator_dev *reg)
{
#if 0
	printk(KERN_ERR"%s():skipped !!\n",__FUNCTION__);
	return 0;
#else
	struct fp9928 *fp9928 = rdev_get_drvdata(reg);

	/* turn off display regulators */
	gpio_set_value(fp9928->gpio_pmic_powerup,0);
	return 0;
#endif
}

static int fp9928_display_is_enabled(struct regulator_dev *reg)
{
	struct fp9928 *fp9928 = rdev_get_drvdata(reg);

	if (gpio_is_valid(fp9928->gpio_pmic_powerup)) {
		return gpio_get_value(fp9928->gpio_pmic_powerup)?1:0;
	}
	return 0;
}

static int fp9928_tmst_get_temperature(struct regulator_dev *reg)
{
	struct fp9928 *fp9928 = rdev_get_drvdata(reg);
	const int iDefaultTemp = 25;
	int iTemperature = iDefaultTemp;

	if(fp9928_get_temperature(fp9928,&iTemperature)<0) { 
		iTemperature = iDefaultTemp;
	}

	return iTemperature;
}

/*
 * Regulator operations
 */

static struct regulator_ops fp9928_display_ops = {
	.enable = fp9928_display_enable,
	.disable = fp9928_display_disable,
	.is_enabled = fp9928_display_is_enabled,
};

static struct regulator_ops fp9928_vcom_ops = {
	.enable = fp9928_vcom_enable,
	.disable = fp9928_vcom_disable,
	.get_voltage = fp9928_vcom_get_voltage,
	.set_voltage = fp9928_vcom_set_voltage,
	.is_enabled = fp9928_vcom_is_enabled,
};

static struct regulator_ops fp9928_v3p3_ops = {
};

static struct regulator_ops fp9928_tmst_ops = {
	.get_voltage = fp9928_tmst_get_temperature,
};

/*
 * Regulator descriptors
 */
static struct regulator_desc fp9928_reg[FP9928_NUM_REGULATORS] = {
{
	.name = "DISPLAY",
	.id = FP9928_DISPLAY,
	.ops = &fp9928_display_ops,
	.type = REGULATOR_VOLTAGE,
	.owner = THIS_MODULE,
},
{
	.name = "VCOM",
	.id = FP9928_VCOM,
	.ops = &fp9928_vcom_ops,
	.type = REGULATOR_VOLTAGE,
	.owner = THIS_MODULE,
},
{
	.name = "V3P3",
	.id = FP9928_V3P3,
	.ops = &fp9928_v3p3_ops,
	.type = REGULATOR_VOLTAGE,
	.owner = THIS_MODULE,
},
{
	.name = "TMST",
	.id = FP9928_TMST,
	.ops = &fp9928_tmst_ops,
	.type = REGULATOR_VOLTAGE,
	.owner = THIS_MODULE,
},
};

#define CHECK_PROPERTY_ERROR_KFREE(prop) \
do { \
	int ret = of_property_read_u32(fp9928->dev->of_node, \
					#prop, &fp9928->prop); \
	if (ret < 0) { \
		return ret;	\
	}	\
} while (0);

#ifdef CONFIG_OF
static int fp9928_pmic_dt_parse_pdata(struct platform_device *pdev,
					struct fp9928_platform_data *pdata)
{
	struct fp9928 *fp9928 = dev_get_drvdata(pdev->dev.parent);
	struct device_node *pmic_np, *regulators_np, *reg_np;
	struct fp9928_regulator_data *rdata;
	int i, ret;

	pmic_np = of_node_get(fp9928->dev->of_node);
	if (!pmic_np) {
		dev_err(&pdev->dev, "could not find pmic sub-node\n");
		return -ENODEV;
	}

	regulators_np = of_find_node_by_name(pmic_np, "regulators");
	if (!regulators_np) {
		dev_err(&pdev->dev, "could not find regulators sub-node\n");
		return -EINVAL;
	}

	pdata->num_regulators = of_get_child_count(regulators_np);
	dev_info(&pdev->dev, "num_regulators %d\n", pdata->num_regulators);

	rdata = devm_kzalloc(&pdev->dev, sizeof(*rdata) *
				pdata->num_regulators, GFP_KERNEL);
	if (!rdata) {
		of_node_put(regulators_np);
		dev_err(&pdev->dev, "could not allocate memory for"
			"regulator data\n");
		return -ENOMEM;
	}

	pdata->regulators = rdata;
	for_each_child_of_node(regulators_np, reg_np) {
		for (i = 0; i < ARRAY_SIZE(fp9928_reg); i++)
			if (!of_node_cmp(reg_np->name, fp9928_reg[i].name))
				break;

		if (i == ARRAY_SIZE(fp9928_reg)) {
			dev_warn(&pdev->dev, "don't know how to configure"
				"regulator %s\n", reg_np->name);
			continue;
		}

		rdata->id = i;
		rdata->initdata = of_get_regulator_init_data(&pdev->dev,
							     reg_np,
							     &fp9928_reg[i]);
		rdata->reg_node = reg_np;
		rdata++;
	}
	of_node_put(regulators_np);

	fp9928->gpio_pmic_vcom_ctrl = of_get_named_gpio(pmic_np,
					"gpio_pmic_vcom_ctrl", 0);
	if (!gpio_is_valid(fp9928->gpio_pmic_vcom_ctrl)) {
		dev_err(&pdev->dev, "no epdc pmic vcom_ctrl pin available\n");
		goto err;
	}
	ret = devm_gpio_request_one(&pdev->dev, fp9928->gpio_pmic_vcom_ctrl,
				GPIOF_OUT_INIT_LOW, "epdc-vcom");
	if (ret < 0) {
		dev_err(&pdev->dev, "request vcom gpio failed (%d)!\n",ret);
		//goto err;
	}
	else {
		printk("%s():gpio_pmic_vcom_ctrl=%d\n",__FUNCTION__,fp9928->gpio_pmic_vcom_ctrl);
	}

	fp9928->gpio_pmic_powerup = of_get_named_gpio(pmic_np,
					"gpio_pmic_powerup", 0);
	if (!gpio_is_valid(fp9928->gpio_pmic_powerup)) {
		dev_err(&pdev->dev, "no epdc pmic powerup pin available\n");
		goto err;
	}
	else {
		printk("%s():gpio_pmic_powerup=%d\n",__FUNCTION__,fp9928->gpio_pmic_powerup);
	}
#if 0
	ret = devm_gpio_request_one(&pdev->dev, fp9928->gpio_pmic_powerup,
				GPIOF_IN, "epdc-powerup");
#else 
	ret = devm_gpio_request_one(&pdev->dev, fp9928->gpio_pmic_powerup,
				GPIOF_OUT_INIT_LOW, "epdc-powerup");
#endif
	if (ret < 0) {
		dev_err(&pdev->dev, "request powerup gpio failed (%d)!\n",ret);
		//goto err;
	}

	fp9928->gpio_pmic_pwrgood = of_get_named_gpio(pmic_np,
					"gpio_pmic_pwrgood", 0);
	if (!gpio_is_valid(fp9928->gpio_pmic_pwrgood)) {
		dev_err(&pdev->dev, "no epdc pmic pwrgood pin available\n");
		goto err;
	}
	ret = devm_gpio_request_one(&pdev->dev, fp9928->gpio_pmic_pwrgood,
				GPIOF_IN, "epdc-pwrstat");
	if (ret < 0) {
		dev_err(&pdev->dev, "request pwrstat gpio failed (%d)!\n",ret);
		//goto err;
	}
	else {
		printk("%s():gpio_pmic_pwrgood=%d\n",__FUNCTION__,fp9928->gpio_pmic_pwrgood);
	}

err:
	return 0;

}
#else
static int fp9928_pmic_dt_parse_pdata(struct platform_device *pdev,
					struct fp9928 *fp9928)
{
	return 0;
}
#endif	/* !CONFIG_OF */

/*
 * Regulator init/probing/exit functions
 */
static int fp9928_regulator_probe(struct platform_device *pdev)
{
	struct fp9928 *fp9928 = dev_get_drvdata(pdev->dev.parent);
	struct fp9928_platform_data *pdata = fp9928->pdata;
	struct fp9928_data *priv;
	struct regulator_dev **rdev;
	struct regulator_config config = { };
	int size, i, ret = 0;

    printk("fp9928_regulator_probe starting , of_node=%p, pdata=%p\n",
				fp9928->dev->of_node, pdata);

	//fp9928->pwrgood_polarity = 1;
	if (fp9928->dev->of_node) {	
		ret = fp9928_pmic_dt_parse_pdata(pdev, pdata);
		if (ret)
			return ret;
	}

	priv = devm_kzalloc(&pdev->dev, sizeof(struct fp9928_data),
			       GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	size = sizeof(struct regulator_dev *) * pdata->num_regulators;
	priv->rdev = devm_kzalloc(&pdev->dev, size, GFP_KERNEL);
	if (!priv->rdev)
		return -ENOMEM;

	rdev = priv->rdev;
	priv->num_regulators = pdata->num_regulators;
	platform_set_drvdata(pdev, priv);

	fp9928->vcom_setup = false;
	fp9928->vcom_uV = fp9928_vcom;
	fp9928->max_wait = 30;

	if (!fp9928->vcom_setup) 
	{
		_fp9928_vcom_set_voltage(fp9928,
			fp9928->vcom_uV,
			fp9928->vcom_uV,
			NULL);
		fp9928->vcom_setup = true;
	}

	for (i = 0; i < pdata->num_regulators; i++) {
		int id = pdata->regulators[i].id;

		config.dev = fp9928->dev;
		config.init_data = pdata->regulators[i].initdata;
		config.driver_data = fp9928;
		config.of_node = pdata->regulators[i].reg_node;

		rdev[i] = regulator_register(&fp9928_reg[id], &config);
		if (IS_ERR(rdev[i])) {
			ret = PTR_ERR(rdev[i]);
			dev_err(&pdev->dev, "regulator init failed for %d\n",
					id);
			rdev[i] = NULL;
			goto err;
		}
	}

#if 1
	ret = device_create_file(fp9928->dev, &dev_attr_powerup);
	if (ret < 0) {
		dev_err(fp9928->dev, "fail : fp9928 powerup create.\n");
		return ret;
	}

	ret = device_create_file(fp9928->dev, &dev_attr_powerall);
	if (ret < 0) {
		dev_err(fp9928->dev, "fail : fp9928 powerall create.\n");
		return ret;
	}

	ret = device_create_file(fp9928->dev, &dev_attr_vcom);
	if (ret < 0) {
		dev_err(fp9928->dev, "fail : fp9928 vcom create.\n");
		return ret;
	}
#endif

    printk("fp9928_regulator_probe success\n");
	return 0;
err:
	while (--i >= 0)
		regulator_unregister(rdev[i]);
	return ret;
}

static int fp9928_regulator_remove(struct platform_device *pdev)
{
	struct fp9928_data *priv = platform_get_drvdata(pdev);
	struct regulator_dev **rdev = priv->rdev;
	int i;

	for (i = 0; i < priv->num_regulators; i++)
		regulator_unregister(rdev[i]);
	return 0;
}

static const struct platform_device_id fp9928_pmic_id[] = {
	{ "fp9928-pmic", 0},
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(platform, fp9928_pmic_id);

static struct platform_driver fp9928_regulator_driver = {
	.probe = fp9928_regulator_probe,
	.remove = fp9928_regulator_remove,
	.id_table = fp9928_pmic_id,
	.driver = {
		.name = "fp9928-pmic",
	},
};

static int __init fp9928_regulator_init(void)
{
	return platform_driver_register(&fp9928_regulator_driver);
}
subsys_initcall_sync(fp9928_regulator_init);

static void __exit fp9928_regulator_exit(void)
{
	platform_driver_unregister(&fp9928_regulator_driver);
}
module_exit(fp9928_regulator_exit);


/*
 * Parse user specified options (`fp9928:')
 * example:
 *   fp9928:pass=2,vcom=-1250000
 */
static int __init fp9928_setup(char *options)
{
	int ret;
	char *opt;
	unsigned long ulResult;
	while ((opt = strsep(&options, ",")) != NULL) {
		if (!*opt)
			continue;
		if (!strncmp(opt, "pass=", 5)) {
			ret = kstrtoul(opt + 5, 0, &fp9928_pass_num);
			if (ret < 0)
				return ret;
		}
		if (!strncmp(opt, "vcom=", 5)) {
			int offs = 5;
			if (opt[5] == '-')
				offs = 6;
			ret = kstrtoul((const char *)(opt + offs), 0, &ulResult);
			fp9928_vcom = (int) ulResult;
			if (ret < 0)
				return ret;
			fp9928_vcom = -fp9928_vcom;
		}
	}

	return 1;
}

__setup("fp9928:", fp9928_setup);

/* Module information */
MODULE_DESCRIPTION("FP9928 regulator driver");
MODULE_LICENSE("GPL");
