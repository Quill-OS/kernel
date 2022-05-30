/* kernel/power/consoleearlysuspend.c
 *
 * Copyright (C) 2005-2008 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/console.h>
#include <linux/earlysuspend.h>
#include <linux/kbd_kern.h>
#include <linux/module.h>
#include <linux/vt_kern.h>
#include <linux/wait.h>

#define EARLY_SUSPEND_CONSOLE	(MAX_NR_CONSOLES-1)

static int orig_fgconsole;
static void console_early_suspend(struct early_suspend *h)
{
	acquire_console_sem();
	orig_fgconsole = fg_console;
	if (vc_allocate(EARLY_SUSPEND_CONSOLE))
		goto err;
	if (set_console(EARLY_SUSPEND_CONSOLE))
		goto err;
	release_console_sem();

	if (vt_waitactive(EARLY_SUSPEND_CONSOLE + 1))
		pr_warning("console_early_suspend: Can't switch VCs.\n");
	return;
err:
	pr_warning("console_early_suspend: Can't set console\n");
	release_console_sem();
}

static void console_late_resume(struct early_suspend *h)
{
	int ret;
	acquire_console_sem();
	ret = set_console(orig_fgconsole);
	release_console_sem();
	if (ret) {
		pr_warning("console_late_resume: Can't set console.\n");
		return;
	}

	if (vt_waitactive(orig_fgconsole + 1))
		pr_warning("console_late_resume: Can't switch VCs.\n");
}

static struct early_suspend console_early_suspend_desc = {
	.level = EARLY_SUSPEND_LEVEL_STOP_DRAWING,
	.suspend = console_early_suspend,
	.resume = console_late_resume,
};

static ssize_t console_switch_show(struct kobject *kobj,
				   struct kobj_attribute *attr, char *buf)
{
	return 0;
}

#define power_ro_attr(_name) \
	static struct kobj_attribute _name##_attr = {	\
		.attr	= {				\
			.name = __stringify(_name),	\
			.mode = 0444,			\
		},					\
		.show	= _name##_show,			\
		.store	= NULL,		\
	}

power_ro_attr(console_switch);

static struct attribute *g[] = {
	&console_switch_attr.attr,
	NULL,
};

static struct attribute_group attr_group = {
	.attrs = g,
};

static int __init console_early_suspend_init(void)
{
	int ret;

	ret = sysfs_create_group(power_kobj, &attr_group);
	if (ret) {
		pr_err("android_power_init: sysfs_create_group failed\n");
		return ret;
	}
	register_early_suspend(&console_early_suspend_desc);
	return 0;
}

static void  __exit console_early_suspend_exit(void)
{
	unregister_early_suspend(&console_early_suspend_desc);
	sysfs_remove_group(power_kobj, &attr_group);
}

module_init(console_early_suspend_init);
module_exit(console_early_suspend_exit);

