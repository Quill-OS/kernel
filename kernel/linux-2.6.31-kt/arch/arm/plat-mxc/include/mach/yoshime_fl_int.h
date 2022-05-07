#ifndef __YOSHIME_FL_INT_H__
#define __YOSHIME_FL_INT_H__

#include <linux/yoshime_fl.h>

struct yoshime_fl_platform_data {
#ifdef CONFIG_YOSHIME_FL_PWM 
	int pwm_id;
	int pwm_period_ns;
#endif
	int max_intensity;
	int def_intensity;
};

#endif // __YOSHIME_FL_INT_H__
