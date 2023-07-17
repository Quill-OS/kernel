#ifndef __STK83XX_QUALCOMM_H__
#define __STK83XX_QUALCOMM_H__

#include "stk83xx.h"
#include "common_define.h"

#define STK_HEADER_VERSION          "0.0.2"
#define STK_C_VERSION               "0.0.1"
#define STK_DRV_I2C_VERSION         "0.0.2"
#define STK_QUALCOMM_VERSION        "0.0.1"
#define STK_ATTR_VERSION            "0.0.1"

typedef struct stk83xx_wrapper
{
    struct i2c_manager      i2c_mgr;
    struct stk_data         stk;
	u64                     fifo_start_ns;
	ktime_t                 timestamp;
    int                     nRotate_Angle;
#ifdef QCOM_SENSORS
    #if defined STK_QUALCOMM || defined STK_SPREADTRUM
    #ifdef STK_QUALCOMM
        struct sensors_classdev     accel_cdev;
    #endif /* STK_QUALCOMM */

        struct input_dev            *input_dev_accel;   /* accel data */

    #ifdef STK_AMD
        struct input_dev            *input_dev_amd;     /* any motion data */
    #endif /* STK_AMD */
    #elif defined STK_MTK
    #ifndef STK_MTK_2_0
        struct acc_hw               hw;
        struct hwmsen_convert       cvt;
    #else //2.0
        struct hf_device            hf_dev;
    #endif
    #endif /* STK_QUALCOMM, STK_SPREADTRUM, STK_MTK */
#else
	/* accelerometer */
	struct input_dev *input_dev_accel;
#endif 

} stk83xx_wrapper;

int stk_i2c_probe(struct i2c_client *client,
                  struct common_function *common_fn);
int stk_i2c_remove(struct i2c_client *client);
#ifndef STK_MTK_2_0
    int stk83xx_suspend(struct device *dev);
    int stk83xx_resume(struct device *dev);
#endif

#endif // __STK3A8X_QUALCOMM_H__