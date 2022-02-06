#ifndef _NTX_HW_//[
#define _NTX_HW_

extern NTX_GPIO * ntx_gpio_keysA[] ;
extern int gi_ntx_gpio_keys;


void _led_R(int iIsTrunOn);
void _led_G(int iIsTrunOn);
void _led_B(int iIsTrunOn);


int _get_boot_sd_number(void);
int _get_pcba_id (void);
int _power_key_status (void);
int _sd_cd_status (void);

int ntx_gpio_key_is_fastboot_down(void);
int ntx_gpio_key_is_home_down(void);
int ntx_gpio_key_is_fl_down(void);
int init_pwr_i2c_function(int iSetAsFunc);

void EPDPMIC_power_on(int iIsPowerON);
void EPDPMIC_vcom_onoff(int iIsON);
void FP9928_rail_power_onoff(int iIsON);
void ntx_hw_late_init(void);
void ntx_hw_early_init(void);

int RC5T619_read_reg(unsigned char bRegAddr,unsigned char *O_pbRegVal);
int RC5T619_write_reg(unsigned char bRegAddr,unsigned char bRegWrVal);
int RC5T619_write_buffer(unsigned char bRegAddr,unsigned char *I_pbRegWrBuf,unsigned short I_wRegWrBufBytes);

int msp430_I2C_Chn_set(unsigned int uiI2C_Chn);
int msp430_read_buf(unsigned char bRegAddr,unsigned char *O_pbBuf,int I_iBufSize);
int msp430_read_reg(unsigned char bRegAddr,unsigned char *O_pbRegVal);
int msp430_write_buf(unsigned char bRegAddr,unsigned char *I_pbBuf,int I_iBufSize);
int msp430_write_reg(unsigned char bRegAddr,unsigned char bRegVal);
#define NTXHW_BOOTDEV_SD		0x2
#define NTXHW_BOOTDEV_MMC		0x3
#define NTXHW_BOOTDEV_UNKOWN		0xff
unsigned char ntxhw_get_bootdevice_type(void);
#endif //]_NTX_HW_

