#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <asm/segment.h>
#include <asm/uaccess.h>
#include <linux/slab.h>
#include <asm/string.h>
#include <linux/watchdog.h>

#include "../../arch/arm/mach-imx/ntx_hwconfig.h"



#define FEEDING_WITH_DEVFS	1 // /dev/watchdog . 
#define FEEDING_WITH_WDD	2 // watchdog device .

#define FEEDING_METHOD	FEEDING_WITH_WDD

extern volatile NTX_HWCONFIG *gptHWCFG;

#if (FEEDING_METHOD==FEEDING_WITH_DEVFS) //[
extern unsigned int imx2_get_wdt_timeout(void);
#elif (FEEDING_METHOD==FEEDING_WITH_WDD)
extern struct watchdog_device * watchdog_get_last_dev(void);
#endif //] FEEDING_WITH_DEVFS


static struct kobject *register_kobj;
static char *param_buf;
struct task_struct *task0;
int val;
const char* watchdog_path = "/dev/watchdog";

static ssize_t __used store_value(struct kobject *kobj, 
            struct kobj_attribute *attr, const char *buf, size_t count) 
{ 
    if (!strncmp(buf, "1", 1)) {
        printk(KERN_ALERT "Stop feeding watchdog, system will reboot later\n");
        kthread_stop(task0);
	}

    return count;
}

static struct kobj_attribute store_val_attribute = __ATTR(stop_feeding, 0220, NULL, store_value);

static struct attribute *register_attrs[] = {
    &store_val_attribute.attr,
    NULL,
};

static struct attribute_group reg_attr_group = {
    .attrs = register_attrs
};


int task(void *arg) 
{
    unsigned int feeding_interval;
#if (FEEDING_METHOD==FEEDING_WITH_DEVFS) //[
    struct file *fd = NULL;
    char buf[8] = "dogfood";

    fd = filp_open("/dev/watchdog", O_WRONLY, 0644);

    while (IS_ERR(fd)) {
        fd = filp_open("/dev/watchdog", O_WRONLY, 0644);
        printk(KERN_INFO "Open /dev/watchdog error\n");
        msleep(100);
    }

    feeding_interval = (imx2_get_wdt_timeout() - 3) * 1000;
#elif (FEEDING_METHOD==FEEDING_WITH_WDD) //][!
	struct watchdog_device *wdd;

	do {
		wdd = watchdog_get_last_dev();
		if(wdd) {
			printk(KERN_INFO"watchdog device ready !!\n");
			break;
		}
		else {
			printk(KERN_ERR"watchdog device not ready !!\n");
		}
		msleep(100);
	}while (1);

    feeding_interval = (wdd->timeout - 3) * 1000;
#endif //] 
    printk(KERN_INFO "watchdog feeding_interval = %d ms\n", feeding_interval);

    while (!kthread_should_stop()) {
#if (FEEDING_METHOD==FEEDING_WITH_DEVFS)//[
        fd->f_op->write(fd, (char *)buf, sizeof(buf), &fd->f_pos);
#elif (FEEDING_METHOD==FEEDING_WITH_WDD) //][!
		wdd->ops->ping(wdd);
#endif //] FEEDING_WITH_WDD

        msleep(feeding_interval);
    }   

#if (FEEDING_METHOD==FEEDING_WITH_DEVFS)//[
    filp_close(fd, NULL);
#endif //] (FEEDING_METHOD==FEEDING_WITH_DEVFS)
    do_exit(1);

    return val;
}

static int watchdog_feeder_init(void)    
{
    if (1 == gptHWCFG->m_val.bPMIC) {
        printk(KERN_INFO "PMIC = RC5T619, no need to feed watchdog\n");
        return -ENODEV;
    } else {
        printk(KERN_INFO "PMIC != RC5T619, need to feed watchdog\n");
    }

    printk(KERN_INFO "watchdog_feeder_init\n");
    val = 1;
    task0 = kthread_run(&task, (void *)val, "feeding_thread");

    param_buf = kzalloc(PAGE_SIZE, GFP_KERNEL);
    register_kobj = kobject_create_and_add("watchdog", kernel_kobj);

    if (!register_kobj)
        return -ENOMEM;

    if (sysfs_create_group(register_kobj, &reg_attr_group)){
        kobject_put(register_kobj);
        return -ENOMEM;
    }

    return 0;
}

static void watchdog_feeder_exit(void)
{
    printk(KERN_INFO "watchdog_feeder_exit\n");
    kthread_stop(task0);
    kfree(param_buf);
    kobject_put(register_kobj);
}

module_init(watchdog_feeder_init);
module_exit(watchdog_feeder_exit);
MODULE_LICENSE("GPL");
