/*
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>

#include <linux/delay.h>
#include <linux/wakelock.h>
#include <linux/mfd/sub-main/core.h>

#define PWR_UP		1
#define PWR_DOWN	2

extern int get_ac_online_info(void);
extern int get_usb_online_info(void);
extern int power_sorce_detection_complete(void);

extern struct sub_main *confirm_the_sub(void);

static struct sub_main *the_sub = NULL;

static u8 power_key_enable_flag = 1;
static char dumy_pwr_key_flag = 0;
static struct wake_lock power_key_lock;
struct input_dev *the_pwr;

static unsigned char pre_pwr_key = PWR_UP;

static int power_key_send_ok_check(void)
{
	int ret = 0;
	
	if( !power_key_enable_flag ){
		ret = 0;
		return ret;
	}
	
	if( get_ac_online_info() && power_sorce_detection_complete() ){
		ret = 1;
	}else if( !get_usb_online_info() && !get_ac_online_info() ){
		ret = 1;
	}
	
	return ret;
}

int dumy_power_key(void)
{
#ifndef CONFIG_USB_G_SERIAL_MODULE
	if( !power_key_send_ok_check() ){
		return 0;
	}
#endif
	
	wake_lock_timeout(&power_key_lock, 1 * HZ);
	
	dumy_pwr_key_flag = 1;
	
	if( pre_pwr_key == PWR_UP ){

		wake_lock_timeout(&power_key_lock, 1 * HZ);

		input_report_key(the_pwr, KEY_POWER, 1); //push
		input_sync(the_pwr);
		msleep(100);
		input_report_key(the_pwr, KEY_POWER, 0); //release
		input_sync(the_pwr);

	}
	
	dumy_pwr_key_flag = 0;
		
	return 0;

}
EXPORT_SYMBOL(dumy_power_key);

static int power_key_diff_check(void)
{
	
	if(!the_sub){
		return 0;
	}
	
	if( (the_sub->pre_chg_ic_status_reg.ext_c & PWR_KEY_BIT)
		!= (the_sub->chg_ic_status_reg.ext_c & PWR_KEY_BIT))
	{
		return 1;
	}else{
		return 0;
	}
}

void power_key_detect_work(void)
{	
	if( !power_key_diff_check() ){
		return;
	}
	
#ifndef CONFIG_USB_G_SERIAL_MODULE
	if( !power_key_send_ok_check() ){
		return;
	}
#endif
	
	wake_lock_timeout(&power_key_lock, 1 * HZ);

	if( !dumy_pwr_key_flag ){
		if( the_sub->chg_ic_status_reg.ext_c & PWR_KEY_BIT ){
			//printk("PWR_DOWN \n ");
			pre_pwr_key = PWR_DOWN;
  			input_report_key(the_pwr, KEY_POWER, 1); //push
			input_sync(the_pwr);
		}else{
			//printk("PWR_UP \n ");
			pre_pwr_key = PWR_UP;
			input_report_key(the_pwr, KEY_POWER, 0); //release
			input_sync(the_pwr);
		}
	}
}
EXPORT_SYMBOL(power_key_detect_work);


/*****************************************************************************
	set_power_key_enable
*****************************************************************************/
static ssize_t set_power_key_enable(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	
	if(count > 3){
		printk("input format error!!\n");
		printk("count = %d\n",count);
		goto err;
	}
	
	if( (unsigned char)*buf == '0'){
		power_key_enable_flag = 0;
		printk("power key is disabled\n");
	}else if( (unsigned char)*buf == '1'){
		power_key_enable_flag = 1;
		printk("power key is enabled\n");
	}else{
		printk("input format error!!\n");
	}

err:
	
	return count;
}

/*****************************************************************************
	show_power_key_enable
*****************************************************************************/
static ssize_t show_power_key_enable(struct device *dev,
		  struct device_attribute *attr, char *buf)
{

	int ret = 0;
	
	if(power_key_enable_flag){
		ret =  snprintf(buf, PAGE_SIZE, "Power Key is enabled\n");
	}else{
		ret =  snprintf(buf, PAGE_SIZE, "Power Key is disabled\n");
	}
	
	return ret;
	
}

static DEVICE_ATTR(power_key_enable, S_IWUSR | S_IRUGO, show_power_key_enable, set_power_key_enable);

static struct attribute *power_key_attributes[] = {
	&dev_attr_power_key_enable.attr,
	NULL,
};

static const struct attribute_group sub_power_key_attr_group = {
	.attrs = power_key_attributes,
};


static int __devinit sub_cpu_pwrbutton_probe(struct platform_device *pdev)
{
	struct input_dev *pwr;
	int err;
	
	the_sub = confirm_the_sub();
	
	wake_lock_init(&power_key_lock, WAKE_LOCK_SUSPEND, "sub_power_key_lock");
	
	pwr = input_allocate_device();
	the_pwr = pwr;
	if (!pwr) {
		dev_dbg(&pdev->dev, "Can't allocate power button\n");
		return -ENOMEM;
	}

	pwr->evbit[0] = BIT_MASK(EV_KEY);
	pwr->keybit[BIT_WORD(KEY_POWER)] = BIT_MASK(KEY_POWER);
	pwr->name = "sub_cpu_pwrbutton";
	pwr->phys = "sub_cpu_pwrbutton/input0";
	pwr->dev.parent = &pdev->dev;

	err = input_register_device(pwr);
	if (err) {
		dev_dbg(&pdev->dev, "Can't register power button: %d\n", err);
		goto free_input_dev;
	}
	
	err = sysfs_create_group(&pdev->dev.kobj, &sub_power_key_attr_group);
	if (err)
		dev_dbg(&pdev->dev, "could not create sysfs files\n");

	platform_set_drvdata(pdev, pwr);
	
	return 0;

free_input_dev:
	input_free_device(pwr);
	return err;
}

static int __devexit sub_cpu_pwrbutton_remove(struct platform_device *pdev)
{
	struct input_dev *pwr = platform_get_drvdata(pdev);
	
	wake_lock_destroy(&power_key_lock);
	
	sysfs_remove_group(&pdev->dev.kobj, &sub_power_key_attr_group);
	
	input_unregister_device(pwr);
	
	return 0;
}

#ifdef CONFIG_PM
static int sub_cpu_pwr_key_suspend(struct platform_device *pdev,
	pm_message_t state)
{
	return 0;
}
#else
#define sub_cpu_bci_suspend	NULL
#endif /* CONFIG_PM */

struct platform_driver sub_cpu_pwrbutton_driver = {
	.probe		= sub_cpu_pwrbutton_probe,
	.remove		= __devexit_p(sub_cpu_pwrbutton_remove),
	.suspend	= sub_cpu_pwr_key_suspend,
	.driver		= {
		.name	= "sub_cpu_pwrbutton",
		.owner	= THIS_MODULE,
	},
};

static int __init sub_cpu_pwrbutton_init(void)
{
	return platform_driver_register(&sub_cpu_pwrbutton_driver);
}
//module_init(sub_cpu_pwrbutton_init);
late_initcall(sub_cpu_pwrbutton_init);

static void __exit sub_cpu_pwrbutton_exit(void)
{
	platform_driver_unregister(&sub_cpu_pwrbutton_driver);
}
module_exit(sub_cpu_pwrbutton_exit);

MODULE_ALIAS("platform:sub_cpu_pwrbutton");
MODULE_DESCRIPTION("SUB CPU Power Button");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("xxx");

