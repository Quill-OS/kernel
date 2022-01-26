
#include <linux/fs.h>
#include <linux/cpufreq.h>
#if 0
extern void cpufreq_save_default_governor(void);
extern void cpufreq_restore_default_governor(void);
extern void cpufreq_set_conservative_governor(void);
extern void cpufreq_set_performance_governor(void);
extern void cpufreq_set_conservative_governor_param(int up_th, int down_th);
#endif

//#define GOV_CHG_DBG		1

#define SET_CONSERVATIVE_GOVERNOR_UP_THRESHOLD 95
#define SET_CONSERVATIVE_GOVERNOR_DOWN_THRESHOLD 50

static char cpufreq_gov_default[32];

static char *sz_cpufreq_gov_performance = "performance";
static char *sz_cpufreq_gov_conservative = "conservative";

static char *cpufreq_sysfs_place_holder = "/sys/devices/system/cpu/cpu%i/cpufreq/scaling_governor";
static char *cpufreq_gov_conservative_param = "/sys/devices/system/cpu/cpufreq/conservative/%s";

static void cpufreq_set_governor(char *governor)
{
	struct file *scaling_gov = NULL;
	char    buf[128];
	int i;
	loff_t offset = 0;

	if (governor == NULL)
		return;

	for_each_online_cpu(i) {
		sprintf(buf, cpufreq_sysfs_place_holder, i);
		scaling_gov = filp_open(buf, O_RDWR, 0);
		if (scaling_gov != NULL) {
			if (scaling_gov->f_op != NULL &&
				scaling_gov->f_op->write != NULL) 
			{
				scaling_gov->f_op->write(scaling_gov,
						governor,
						strlen(governor),
						&offset);
#ifdef GOV_CHG_DBG//[
				printk("%s():set policy \"%s\"\n",__FUNCTION__,governor);
#endif //]GOV_CHG_DBG
			}
			else
				pr_err("f_op might be null\n");

			filp_close(scaling_gov, NULL);
		} else {
			pr_err("%s. Can't open %s\n", __func__, buf);
		}
	}
}

void cpufreq_save_default_governor(void)
{
	int ret;
	struct cpufreq_policy current_policy;
	ret = cpufreq_get_policy(&current_policy, 0);
	if (ret < 0)
		pr_err("%s: cpufreq_get_policy got error", __func__);
	memcpy(cpufreq_gov_default, current_policy.governor->name, 32);
#ifdef GOV_CHG_DBG//[
	printk("%s():save policy \"%s\"\n",__FUNCTION__,cpufreq_gov_default);
#endif //]GOV_CHG_DBG
}

void cpufreq_restore_default_governor(void)
{
	cpufreq_set_governor(cpufreq_gov_default);
#ifdef GOV_CHG_DBG//[
	printk("%s():restore policy \"%s\"\n",__FUNCTION__,cpufreq_gov_default);
#endif //]GOV_CHG_DBG
}

void cpufreq_set_conservative_governor_param(int up_th, int down_th)
{
	struct file *gov_param = NULL;
	static char buf[128], parm[8];
	loff_t offset = 0;

	if (up_th <= down_th) {
		printk(KERN_ERR "%s: up_th(%d) is lesser than down_th(%d)\n",
			__func__, up_th, down_th);
		return;
	}

	sprintf(parm, "%d", up_th);
	sprintf(buf, cpufreq_gov_conservative_param , "up_threshold");
	gov_param = filp_open(buf, O_RDONLY, 0);
	if (gov_param != NULL) {
		if (gov_param->f_op != NULL &&
			gov_param->f_op->write != NULL)
			gov_param->f_op->write(gov_param,
					parm,
					strlen(parm),
					&offset);
		else
			pr_err("f_op might be null\n");

		filp_close(gov_param, NULL);
	} else {
		pr_err("%s. Can't open %s\n", __func__, buf);
	}

	sprintf(parm, "%d", down_th);
	sprintf(buf, cpufreq_gov_conservative_param , "down_threshold");
	gov_param = filp_open(buf, O_RDONLY, 0);
	if (gov_param != NULL) {
		if (gov_param->f_op != NULL &&
			gov_param->f_op->write != NULL)
			gov_param->f_op->write(gov_param,
					parm,
					strlen(parm),
					&offset);
		else
			pr_err("f_op might be null\n");

		filp_close(gov_param, NULL);
	} else {
		pr_err("%s. Can't open %s\n", __func__, buf);
	}
}
void cpufreq_set_performance_governor(void)
{
	cpufreq_set_governor(sz_cpufreq_gov_performance);
}
void cpufreq_set_conservative_governor(void)
{
	cpufreq_set_governor(sz_cpufreq_gov_conservative);
}


