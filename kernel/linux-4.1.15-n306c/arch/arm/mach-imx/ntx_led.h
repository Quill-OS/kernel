#ifndef NTX_LED_H //[
#define NTX_LED_H

void ntx_led_current (unsigned int channel, unsigned char value);
void ntx_led_dc (unsigned int channel, unsigned char dc);
void ntx_led_blink (unsigned int channel, int period);
void ntx_led_init(void);
void ntx_led_release(void);
void ntx_led_set_auto_blink(int iAutoModeMode);


void led_green (int isOn);
void led_blue (int isOn);
void led_red (int isOn);

#endif //]NTX_LED_H


