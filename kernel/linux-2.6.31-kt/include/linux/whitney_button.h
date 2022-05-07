#ifndef _WHITNEY_KEYS_H
#define _WHITNEY_KEYS_H

struct whitney_keys_button {
	/* Configuration parameters */
	int code;		/* input event code (KEY_*, SW_*) */
	int gpio;
	int active_low;
	char *desc;
	int type;		/* input event type (EV_KEY, EV_SW) */
	int wakeup;		/* configure the button as a wake-up source */
	int debounce_interval;	/* debounce ticks interval in msecs */
};

struct whitney_keys_platform_data {
	struct whitney_keys_button *buttons;
	int nbuttons;
	unsigned int rep:1;		/* enable input subsystem auto repeat */
	int (*enable)(struct device *dev);
	void (*disable)(struct device *dev);
};

#endif
