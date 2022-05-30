#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>

#define SINGLE_SHOT_TIME (10*1000) //in ms

struct sony_bbsw {
	struct input_dev *dev;
	struct input_event *event;
	int gpio;
};

static struct timer_list single_shot_timer;

#define BOARD_ID	(5*32+14)	/* GPIO_6_14 */

static irqreturn_t sony_bbsw_handler(int irq, void *data)
{
	struct sony_bbsw *sony_bbsw = data;
	
	if ( !gpio_get_value(BOARD_ID) )  {	//In the case of BB.
		input_report_switch(sony_bbsw->dev, sony_bbsw->event->code , 
		gpio_get_value(sony_bbsw->gpio));
		input_sync(sony_bbsw->dev);
	}
	
	return IRQ_HANDLED;
}

static void single_shot(unsigned long data)
{
	struct sony_bbsw *sony_bbsw = (struct sony_bbsw *)data;
	
	if ( !gpio_get_value(BOARD_ID) )  {	//In the case of BB.
		input_report_switch(sony_bbsw->dev, sony_bbsw->event->code , 
			gpio_get_value(sony_bbsw->gpio));
		input_sync(sony_bbsw->dev);
	}

	del_timer(&single_shot_timer);
}

static int __devinit sony_bbsw_probe(struct platform_device *pdev)
{
	int ret;
	int irq;
	struct sony_bbsw *sony_bbsw;

	sony_bbsw = kzalloc(sizeof(struct sony_bbsw), GFP_KERNEL);
	if (!sony_bbsw) {
		dev_err(&pdev->dev, "Error: kzalloc\n");
		ret = -ENOMEM;
		goto err_allocate;
	}

	sony_bbsw->event = pdev->dev.platform_data;
	sony_bbsw->gpio = platform_get_resource(pdev, IORESOURCE_IO, 0)->start;

	ret = gpio_request(sony_bbsw->gpio, "sony-bbsw");
	if (ret){
		dev_err(&pdev->dev, "Error: gpio_request\n");
		goto err_gpio_request;
	}

    ret = gpio_direction_input(sony_bbsw->gpio);
	if (ret){
		dev_err(&pdev->dev, "Error: gpio_direction_input\n");
		goto err_gpio_input;
	}

	irq = platform_get_irq(pdev, 0);
	ret = request_irq(irq, sony_bbsw_handler, 
		IRQF_TRIGGER_RISING|IRQF_TRIGGER_FALLING, "sony_bbsw", sony_bbsw);
	if (ret) {
		dev_err(&pdev->dev, "Error: request_irq\n");
		goto err_request_irq;
	}

	sony_bbsw->dev = input_allocate_device();
	if (!sony_bbsw->dev) {
		dev_err(&pdev->dev, "Error: input_allocate_device\n");
		ret = -ENOMEM;
		goto err_input_allocate;
	}
	sony_bbsw->dev->evbit[0] = BIT_MASK(sony_bbsw->event->type);
	sony_bbsw->dev->swbit[BIT_WORD(sony_bbsw->event->code)] = 
		BIT_MASK(sony_bbsw->event->code);
	sony_bbsw->dev->name = "sony_bbsw";
	sony_bbsw->dev->dev.parent = &pdev->dev;
	ret = input_register_device(sony_bbsw->dev);
	if (ret) {
		dev_dbg(&pdev->dev, "Error: input_register_device\n");
		goto err_input_register;
	}

	dev_info(&pdev->dev, "gpio %d irq %d event(type %d code %d)\n", 
		sony_bbsw->gpio, irq, sony_bbsw->event->type, sony_bbsw->event->code);
	platform_set_drvdata(pdev, sony_bbsw);
	
	init_timer(&single_shot_timer);
	single_shot_timer.expires = jiffies + msecs_to_jiffies(SINGLE_SHOT_TIME);
	single_shot_timer.function = single_shot;
	single_shot_timer.data = (unsigned long)sony_bbsw;
	add_timer(&single_shot_timer);

	return 0;

	//input_unregister_device(sony_bbsw->dev);
err_input_register:

	input_free_device(sony_bbsw->dev);
err_input_allocate:

	free_irq(irq, sony_bbsw);
err_request_irq:

err_gpio_input:

	gpio_free(sony_bbsw->gpio);
err_gpio_request:

	kfree(sony_bbsw);
err_allocate:

	return ret;
}

static int __devexit sony_bbsw_remove(struct platform_device *pdev)
{
	struct sony_bbsw *sony_bbsw = platform_get_drvdata(pdev);
	
	del_timer(&single_shot_timer);
	input_unregister_device(sony_bbsw->dev);
	input_free_device(sony_bbsw->dev);
	free_irq(platform_get_irq(pdev, 0), sony_bbsw);
	gpio_free(sony_bbsw->gpio);
	kfree(sony_bbsw);
	return 0;
}

static struct platform_driver sony_bbsw_driver = {
	.probe		= sony_bbsw_probe,
	.remove		= __devexit_p(sony_bbsw_remove),
	.driver		= {
		.name	= "sony_bbsw",
		.owner	= THIS_MODULE,
	},
};

static int __init sony_bbsw_init(void)
{
	return platform_driver_register(&sony_bbsw_driver);
}
module_init(sony_bbsw_init);

static void __exit sony_bbsw_exit(void)
{
	platform_driver_unregister(&sony_bbsw_driver);
}
module_exit(sony_bbsw_exit);
