#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/delay.h>

#include <linux/io.h>

#define GDEBUG 0
#include <linux/gallen_dbg.h>

#define SINGLE_SHOT_TIME (6*1000) //in ms


#ifdef CONFIG_MFD_RICOH619 //[
#include <linux/power/ricoh619_battery.h>
extern int ricoh619_charger_detect(void);
#else 
#define NO_CHARGER_PLUGGED	0
#define SDP_CHARGER					1
#endif //]CONFIG_MFD_RICOH619


struct usb_plug_priv {
	struct device    *ddev;
	int (*get_status) (void);
};

static struct usb_plug_priv *mxc_usbplug;
extern void set_pmic_dc_charger_state(int dccharger);

void usb_plug_handler(void *dummy)
{
	int plugged;
	static int last_status=-1;

	GALLEN_DBGLOCAL_BEGIN();

	if(0==mxc_usbplug) {
		printk(KERN_ERR "%s(%d):skip %s() when mxc_usbplug not exist !\n",
				__FILE__,__LINE__,__FUNCTION__);
		return ;
	}
	if(0==mxc_usbplug->ddev) {
		printk(KERN_ERR "%s(%d):skip %s() when mxc_usbplug->ddev not exist !\n",
				__FILE__,__LINE__,__FUNCTION__);
		return ;
	}

#ifdef CONFIG_MFD_RICOH619 //[
	plugged = ricoh619_charger_detect ();
#else //][

	printk("%s() : charger detecting function TBD !\n",__FUNCTION__);
	plugged = SDP_CHARGER ;


#endif //]CONFIG_MFD_RICOH619



	if(plugged==last_status) {
		return ;
	}

	if (NO_CHARGER_PLUGGED != plugged) {
		if ((SDP_CHARGER==plugged)||(CDP_CHARGER==plugged)) {
			kobject_rename(&mxc_usbplug->ddev->kobj, "usb_host");
		}
		else {
			kobject_rename(&mxc_usbplug->ddev->kobj, "usb_plug");
		}

		pr_info("usb plugged %d\n", plugged);
		kobject_uevent(&mxc_usbplug->ddev->kobj, KOBJ_ADD);
	} else {
		kobject_uevent(&mxc_usbplug->ddev->kobj, KOBJ_REMOVE);
		kobject_set_name(&mxc_usbplug->ddev->kobj, "%s", "usb_state");
		pr_info("usb unplugged\n");
	}
	last_status = plugged;
	GALLEN_DBGLOCAL_END();
}

static void single_shot_worker(struct work_struct *work)
{
	GALLEN_DBGLOCAL_BEGIN();
	usb_plug_handler(NULL);
	GALLEN_DBGLOCAL_END();

}
static DECLARE_DELAYED_WORK(single_shot_work, single_shot_worker);

static int usb_plug_probe(struct platform_device *pdev)
{
	int ret;
	struct usbplug_event_platform_data *pdata = pdev->dev.platform_data;

	mxc_usbplug = kzalloc(sizeof(struct usb_plug_priv), GFP_KERNEL);
	if (!mxc_usbplug) {
		GALLEN_DBGLOCAL_RUNLOG(0);
		dev_err(&pdev->dev, "Error: kzalloc\n");
		ret = -ENOMEM;
		goto err_allocate;
	}

	mxc_usbplug->ddev = &pdev->dev;
	kobject_set_name(&mxc_usbplug->ddev->kobj, "%s", "usb_state");
	schedule_delayed_work(&single_shot_work, msecs_to_jiffies(SINGLE_SHOT_TIME));

	GALLEN_DBGLOCAL_END();
	return 0;

err_allocate:

	return ret;
}

static int usb_plug_remove(struct platform_device *pdev)
{
	GALLEN_DBGLOCAL_BEGIN();
	cancel_delayed_work_sync(&single_shot_work);
	kfree(mxc_usbplug);
	GALLEN_DBGLOCAL_END();
	return 0;
}

static struct platform_driver usb_plug_driver = {
	.probe		= usb_plug_probe,
	.remove		= usb_plug_remove,
	.driver		= {
		.name	= "usb_plug",
		.owner	= THIS_MODULE,
	},
};
#ifndef CONFIG_FSL_UTP
static int usb_plug_init(void)
{
	return platform_driver_register(&usb_plug_driver);
}
module_init(usb_plug_init);

static void usb_plug_exit(void)
{
	platform_driver_unregister(&usb_plug_driver);
}
module_exit(usb_plug_exit);
#endif

