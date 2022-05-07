#ifndef _YOSHIME_HALL_H
#define _YOSHIME_HALL_H

struct yoshime_hall_platform_data {
	int hall_gpio;
	int hall_irq;
	char *desc;
	int wakeup;		/* configure the button as a wake-up source */
	int (*enable)(struct device *dev);
	void (*disable)(struct device *dev);
};

#define HALL_INPUT_CODE	KEY_STOP	
#define HALL_INPUT_TYPE	EV_KEY 

#define HALL_MAGIC_NUMBER		'H'
/* HALL_SENSOR STATES : OPEN=0 CLOSED=1 */
#define HALL_IOCTL_GET_STATE	 _IOR(HALL_MAGIC_NUMBER, 0x01, int)

#endif
