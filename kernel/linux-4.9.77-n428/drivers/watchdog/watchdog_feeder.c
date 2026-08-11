#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <asm/segment.h>
#include <asm/uaccess.h>
#include <linux/slab.h>
#include <asm/string.h>
#include <linux/ioctl.h>
#include <linux/watchdog.h>
#include <linux/fcntl.h>

static struct kobject *register_kobj;
static char *param_buf;
struct task_struct *task0;
int val;
const char* watchdog_path = "/dev/watchdog";
int timeout = 31;

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

static int task(void *arg)
{
    struct file *fd = NULL;
    char buf[8] = "dogfood";
    unsigned int feeding_interval;

    fd = filp_open("/dev/watchdog", O_WRONLY, 0644);

    while (IS_ERR(fd)) {
        fd = filp_open("/dev/watchdog", O_WRONLY, 0644);
        // printk(KERN_INFO "Open /dev/watchdog error\n");
        msleep(100);
    }

    feeding_interval = (timeout - 3) * 1000;
    printk(KERN_INFO "watchdog feeding_interval = %d ms\n", feeding_interval);

    while (!kthread_should_stop()) {
        fd->f_op->write(fd, (char *)buf, sizeof(buf), &fd->f_pos);
        msleep(feeding_interval);
    }

    filp_close(fd, NULL);
    do_exit(1);

    return val;
}

static int watchdog_feeder_init(void)
{
    val = 1;

    if (!IS_ERR_OR_NULL(task0)) {
		pr_info("watchdog_feeder_init task0 already run.\n");
	} else {
		pr_info("watchdog_feeder_init task0 is null.\n");
		task0 = kthread_run(&task, (void *)val, "feeding_thread");

		param_buf = kzalloc(PAGE_SIZE, GFP_KERNEL);
		register_kobj = kobject_create_and_add("watchdog", kernel_kobj);

		if (!register_kobj)
			return -ENOMEM;

		if (sysfs_create_group(register_kobj, &reg_attr_group)){
			kobject_put(register_kobj);
			return -ENOMEM;
		}
	}
    return 0;
}

static void watchdog_feeder_exit(void)
{
	int ret;

    if (!IS_ERR_OR_NULL(task0)) {
		ret = kthread_stop(task0);
		task0 = NULL;
		kfree(param_buf);
		kobject_put(register_kobj);
		printk(KERN_INFO "watchdog_feeder_exit exit code %d - stop task!\n",  ret);
	} else {
		printk(KERN_INFO "watchdog_feeder_exit -no task need to stop! \n");
		return;
	}
}

module_init(watchdog_feeder_init);
module_exit(watchdog_feeder_exit);
MODULE_LICENSE("GPL");
