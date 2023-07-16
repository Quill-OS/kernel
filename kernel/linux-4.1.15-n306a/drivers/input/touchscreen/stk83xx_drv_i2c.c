#include <linux/input.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/types.h>
#include <linux/pm.h>
#include "stk83xx.h"
#include "stk83xx_qualcomm.h"

#ifdef QCOM_SENSORS
	#include <linux/sensors.h>
#endif

#ifdef CONFIG_OF
static struct of_device_id stk83xx_match_table[] =
{
    { .compatible = "stk,stk83xx", },
    {}
};
#endif /* CONFIG_OF */

/*
 * @brief: Proble function for i2c_driver.
 *
 * @param[in] client: struct i2c_client *
 * @param[in] id: struct i2c_device_id *
 *
 * @return: Success or fail
 *          0: Success
 *          others: Fail
 */
static int stk83xx_i2c_probe(struct i2c_client *client,
                             const struct i2c_device_id *id)
{
	struct common_function common_fn =
	{
		.bops = &stk_i2c_bops,
		.tops = &stk_t_ops,
		.gops = &stk_g_ops,
		.sops = &stk_s_ops,
	};
	return stk_i2c_probe(client, &common_fn);
}

/*
 * @brief: Remove function for i2c_driver.
 *
 * @param[in] client: struct i2c_client *
 *
 * @return: 0
 */
static int stk83xx_i2c_remove(struct i2c_client *client)
{
    return stk_i2c_remove(client);
}

/**
 * @brief:
 */
static int stk83xx_i2c_detect(struct i2c_client *client, struct i2c_board_info *info)
{
    strcpy(info->type, STK83XX_NAME);
    return 0;
}

#ifdef CONFIG_PM_SLEEP
/*
 * @brief: Suspend function for dev_pm_ops.
 *
 * @param[in] dev: struct device *
 *
 * @return: 0
 */
static int stk83xx_i2c_suspend(struct device *dev)
{
    return stk83xx_suspend(dev);
}

/*
 * @brief: Resume function for dev_pm_ops.
 *
 * @param[in] dev: struct device *
 *
 * @return: 0
 */
static int stk83xx_i2c_resume(struct device *dev)
{
    return stk83xx_resume(dev);
}

static const struct dev_pm_ops stk83xx_pm_ops =
{
    .suspend = stk83xx_i2c_suspend,
    .resume = stk83xx_i2c_resume,
};
#endif /* CONFIG_PM_SLEEP */

#ifdef CONFIG_ACPI
static const struct acpi_device_id stk83xx_acpi_id[] =
{
    {"STK83XX", 0},
    {}
};
MODULE_DEVICE_TABLE(acpi, stk83xx_acpi_id);
#endif /* CONFIG_ACPI */

static const struct i2c_device_id stk83xx_i2c_id[] =
{
    {STK83XX_NAME, 0},
    {}
};

MODULE_DEVICE_TABLE(i2c, stk83xx_i2c_id);

static struct i2c_driver stk83xx_i2c_driver =
{
    .probe      = stk83xx_i2c_probe,
    .remove     = stk83xx_i2c_remove,
    .detect     = stk83xx_i2c_detect,
    .id_table   = stk83xx_i2c_id,
    .class      = I2C_CLASS_HWMON,
    .driver = {
        .owner  = THIS_MODULE,
        .name   = STK83XX_NAME,
#ifdef CONFIG_PM_SLEEP
        .pm     = &stk83xx_pm_ops,
#endif
#ifdef CONFIG_ACPI
        .acpi_match_table = ACPI_PTR(stk83xx_acpi_id),
#endif /* CONFIG_ACPI */
#ifdef CONFIG_OF
        .of_match_table = stk83xx_match_table,
#endif /* CONFIG_OF */
    }
};


static int __init stk83xx_init(void)
{
	/* register driver */
	int res;
	res = i2c_add_driver(&stk83xx_i2c_driver);
	if (res < 0) {
		printk(KERN_INFO "add stk83xx i2c driver failed\n");
		return -ENODEV;
	}
	return res;
}

static void __exit stk83xx_exit(void)
{
	i2c_del_driver(&stk83xx_i2c_driver);
}

//module_i2c_driver(stk83xx_i2c_driver);
module_init(stk83xx_init);
module_exit(stk83xx_exit);

MODULE_AUTHOR("Sensortek");
MODULE_DESCRIPTION("stk83xx 3-Axis accelerometer driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(STK_QUALCOMM_VERSION);
