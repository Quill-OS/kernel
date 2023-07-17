// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2018 ROHM Semiconductors

#include <linux/gpio/driver.h>
#include <linux/mfd/rohm-bd71828.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>

#define GPIO_OUT_REG(off) (BD71828_REG_GPIO_CTRL1 + (off))
#define HALL_GPIO_OFFSET 3

struct bd71828 {
    struct device   *dev;
    struct regmap   *regmap;
};

struct bd71828_platform_data {
    int     gpio_base;
};

struct bd71828_gpio {
	struct gpio_chip gpio_chip;
	struct bd71828 *bd71828;
};

static inline struct bd71828_gpio *to_bd71828_gpio(struct gpio_chip *chip)
{
	return container_of(chip, struct bd71828_gpio, gpio_chip);
}

static void bd71828_gpio_set(struct gpio_chip *chip, unsigned int offset,
			     int value)
{
	struct bd71828_gpio *bd71828_gpio = to_bd71828_gpio(chip);
	u8 val = (value) ? BD71828_GPIO_OUT_HI : BD71828_GPIO_OUT_LO;
	int ret;

	/*
	 * The HALL input pin can only be used as input. If this is the pin
	 * we are dealing with - then we are done
	 */
	if (offset == HALL_GPIO_OFFSET)
		return;

	ret = regmap_update_bits(bd71828_gpio->bd71828->regmap, GPIO_OUT_REG(offset),
				 BD71828_GPIO_OUT_MASK, val);
	if (ret)
		dev_err(bd71828_gpio->bd71828->dev, "Could not set gpio to %d\n", value);
}

static int bd71828_gpio_get(struct gpio_chip *chip, unsigned int offset)
{
	struct bd71828_gpio *bd71828_gpio = to_bd71828_gpio(chip);
	uint8_t val = 0;
	int ret;

	if (offset == HALL_GPIO_OFFSET)
		ret = regmap_read(bd71828_gpio->bd71828->regmap, BD71828_REG_IO_STAT,
				  &val);
	else
		ret = regmap_read(bd71828_gpio->bd71828->regmap, GPIO_OUT_REG(offset),
				  &val);
	if (!ret)
		ret = (val & BD71828_GPIO_OUT_MASK);

	return ret;
}

/*static int bd71828_gpio_set_config(struct gpio_chip *chip, unsigned int offset,
				   unsigned long config)
{
	struct bd71828_gpio *bdgpio = gpiochip_get_data(chip);

	if (offset == HALL_GPIO_OFFSET)
		return -ENOTSUPP;

	switch (pinconf_to_config_param(config)) {
	case PIN_CONFIG_DRIVE_OPEN_DRAIN:
		return regmap_update_bits(bdgpio->regmap,
					  GPIO_OUT_REG(offset),
					  BD71828_GPIO_DRIVE_MASK,
					  BD71828_GPIO_OPEN_DRAIN);
	case PIN_CONFIG_DRIVE_PUSH_PULL:
		return regmap_update_bits(bdgpio->regmap,
					  GPIO_OUT_REG(offset),
					  BD71828_GPIO_DRIVE_MASK,
					  BD71828_GPIO_PUSH_PULL);
	default:
		break;
	}
	return -ENOTSUPP;
}*/

static int bd71828_get_direction(struct gpio_chip *chip, unsigned int offset)
{
	/*
	 * Pin usage is selected by OTP data. We can't read it runtime. Hence
	 * we trust that if the pin is not excluded by "gpio-reserved-ranges"
	 * the OTP configuration is set to OUT. (Other pins but HALL input pin
	 * on BD71828 can't really be used for general purpose input - input
	 * states are used for specific cases like regulator control or
	 * PMIC_ON_REQ.
	 */
	if (offset == HALL_GPIO_OFFSET)
		return GPIO_LINE_DIRECTION_IN;

	return GPIO_LINE_DIRECTION_OUT;
}

static int bd71828_probe(struct platform_device *pdev)
{
	struct bd71828 *bd71828 = dev_get_drvdata(pdev->dev.parent);
	struct bd71828_platform_data *pdata = dev_get_platdata(bd71828->dev);
	struct bd71828_gpio *bd71828_gpio;

	bd71828_gpio = devm_kzalloc(&pdev->dev, sizeof(*bd71828_gpio),
					GFP_KERNEL);
	if (!bd71828_gpio)
		return -ENOMEM;

	bd71828_gpio->gpio_chip.label = "gpio-bd71828",
	bd71828_gpio->gpio_chip.owner = THIS_MODULE,
	bd71828_gpio->gpio_chip.get_direction = bd71828_get_direction;
	//bdgpio->gpio.set_config = bd71828_gpio_set_config;
	bd71828_gpio->gpio_chip.set = bd71828_gpio_set,
	bd71828_gpio->gpio_chip.get = bd71828_gpio_get,
	bd71828_gpio->gpio_chip.can_sleep = true,
	bd71828_gpio->gpio_chip.dev = &pdev->dev;
	bd71828_gpio->gpio_chip.base = -1;
	bd71828_gpio->bd71828 = bd71828;

	if (pdata && pdata->gpio_base)
		bd71828_gpio->gpio_chip.base = pdata->gpio_base;

	platform_set_drvdata(pdev, bd71828_gpio);

	return gpiochip_add(&bd71828_gpio->gpio_chip);
}

static struct platform_driver bd71828_gpio = {
	.driver = {
		.name = "bd71828-gpio"
	},
	.probe = bd71828_probe,
};

//module_platform_driver(bd71828_gpio);

static int __init bd71828_gpio_init(void)
{
	return platform_driver_register(&bd71828_gpio);
}
subsys_initcall_sync(bd71828_gpio_init);

static void __exit bd71828_gpio_exit(void)
{
	platform_driver_unregister(&bd71828_gpio);
}
module_exit(bd71828_gpio_exit);

MODULE_AUTHOR("Matti Vaittinen <matti.vaittinen@fi.rohmeurope.com>");
MODULE_DESCRIPTION("BD71828 voltage regulator driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:bd71828-gpio");
