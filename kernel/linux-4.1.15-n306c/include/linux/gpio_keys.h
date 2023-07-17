#ifndef _GPIO_KEYS_H
#define _GPIO_KEYS_H

struct device;
struct gpio_desc;

typedef int (*gpio_keys_callback)(struct gpio_keys_button *I_gpio_btn_data,int state);
/**
 * struct gpio_keys_button - configuration parameters
 * @code:		input event code (KEY_*, SW_*)
 * @gpio:		%-1 if this key does not support gpio
 * @active_low:		%true indicates that button is considered
 *			depressed when gpio is low
 * @desc:		label that will be attached to button's gpio
 * @type:		input event type (%EV_KEY, %EV_SW, %EV_ABS)
 * @wakeup:		configure the button as a wake-up source
 * @debounce_interval:	debounce ticks interval in msecs
 * @can_disable:	%true indicates that userspace is allowed to
 *			disable button via sysfs
 * @value:		axis value for %EV_ABS
 * @irq:		Irq number in case of interrupt keys
 * @gpiod:		GPIO descriptor
 */
struct gpio_keys_button {
	unsigned int code;
	int gpio;
	int active_low;
	const char *desc;
	unsigned int type;
	int wakeup;
	int debounce_interval;
	bool can_disable;
	bool do_not_wakeup_in_hibernation;
	int value;
	unsigned int irq;
	struct gpio_desc *gpiod;
	gpio_keys_callback hook;
};

/**
 * struct gpio_keys_platform_data - platform data for gpio_keys driver
 * @buttons:		pointer to array of &gpio_keys_button structures
 *			describing buttons attached to the device
 * @nbuttons:		number of elements in @buttons array
 * @poll_interval:	polling interval in msecs - for polling driver only
 * @rep:		enable input subsystem auto repeat
 * @enable:		platform hook for enabling the device
 * @disable:		platform hook for disabling the device
 * @name:		input device name
 */
struct gpio_keys_platform_data {
	struct gpio_keys_button *buttons;
	int nbuttons;
	unsigned int poll_interval;
	unsigned int rep:1;
	int (*enable)(struct device *dev);
	void (*disable)(struct device *dev);
	const char *name;
};

#endif
