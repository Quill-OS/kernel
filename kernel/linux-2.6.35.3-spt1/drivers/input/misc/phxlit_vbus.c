#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/wakelock.h>

static struct wake_lock vbus_call_back_lock;

struct phxlit_vbus {
	struct input_dev *dev;
	struct input_event *event;
};

static struct phxlit_vbus *phxlit_vbus_data = NULL; 

static int pre_on = 0;

void phxlit_vbus_callback(int on)
{
	wake_lock_timeout(&vbus_call_back_lock, 2 * HZ);
	
	if(phxlit_vbus_data) {
		if( (pre_on != 1) && ( on == 0 )){
			input_report_switch(phxlit_vbus_data->dev, 
				phxlit_vbus_data->event->code, 1);
		}
		input_report_switch(phxlit_vbus_data->dev, 
			phxlit_vbus_data->event->code, on);
		input_sync(phxlit_vbus_data->dev);
		pre_on = on;
	}
	else
		printk(KERN_ERR"phxlit_vbus_data is NULL");
}
EXPORT_SYMBOL(phxlit_vbus_callback);

static int __devinit phxlit_vbus_probe(struct platform_device *pdev)
{
	int ret;
	
	wake_lock_init(&vbus_call_back_lock, WAKE_LOCK_SUSPEND, "vbus_call_back_lock");

	phxlit_vbus_data = kzalloc(sizeof(struct phxlit_vbus), GFP_KERNEL);
	if (!phxlit_vbus_data) {
		dev_err(&pdev->dev, "Error: kzalloc\n");
		ret = -ENOMEM;
		goto err_allocate;
	}

	phxlit_vbus_data->event = pdev->dev.platform_data;

	phxlit_vbus_data->dev = input_allocate_device();
	if (!phxlit_vbus_data->dev) {
		dev_err(&pdev->dev, "Error: input_allocate_device\n");
		ret = -ENOMEM;
		goto err_input_allocate;
	}
	phxlit_vbus_data->dev->evbit[0] = BIT_MASK(phxlit_vbus_data->event->type);
	phxlit_vbus_data->dev->swbit[BIT_WORD(phxlit_vbus_data->event->code)] = 
		BIT_MASK(phxlit_vbus_data->event->code);
	phxlit_vbus_data->dev->name = "phxlit_vbus";
	phxlit_vbus_data->dev->dev.parent = &pdev->dev;
	ret = input_register_device(phxlit_vbus_data->dev);
	if (ret) {
		dev_dbg(&pdev->dev, "Error: input_register_device\n");
		goto err_input_register;
	}

	dev_info(&pdev->dev, "event(type %d code %d)\n", 
		phxlit_vbus_data->event->type, phxlit_vbus_data->event->code);
	platform_set_drvdata(pdev, phxlit_vbus_data);
	return 0;

	//input_unregister_device(phxlit_vbus_data->dev);
err_input_register:

	input_free_device(phxlit_vbus_data->dev);
err_input_allocate:

	kfree(phxlit_vbus_data);
err_allocate:

	return ret;
}

static int __devexit phxlit_vbus_remove(struct platform_device *pdev)
{
	wake_lock_destroy(&vbus_call_back_lock);
	
	input_unregister_device(phxlit_vbus_data->dev);
	input_free_device(phxlit_vbus_data->dev);
	kfree(phxlit_vbus_data);
	return 0;
}

static struct platform_driver phxlit_vbus_driver = {
	.probe		= phxlit_vbus_probe,
	.remove		= __devexit_p(phxlit_vbus_remove),
	.driver		= {
		.name	= "phxlit_vbus",
		.owner	= THIS_MODULE,
	},
};

static int __init phxlit_vbus_init(void)
{
	return platform_driver_register(&phxlit_vbus_driver);
}
module_init(phxlit_vbus_init);

static void __exit phxlit_vbus_exit(void)
{
	platform_driver_unregister(&phxlit_vbus_driver);
}
module_exit(phxlit_vbus_exit);

