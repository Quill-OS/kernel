#ifndef __LK_FP9928_H //[
#define __LK_FP9928_H


#define FP9928_RET_SUCCESS							0
#define FP9928_RET_GPIOINITFAIL					(-1)
#define FP9928_RET_MEMNOTENOUGH					(-2)
#define FP9928_RET_NOTINITEDSTATE				(-3)
#define FP9928_RET_PWRDWNWORKPENDING		(-4)
#define FP9928_RET_I2CCHN_NOTFOUND			(-5)
#define FP9928_RET_I2CTRANS_ERR					(-6)


int fp9928_init(int iPort);
void fp9928_release(void);


int fp9928_suspend(void);
void fp9928_resume(void);
void fp9928_shutdown(void);


int fp9928_output_power(int iIsOutputPwr,int iIsChipPowerDown);
int fp9928_power_onoff(int iIsPowerOn,int iIsOutputPwr);

int fp9928_get_temperature(int *O_piTemperature);
int fp9928_vcom_set(int iVCOM_mV,int iIsWriteToFlash);
int fp9928_vcom_get(int *O_piVCOM_mV);
int fp9928_vcom_get_cached(int *O_piVCOM_mV);

int fp9928_ONOFF(int iIsON);

#endif //] __LK_FP9928_H

