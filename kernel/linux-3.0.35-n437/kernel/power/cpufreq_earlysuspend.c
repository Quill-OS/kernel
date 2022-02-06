/*
 * Copyright (C) 2012 Freescale Semiconductor, Inc. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <linux/fs.h>
#include <linux/earlysuspend.h>
#include <linux/cpufreq.h>

extern void cpufreq_save_default_governor(void);
extern void cpufreq_restore_default_governor(void);
extern void cpufreq_set_conservative_governor_param(int up_th, int down_th);
extern void cpufreq_set_performance_governor(void);
extern void cpufreq_set_conservative_governor(void);

#define SET_GOVERNOR_TO_PERFORMANCE		1

static void cpufreq_early_suspend(struct early_suspend *p)
{
	cpufreq_save_default_governor();
#ifdef SET_GOVERNOR_TO_PERFORMANCE//[
	cpufreq_set_performance_governor();
#else //][SET_GOVERNOR_TO_PERFORMANCE
	cpufreq_set_conservative_governor();
	cpufreq_set_conservative_governor_param(
		SET_CONSERVATIVE_GOVERNOR_UP_THRESHOLD,
		SET_CONSERVATIVE_GOVERNOR_DOWN_THRESHOLD);
#endif //]SET_GOVERNOR_TO_PERFORMANCE
}

static void cpufreq_late_resume(struct early_suspend *p)
{
	cpufreq_restore_default_governor();
}

struct early_suspend cpufreq_earlysuspend = {
	.level = EARLY_SUSPEND_LEVEL_POST_DISABLE_FB,
	.suspend = cpufreq_early_suspend,
	.resume = cpufreq_late_resume,
};

static int __init cpufreq_on_earlysuspend_init(void)
{
	register_early_suspend(&cpufreq_earlysuspend);
	return 0;
}

static void __exit cpufreq_on_earlysuspend_exit(void)
{
	unregister_early_suspend(&cpufreq_earlysuspend);
}

module_init(cpufreq_on_earlysuspend_init);
module_exit(cpufreq_on_earlysuspend_exit);
