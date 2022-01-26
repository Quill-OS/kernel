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
#include <linux/usbplugevent.h>
#include <linux/delay.h>

#include <linux/io.h>

#define GDEBUG 0
#include <linux/gallen_dbg.h>

#include "../../../arch/arm/mach-mx6/crm_regs.h"
#include <mach/arc_otg.h>

#define SINGLE_SHOT_TIME (6*1000) //in ms
//#define MXC_CCM_CCGR2 0xF7ED4070	// remapped IO addr for CCGR2 reg
//#define USB_PHY_CTR_FUNC 0xF7E80808	// remapped IO addr for USB_PHY_CTR reg

struct usb_plug_priv {
	struct device    *ddev;
	int (*get_status) (void);
};

static struct usb_plug_priv *mxc_usbplug;
extern void set_pmic_dc_charger_state(int dccharger);

static void usb_plug_handler(void *dummy)
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

	plugged = mxc_usbplug->get_status();

	if(plugged==last_status) {
		return ;
	}

	if (plugged) {
		int charger;
		int ret;
		//volatile unsigned long *ccm_ccgr2 = (unsigned long*)(MXC_CCM_CCGR2);
		//volatile unsigned long *usb_phy = (unsigned long*)(USB_PHY_CTR_FUNC);
		unsigned long old_ccgr2;
		unsigned long old_usbphy;
		u32 dwTemp;

		GALLEN_DBGLOCAL_RUNLOG(0);
	
		#if 0	//[
		/* query the i.MX to test for D+/D- charger detection */
		//old_ccgr2 = *ccm_ccgr2;
		old_ccgr2 = __raw_readl(MXC_CCM_CCGR2);
		// *ccm_ccgr2 = (old_ccgr2 | 0x0C000000); /* enable usbotg clocks temporarily */
		dwTemp = (old_ccgr2 | 0x0C000000);
		__raw_writel(dwTemp,MXC_CCM_CCGR2);
		mdelay(100);


		//old_usbphy = *usb_phy;   
		old_usbphy = __raw_readl(USB_PHY_CTR_FUNC);   
		// *usb_phy = (old_usbphy | 0x01800000);  /* enable charge detection circuit  */
		dwTemp = (old_usbphy | 0x01800000);  /* enable charge detection circuit  */
		__raw_writel(dwTemp,USB_PHY_CTR_FUNC);
		mdelay(100);	

		//charger = (int)(*usb_phy & (0x00000004)); /* bit 2 = charger detection */
		charger = (int)(old_usbphy & (0x00000004)); /* bit 2 = charger detection */

		/* restore old vals */
		// *usb_phy = old_usbphy;
		__raw_writel(old_usbphy,USB_PHY_CTR_FUNC);
		// *ccm_ccgr2 = old_ccgr2;
		__raw_writel(old_ccgr2,MXC_CCM_CCGR2);
		#else//][
		if (1 < plugged) {
			// for PMIC support usb charger detect function.
			charger=1;
		}
		else
			charger=0;
		#endif //]

		if (charger) {
			GALLEN_DBGLOCAL_RUNLOG(1);
			set_pmic_dc_charger_state(1);
			ret = kobject_rename(&mxc_usbplug->ddev->kobj, "usb_plug");
		} else {
			GALLEN_DBGLOCAL_RUNLOG(2);
			ret = kobject_rename(&mxc_usbplug->ddev->kobj, "usb_host");
		}
		pr_info("usb plugged %d-%d\n", plugged, charger);
		kobject_uevent(&mxc_usbplug->ddev->kobj, KOBJ_ADD);
	} else {
		GALLEN_DBGLOCAL_RUNLOG(3);
		kobject_uevent(&mxc_usbplug->ddev->kobj, KOBJ_REMOVE);
		kobject_set_name(&mxc_usbplug->ddev->kobj, "%s", "usb_state");
		set_pmic_dc_charger_state(0);
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

static int __devinit usb_plug_probe(struct platform_device *pdev)
{
	int ret;
	struct usbplug_event_platform_data *pdata = pdev->dev.platform_data;

	GALLEN_DBGLOCAL_BEGIN();
	mxc_usbplug = kzalloc(sizeof(struct usb_plug_priv), GFP_KERNEL);
	if (!mxc_usbplug) {
		GALLEN_DBGLOCAL_RUNLOG(0);
		dev_err(&pdev->dev, "Error: kzalloc\n");
		ret = -ENOMEM;
		goto err_allocate;
	}

	mxc_usbplug->ddev = &pdev->dev;
	kobject_set_name(&mxc_usbplug->ddev->kobj, "%s", "usb_state");
	mxc_usbplug->get_status = pdata->get_key_status;
	pdata->register_usbplugevent(usb_plug_handler);
	schedule_delayed_work(&single_shot_work,
		msecs_to_jiffies(SINGLE_SHOT_TIME));

	GALLEN_DBGLOCAL_END();
	return 0;

err_allocate:

	return ret;
}

static int __devexit usb_plug_remove(struct platform_device *pdev)
{
	GALLEN_DBGLOCAL_BEGIN();
	cancel_delayed_work_sync(&single_shot_work);
	kfree(mxc_usbplug);
	GALLEN_DBGLOCAL_END();
	return 0;
}

static struct platform_driver usb_plug_driver = {
	.probe		= usb_plug_probe,
	.remove		= __devexit_p(usb_plug_remove),
	.driver		= {
		.name	= "usb_plug",
		.owner	= THIS_MODULE,
	},
};

static int __init usb_plug_init(void)
{
	return platform_driver_register(&usb_plug_driver);
}
module_init(usb_plug_init);

static void __exit usb_plug_exit(void)
{
	platform_driver_unregister(&usb_plug_driver);
}
module_exit(usb_plug_exit);

