/*
 * Copyright 2007-2010 Freescale Semiconductor, Inc. All Rights Reserved.
 */

/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#include <linux/errno.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/clk.h>

#include <mach/gpio.h>
#include <mach/clock.h>
#include <mach/iomux-mx50.h>

#include "mx50_gpio.h"

#if 1 /* E_BOOK */
#include <linux/pastlog.h>
#endif /* E_BOOK */

extern void acquire_wake_up_wake_lock(void);
extern int confirme_v29_vcc_off_flag(void);
extern void wm831x_rtc_alarm_disable(void);

void gpio_uart_active(int port, int no_irda) {}
EXPORT_SYMBOL(gpio_uart_active);

void gpio_uart_inactive(int port, int no_irda) {}
EXPORT_SYMBOL(gpio_uart_inactive);

void gpio_gps_active(void) {}
EXPORT_SYMBOL(gpio_gps_active);

void gpio_gps_inactive(void) {}
EXPORT_SYMBOL(gpio_gps_inactive);

void config_uartdma_event(int port) {}
EXPORT_SYMBOL(config_uartdma_event);

void gpio_spi_active(int cspi_mod) {}
EXPORT_SYMBOL(gpio_spi_active);

void gpio_spi_inactive(int cspi_mod) {}
EXPORT_SYMBOL(gpio_spi_inactive);

void gpio_owire_active(void) {}
EXPORT_SYMBOL(gpio_owire_active);

void gpio_owire_inactive(void) {}
EXPORT_SYMBOL(gpio_owire_inactive);

void gpio_i2c_active(int i2c_num) {}
EXPORT_SYMBOL(gpio_i2c_active);

void gpio_i2c_inactive(int i2c_num) {}
EXPORT_SYMBOL(gpio_i2c_inactive);

void gpio_i2c_hs_active(void) {}
EXPORT_SYMBOL(gpio_i2c_hs_active);

void gpio_i2c_hs_inactive(void) {}
EXPORT_SYMBOL(gpio_i2c_hs_inactive);

void gpio_pmic_active(void) {}
EXPORT_SYMBOL(gpio_pmic_active);

void gpio_sdhc_active(int module) {}
EXPORT_SYMBOL(gpio_sdhc_active);

void gpio_sdhc_inactive(int module) {}
EXPORT_SYMBOL(gpio_sdhc_inactive);

void gpio_sensor_select(int sensor) {}

void gpio_sensor_active(unsigned int csi) {}
EXPORT_SYMBOL(gpio_sensor_active);

void gpio_sensor_inactive(unsigned int csi) {}
EXPORT_SYMBOL(gpio_sensor_inactive);

void gpio_ata_active(void) {}
EXPORT_SYMBOL(gpio_ata_active);

void gpio_ata_inactive(void) {}
EXPORT_SYMBOL(gpio_ata_inactive);

void gpio_nand_active(void) {}
EXPORT_SYMBOL(gpio_nand_active);

void gpio_nand_inactive(void) {}
EXPORT_SYMBOL(gpio_nand_inactive);

void gpio_keypad_active(void) {}
EXPORT_SYMBOL(gpio_keypad_active);

void gpio_keypad_inactive(void) {}
EXPORT_SYMBOL(gpio_keypad_inactive);

int gpio_usbotg_hs_active(void)
{
	return 0;
}
EXPORT_SYMBOL(gpio_usbotg_hs_active);

void gpio_usbotg_hs_inactive(void) {}
EXPORT_SYMBOL(gpio_usbotg_hs_inactive);

void gpio_fec_active(void) {}
EXPORT_SYMBOL(gpio_fec_active);

void gpio_fec_inactive(void) {}
EXPORT_SYMBOL(gpio_fec_inactive);

void gpio_spdif_active(void) {}
EXPORT_SYMBOL(gpio_spdif_active);

void gpio_spdif_inactive(void) {}
EXPORT_SYMBOL(gpio_spdif_inactive);

void gpio_mlb_active(void) {}
EXPORT_SYMBOL(gpio_mlb_active);

void gpio_mlb_inactive(void) {}
EXPORT_SYMBOL(gpio_mlb_inactive);




void
gpio_audio_activate(void)
{
	gpio_request(AUD_MUTE, "aud-mute");
	gpio_direction_output(AUD_MUTE, 0);

	gpio_request(AUD_RESET, "aud-reset");
	gpio_direction_output(AUD_RESET, 0);

	gpio_request(HP_DETECT, "hp-detect");
	gpio_direction_input(HP_DETECT);
}
EXPORT_SYMBOL(gpio_audio_activate);

void
gpio_audio_mute(int mute)
{
#define	__AUDIO_MUTE_DELAY		(400)	/* msec */
	if (mute == 0) {
		msleep(10);
	}
	gpio_set_value(AUD_MUTE, !mute);
	msleep((mute != 0) ? __AUDIO_MUTE_DELAY * 2: __AUDIO_MUTE_DELAY);
}
EXPORT_SYMBOL(gpio_audio_mute);

void
gpio_audio_enable(int enable)
{
	gpio_set_value(AUD_RESET, enable);
}
EXPORT_SYMBOL(gpio_audio_enable);


static struct delayed_work log_int_worker;

void log_int_work(struct work_struct *work)
{
	unsigned long	flags;
	extern void turn_on_v29_vcc_at_standby(void);
	
#ifdef CONFIG_REGULATOR_WM831X

#if 1 /* E_BOOK *//* 2011/07/08 */
	//printk("logint: reset power\n");
	wm831x_rtc_alarm_disable();
	turn_on_v29_vcc_at_standby();
#else	
	wm831x_rtc_alarm_disable();
	
	if( confirme_v29_vcc_off_flag()){
		acquire_wake_up_wake_lock();
		return ;
	}
#endif
#endif
	
	local_irq_save(flags);

	//printk("logint: log flush\n");
#ifdef CONFIG_PAST_LOG /* E_BOOK */
	past_flush();
#endif /* CONFIG_PAST_LOG */

	printk("reset completed\n");
	for ( ; ; ) ;
	
	local_irq_restore(flags);

}

static int logint_flag=0;
static	irqreturn_t
gpio_logint_irq_handler(int irq, void* data)
{
	if ( logint_flag) {
	  return IRQ_HANDLED;
	}
	logint_flag=1;
	printk("reset button pressed\n");
	dump_stack();

	/* audio: mute */
	gpio_set_value(AUD_MUTE, 0);

#if 1 /* E_BOOK *//* 2011/2/23 */
	/* power down sd1 */
	gpio_set_value(4*32+12,1);
	//printk("logint:SD power off\n");
#endif

#ifdef CONFIG_AUDIO_DISABLE_ON_RESET
	/* audio: disable */
	{
	  struct	clk*	clk_audio_master;
	  mdelay(__AUDIO_MUTE_DELAY);
	  gpio_set_value(AUD_RESET, 0);
	  clk_audio_master = clk_get(NULL, "ssi_ext1_clk");
	  if (clk_audio_master != NULL) {
	    clk_audio_master->disable(clk_audio_master);
	    clk_put(clk_audio_master);
	  }
	}
#endif

	schedule_delayed_work(&log_int_worker, msecs_to_jiffies(0));

	return IRQ_HANDLED;
}

int
gpio_logint_activate(void)
{
	int		result;
	int		ret;
	int		irq_num = IOMUX_TO_IRQ_V3(LOG_INT);
	
	INIT_DELAYED_WORK_DEFERRABLE(&log_int_worker, log_int_work);

	gpio_request(LOG_INT, "log-int");
	gpio_direction_input(LOG_INT);

	result = request_irq(
					irq_num,
					gpio_logint_irq_handler,
					IRQF_TRIGGER_RISING ,
					"log-int",
					NULL
				);

	ret = enable_irq_wake(irq_num);
	if(ret != 0) {
		printk("Can't enable logint IRQ(%d) as wake source: %d\n", irq_num, ret);
	}

	return result;
}
EXPORT_SYMBOL(gpio_logint_activate);

void gpio_wwan_active(void)
{
	gpio_request(WAN_POWER_EN, "wwan_power_enable");
	gpio_direction_output(WAN_POWER_EN, 0);		//OFF

	gpio_request(WWAN_DISABLE, "wwan_disable");
	gpio_direction_output(WWAN_DISABLE, 0);		//
	
	/* add 2011/06/15 */
	gpio_request(WAN_WAKE, "wan_wake");
	gpio_direction_output(WAN_WAKE, 0);
}
EXPORT_SYMBOL(gpio_wwan_active);
