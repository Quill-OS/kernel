#ifndef __STK83XX_H__
#define __STK83XX_H__

#include "common_define.h"
#include "stk83xx_config.h"
#include "stk83xx_ver.h"

/*****************************************************************************
 * Global variable
 *****************************************************************************/
/* Any motion only works under either STK_INTERRUPT_MODE or STK_POLLING_MODE */
#ifdef STK_AMD
    #if !defined STK_INTERRUPT_MODE && !defined STK_POLLING_MODE
        #undef STK_AMD
    #endif /* !defined STK_INTERRUPT_MODE && !defined STK_POLLING_MODE*/
#endif /* STK_AMD */

#ifdef STK_AUTOK
    #define STK_CALI
    #define STK_CALI_FILE
#endif /* STK_AUTOK */

#ifdef STK_FIR
#define STK_FIR_LEN         2
#define STK_FIR_LEN_MAX     32
struct data_fir
{
    s16 xyz[STK_FIR_LEN_MAX][3];
    int sum[3];
    int idx;
    int count;
};
#endif /* STK_FIR */

#if (defined STK_FIR && defined STK_ZG)
    #define ZG_FACTOR   0
#endif /* defined STK_FIR && defined STK_ZG */

#ifndef GRAVITY_EARTH_1000
    #define GRAVITY_EARTH_1000 9.876
#endif // !GRAVITY_EARTH_1000

/*****************************************************************************
 * stk83xx register, start
 *****************************************************************************/
#define STK83XX_REG_CHIPID          0x00
#define STK83XX_REG_XOUT1           0x02
#define STK83XX_REG_XOUT2           0x03
#define STK83XX_REG_YOUT1           0x04
#define STK83XX_REG_YOUT2           0x05
#define STK83XX_REG_ZOUT1           0x06
#define STK83XX_REG_ZOUT2           0x07
#define STK83XX_REG_INTSTS1         0x09
#define STK83XX_REG_INTSTS2         0x0A
#define STK83XX_REG_FIFOSTS         0x0C
#define STK83XX_REG_STEPOUT1        0x0D
#define STK83XX_REG_STEPOUT2        0x0E
#define STK83XX_REG_RANGESEL        0x0F
#define STK83XX_REG_BWSEL           0x10
#define STK83XX_REG_POWMODE         0x11
#define STK83XX_REG_ODRMODE         0x12
#define STK83XX_REG_SWRST           0x14
#define STK83XX_REG_INTEN1          0x16
#define STK83XX_REG_INTEN2          0x17
#define STK83XX_REG_INTMAP3         0x18
#define STK83XX_REG_INTMAP1         0x19
#define STK83XX_REG_INTMAP2         0x1A
#define STK83XX_REG_SIXDCFG         0x1B
#define STK83XX_REG_SIXDTHD         0x1C
#define STK83XX_REG_SIXDSTS         0x1D
#define STK83XX_REG_SIXDDLY         0x1E
#define STK83XX_REG_INTCFG1         0x20
#define STK83XX_REG_INTCFG2         0x21
#define STK83XX_REG_TILTTHD         0x23
#define STK83XX_REG_SLOPEDLY        0x27
#define STK83XX_REG_SLOPETHD        0x28
#define STK83XX_REG_SIGMOT1         0x29
#define STK83XX_REG_SIGMOT2         0x2A
#define STK83XX_REG_SIGMOT3         0x2B
#define STK83XX_REG_STEPCNT1        0x2C
#define STK83XX_REG_STEPCNT2        0x2D
#define STK83XX_REG_STEPTHD         0x2E
#define STK83XX_REG_STEPDEB         0x2F
#define STK83XX_REG_STEPMAXTW       0x31
#define STK83XX_REG_INTFCFG         0x34
#define STK83XX_REG_OFSTCOMP1       0x36
#define STK83XX_REG_OFSTX           0x38
#define STK83XX_REG_OFSTY           0x39
#define STK83XX_REG_OFSTZ           0x3A
#define STK83XX_REG_CFG1            0x3D
#define STK83XX_REG_CFG2            0x3E
#define STK83XX_REG_FIFOOUT         0x3F

/* STK83XX_REG_CHIPID */
#define STK8BA50_R_ID                       0x86
#define STK8BA53_ID                         0x87
#define STK8323_ID                          0x23 /* include for STK8321 */
#define STK8327_ID                          0x26 /* include for STK8331 */
#define STK8329_ID                          0x25
static const u8 STK_ID[] = {STK8BA50_R_ID, STK8BA53_ID, STK8323_ID, STK8327_ID, STK8329_ID};

/* STK83XX_REG_INTSTS1 */
#define STK83XX_INTSTS1_SIG_MOT_STS         0x1
#define STK83XX_INTSTS1_ANY_MOT_STS         0x4

/* STK83XX_REG_INTSTS2 */
#define STK83XX_INTSTS2_FWM_STS_MASK        0x40

/* STK83XX_REG_FIFOSTS */
#define STK83XX_FIFOSTS_FIFOOVER            0x80
#define STK83XX_FIFOSTS_FIFO_FRAME_CNT_MASK 0x7F

/* STK83XX_REG_RANGESEL */
#define STK83XX_RANGESEL_2G                 0x3
#define STK83XX_RANGESEL_4G                 0x5
#define STK83XX_RANGESEL_8G                 0x8
#define STK83XX_RANGESEL_16G                0xc
#define STK83XX_RANGESEL_BW_MASK            0xF
#define STK83XX_RANGESEL_DEF                STK83XX_RANGESEL_2G
typedef enum
{
    STK_2G = STK83XX_RANGESEL_2G,
    STK_4G = STK83XX_RANGESEL_4G,
    STK_8G = STK83XX_RANGESEL_8G,
    STK_16G = STK83XX_RANGESEL_16G
} stk_rangesel;

/* STK83XX_REG_BWSEL */
#define STK83XX_BWSEL_BW_7_81               0x08    /* ODR = BW x 2 = 15.62Hz */
#define STK83XX_BWSEL_BW_15_63              0x09    /* ODR = BW x 2 = 31.26Hz */
#define STK83XX_BWSEL_BW_31_25              0x0A    /* ODR = BW x 2 = 62.5Hz */
#define STK83XX_BWSEL_BW_62_5               0x0B    /* ODR = BW x 2 = 125Hz */
#define STK83XX_BWSEL_BW_125                0x0C    /* ODR = BW x 2 = 250Hz */
#define STK83XX_BWSEL_BW_250                0x0D    /* ODR = BW x 2 = 500Hz */
#define STK83XX_BWSEL_BW_500                0x0E    /* ODR = BW x 2 = 1000Hz */

/* STK83XX_REG_POWMODE */
#define STK83XX_PWMD_SUSPEND                0x80
#define STK83XX_PWMD_LOWPOWER               0x40
#define STK83XX_PWMD_SLEEP_TIMER            0x20
#define STK83XX_PWMD_NORMAL                 0x00
/* STK83XX_REG_ODRMODE */
#define STK83XX_ODR_NORMODE                 0x00
#define STK83XX_ODR_ESMMODE                 0x08

/* STK83XX_SLEEP_DUR */
#define STK83XX_PWMD_1                      0x1E
#define STK83XX_PWMD_2                      0x1C
#define STK83XX_PWMD_10                     0x1A
#define STK83XX_PWMD_20                     0x18
#define STK83XX_PWMD_25                     0x16
#define STK83XX_PWMD_50                     0x14
#define STK83XX_PWMD_100                    0x12
#define STK83XX_PWMD_163                    0x10
#define STK83XX_PWMD_200                    0x0E
#define STK83XX_PWMD_300                    0x0C
#define STK83XX_PWMD_SLP_MASK               0x7E

/* STK83XX_REG_SWRST */
#define STK83XX_SWRST_VAL                   0xB6

/* STK83XX_REG_INTEN1 */
#define STK83XX_INTEN1_SLP_EN_XYZ           0x07

/* STK83XX_REG_INTEN2 */
#define STK83XX_INTEN2_DATA_EN              0x10
#define STK83XX_INTEN2_FWM_EN               0x40

/* STK83XX_REG_INTMAP1 */
#define STK83XX_INTMAP1_SIGMOT2INT1         0x01
#define STK83XX_INTMAP1_ANYMOT2INT1         0x04
#define STK83XX_INTMAP1_SIGMOT2INT2         0x80
#define STK83XX_INTMAP1_ANYMOT2INT2         0x20

/* STK83XX_REG_INTMAP2 */
#define STK83XX_INTMAP2_DATA2INT1           0x01
#define STK83XX_INTMAP2_FWM2INT1            0x02
#define STK83XX_INTMAP2_FWM2INT2            0x40
#define STK83XX_INTMAP2_DATA2INT2           0x80

/* STK83XX_REG_INTMAP3 */
#define STK83XX_INTMAP3_6D_LATCH_MODE       0x01
#define STK83XX_INTMAP3_TILT_2INT1          0x10
#define STK83XX_INTMAP3_6D_2INT1            0x20
#define STK83XX_INTMAP3_TILT_2INT2          0x40
#define STK83XX_INTMAP3_6D_2INT2            0x80

/* STK83XX_REG_SIXDCFG */
#define STK83XX_SIXDCFG_6D_CTRL_LOW_X       0x01
#define STK83XX_SIXDCFG_6D_CTRL_HIGH_X      0x02
#define STK83XX_SIXDCFG_6D_CTRL_LOW_Y       0x04
#define STK83XX_SIXDCFG_6D_CTRL_HIGH_Y      0x08
#define STK83XX_SIXDCFG_6D_CTRL_LOW_Z       0x10
#define STK83XX_SIXDCFG_6D_CTRL_HIGH_Z      0x20

#define STK83XX_SIXDCFG_6D_MODE_MASK        0x40
#define STK83XX_SIXDCFG_6D_AO_MASK          0x80

/* STK83XX_REG_SIXDTHD */
#define STK83XX_SIXDTHD_2G_DEF              0x33 // 0x33 = 800mg
#define STK83XX_SIXDTHD_4G_DEF              0x19 // 0x19 = 800mg
#define STK83XX_SIXDTHD_8G_DEF              0x0C // 0x0C = 800mg

/* STK83XX_REG_SIXDSTS */
#define STK83XX_SIXDSTS_X_L_MASK            0x01 
#define STK83XX_SIXDSTS_X_H_MASK            0x02
#define STK83XX_SIXDSTS_Y_L_MASK            0x04
#define STK83XX_SIXDSTS_Y_H_MASK            0x08
#define STK83XX_SIXDSTS_Z_L_MASK            0x10
#define STK83XX_SIXDSTS_Z_H_MASK            0x20


/* STK83XX_REG_SIXDDLY */
#define STK83XX_SIXDDLY_DEF                 0x30

/* STK83XX_REG_INTCFG1 */
#define STK83XX_INTCFG1_INT1_ACTIVE_H       0x01	// bit 0
#define STK83XX_INTCFG1_INT1_ACTIVE_L       0x00	// bit 0
#define STK83XX_INTCFG1_INT1_OD_PUSHPULL    0x00
#define STK83XX_INTCFG1_INT1_OD_OEPNDRAIN	0x02	// Open drain
#define STK83XX_INTCFG1_INT2_ACTIVE_H       0x04
#define STK83XX_INTCFG1_INT2_OD_PUSHPULL    0x00
#define STK83XX_INTCFG1_INT2_OD_OEPNDRAIN	0x08	// Open drain

/* STK83XX_REG_INTCFG2 */
#define STK83XX_INTCFG2_NOLATCHED           0x00
#define STK83XX_INTCFG2_LATCHED             0x0F
#define STK83XX_INTCFG2_INT_RST             0x80

/* STK83XX_REG_SLOPETHD */
#define STK83XX_SLOPETHD_DEF                0x14

/* STK83XX_REG_SIGMOT1 */
#define STK83XX_SIGMOT1_SKIP_TIME_3SEC      0x96	// 3 secs (default)
#define STK83XX_SIGMOT1_SKIP_TIME_0_5SEC    0x01	// 0.5 sec


/* STK83XX_REG_SIGMOT2 */
#define STK83XX_SIGMOT2_SIG_MOT_EN          0x02
#define STK83XX_SIGMOT2_ANY_MOT_EN          0x04

/* STK83XX_REG_SIGMOT3 */
#define STK83XX_SIGMOT3_PROOF_TIME_1SEC     0x32    // 1 sec (default)
#define STK83XX_SIGMOT3_PROOF_TIME_0_5SEC   0x01    // 0.5 sec

/* STK83XX_REG_STEPCNT2 */
#define STK83XX_STEPCNT2_RST_CNT            0x04
#define STK83XX_STEPCNT2_STEP_CNT_EN        0x08

/* STK83XX_REG_INTFCFG */
#define STK83XX_INTFCFG_I2C_WDT_EN          0x04

/* STK83XX_REG_OFSTCOMP1 */
#define STK83XX_OFSTCOMP1_OFST_RST          0x80

/* STK83XX_REG_CFG1 */
/* the maximum space for FIFO is 32*3 bytes */
#define STK83XX_CFG1_XYZ_FRAME_MAX          32

/* STK83XX_REG_CFG2 */
#define STK83XX_CFG2_FIFO_MODE_BYPASS       0x0
#define STK83XX_CFG2_FIFO_MODE_FIFO         0x1
#define STK83XX_CFG2_FIFO_MODE_SHIFT        5
#define STK83XX_CFG2_FIFO_DATA_SEL_XYZ      0x0
#define STK83XX_CFG2_FIFO_DATA_SEL_X        0x1
#define STK83XX_CFG2_FIFO_DATA_SEL_Y        0x2
#define STK83XX_CFG2_FIFO_DATA_SEL_Z        0x3
#define STK83XX_CFG2_FIFO_DATA_SEL_MASK     0x3

/* STK83XX_REG_OFSTx */
#define STK83XX_OFST_LSB                    128     /* 8 bits for +-1G */
/*****************************************************************************
 * stk83xx register, end
 *****************************************************************************/

#define STK83XX_NAME    "stk83xx"

typedef struct stk_data stk_data;
typedef void (*STK_READ_DATA_CB)(struct stk_data *);
typedef void (*STK_REPORT_CB)(struct stk_data *);
typedef void (*STK_READ_FIFO_CB)(struct stk_data *);
typedef void (*STK_FIFO_REPORT_CB)(struct stk_data *, s16 *, int);
#ifdef STK_AMD
    typedef void (*STK_REPORTAMD_CB)(struct stk_data *, bool);
#endif

struct stk_data
{
    const struct stk_bus_ops        *bops;
    const struct stk_timer_ops      *tops;
    const struct stk_gpio_ops       *gops;
    const struct stk_storage_ops    *sops;
    STK_READ_DATA_CB            read_data_cb;
    STK_REPORT_CB               accel_report_cb;
    STK_READ_FIFO_CB            read_fifo_cb;
    STK_FIFO_REPORT_CB          fifo_report_cb;
#ifdef STK_AMD
    STK_REPORTAMD_CB            reportamd_cb;
#endif
    atomic_t                    enabled;            /* chip is enabled or not */
    atomic_t                    selftest;           /* selftest result */
    int                         direction;
    int                         sr_no;              /* Serial number of stkODRTable */
    int                         sensitivity;        /* sensitivity, bit number per G */
    int                         bus_idx;
    s16                         xyz[3];             /* The latest data of xyz */
    u8                          power_mode;
    u8                          pid;
    u8                          data_shift;
    u8                          odr_table_count;
    u8                          recv;
    bool                        fifo;               /* Support FIFO or not */
    bool                        temp_enable;        /* record current power status. For Suspend/Resume used. */
    bool                        is_esm_mode;
#ifdef STK_CALI
    s16                         cali_sw[3];
    atomic_t                    cali_status;        /* cali status */
#ifdef STK_AUTOK
    atomic_t                    first_enable;
    u8                          offset[3];          /* offset value for STK83XX_REG_OFSTX~Z */
#endif /* STK_AUTOK */
#endif /* STK_CALI */
#ifdef STK_HW_STEP_COUNTER
    int                         steps;              /* The latest step counter value */
#endif /* STK_HW_STEP_COUNTER */
#ifdef STK_INTERRUPT_MODE
    stk_gpio_info                   gpio_info;
#elif defined STK_POLLING_MODE
    stk_timer_info                  stk_timer_info;
#endif /* STK_INTERRUPT_MODE, STK_POLLING_MODE */
#ifdef STK_FIR
    struct data_fir             fir;
    /*
     * fir_len
     * 0: turn OFF FIR operation
     * 1 ~ STK_FIR_LEN_MAX: turn ON FIR operation
     */
    atomic_t                    fir_len;
#endif /* STK_FIR */
};

struct stk83xx_platform_data
{
    unsigned char   direction;
    int             interrupt_int1_pin;
};

#define STK_ACC_TAG                 "[stkAccel]"
#define STK_ACC_FUN(f)              printk(KERN_INFO STK_ACC_TAG" %s\n", __FUNCTION__)
#define STK_ACC_LOG(fmt, args...)   printk(KERN_INFO STK_ACC_TAG" %s/%d: "fmt"\n", __FUNCTION__, __LINE__, ##args)
#define STK_ACC_ERR(fmt, args...)   printk(KERN_ERR STK_ACC_TAG" %s/%d: "fmt"\n", __FUNCTION__, __LINE__, ##args)

#define STK_REG_READ(stk_data, reg, val)                    ((stk_data)->bops->read((stk_data)->bus_idx, reg, val))
#define STK_REG_READ_BLOCK(stk_data, reg, count, buf)       ((stk_data)->bops->read_block((stk_data)->bus_idx, reg, count, buf))
#define STK_REG_WRITE(stk_data, reg, val)                   ((stk_data)->bops->write((stk_data)->bus_idx, reg, val))
#define STK_REG_WRITE_BLOCK(stk_data, reg, val, len)        ((stk_data)->bops->write_block((stk_data)->bus_idx, reg, val, len))
#define STK_REG_READ_MODIFY_WRITE(stk_data, reg, val, mask) ((stk_data)->bops->read_modify_write((stk_data)->bus_idx, reg, val, mask))

#define STK_TIMER_REGISTER(stk_data, t_info)                ((stk_data)->tops->register_timer(t_info))
#define STK_TIMER_START(stk_data, t_info)                   ((stk_data)->tops->start_timer(t_info))
#define STK_TIMER_STOP(stk_data, t_info)                    ((stk_data)->tops->stop_timer(t_info))
#define STK_TIMER_REMOVE(stk_data, t_info)                  ((stk_data)->tops->remove(t_info))
#define STK_TIMER_BUSY_WAIT(stk_data, min, max, mode)       ((stk_data)->tops->busy_wait(min, max, mode))

#define STK_GPIO_IRQ_REGISTER(stk_data, g_info)             ((stk_data)->gops->register_gpio_irq(g_info))
#define STK_GPIO_IRQ_START(stk_data, g_info)                ((stk_data)->gops->start_gpio_irq(g_info))
#define STK_GPIO_IRQ_STOP(stk_data, g_info)                 ((stk_data)->gops->stop_gpio_irq(g_info))
#define STK_GPIO_IRQ_REMOVE(stk_data, g_info)               ((stk_data)->gops->remove(g_info))

#define STK_STORAGE_INIT(stk_data)                          ((stk_data)->sops->init_storage())
#define STK_W_TO_STORAGE(stk_data, name, buf, size)         ((stk_data)->sops->write_to_storage(name, buf, size))
#define STK_R_FROM_STORAGE(stk_data, name, buf, size)       ((stk_data)->sops->read_from_storage(name, buf, size))
#define STK_STORAGE_REMOVE(stk_data)                        ((stk_data)->sops->remove())

#define STK83XX_READ_DATA_CB(stk_data)                      if ((stk_data)->read_data_cb)           ((stk_data)->read_data_cb(stk_data))
#ifdef STK_AMD
    #define STK83XX_AMD_REPORT(stk_data, flag)                  if ((stk_data)->reportamd_cb)           ((stk_data)->reportamd_cb(stk_data, flag))
#endif
#define STK83XX_ACCEL_REPORT(stk_data)                      if ((stk_data)->accel_report_cb)        ((stk_data)->accel_report_cb(stk_data))
#define STK83XX_READ_FIFO_CB(stk_data)                      if ((stk_data)->read_fifo_cb)           ((stk_data)->read_fifo_cb(stk_data))
#define STK83XX_FIFO_REPORT(stk_data, xyz, len)             if ((stk_data)->fifo_report_cb)         ((stk_data)->fifo_report_cb(stk_data, xyz, len))
#define STK83XX_REQUEST_REGISTRY(stk_data)                  if ((stk_data)->request_registry_cb)    ((stk_data)->request_registry_cb(stk_data))

typedef struct
{
    uint8_t     regBwsel;
    uint8_t     regPwsel;
    int         sample_rate_us;
    int         drop;
} _stkOdrMap;

#define STK_MIN_DELAY_US 2000   /* stkODRTable[4].sample_rate_us */
#define STK_MAX_DELAY_US 32000  /* stkODRTable[0].sample_rate_us */

/* selftest usage */
#define STK_SELFTEST_SAMPLE_NUM             100
#define STK_SELFTEST_RESULT_NA              0
#define STK_SELFTEST_RESULT_RUNNING         (1 << 0)
#define STK_SELFTEST_RESULT_NO_ERROR        (1 << 1)
#define STK_SELFTEST_RESULT_DRIVER_ERROR    (1 << 2)
#define STK_SELFTEST_RESULT_FAIL_X          (1 << 3)
#define STK_SELFTEST_RESULT_FAIL_Y          (1 << 4)
#define STK_SELFTEST_RESULT_FAIL_Z          (1 << 5)
#define STK_SELFTEST_RESULT_NO_OUTPUT       (1 << 6)
static inline int stk_selftest_offset_factor(int sen)
{
    return sen * 3 / 10;
}
static inline int stk_selftest_noise_factor(int sen)
{
    return sen / 10;
}

#ifdef STK_CALI
    /* calibration parameters */
    #define STK_CALI_SAMPLE_NO          10
    #ifdef STK_CALI_FILE
        #define STK_CALI_VER0               0x18
        #define STK_CALI_VER1               0x03
        #define STK_CALI_END                '\0'
        #define STK_CALI_FILE_PATH          "/data/misc/stkacccali.conf"
        #define STK_CALI_FILE_SIZE          25
    #endif /* STK_CALI_FILE */
    /* parameter for cali_status/atomic_t and cali file */
    #define STK_K_SUCCESS_FILE          0x01
    /* parameter for cali_status/atomic_t */
    #define STK_K_FAIL_WRITE_OFST       0xF2
    #define STK_K_FAIL_I2C              0xF8
    #define STK_K_FAIL_W_FILE           0xFB
    #define STK_K_FAIL_VERIFY_CALI      0xFD
    #define STK_K_RUNNING               0xFE
    #define STK_K_NO_CALI               0xFF
#endif /* STK_CALI */

int stk_get_delay(struct stk_data *stk);
int stk_set_delay(struct stk_data *stk, int delay_us);
int stk_get_offset(struct stk_data *stk, u8 offset[3]);
int stk_set_offset(struct stk_data *stk, u8 offset[3]);
void stk_set_enable(struct stk_data *stk, char en);
int stk_get_pid(struct stk_data *stk);
void stk_data_initialize(struct stk_data *stk);
int stk_range_selection(struct stk_data *stk, stk_rangesel range);
void stk_fifo_reading(struct stk_data *stk, u8 fifo[], int len);
int stk_change_fifo_status(struct stk_data *stk, u8 wm);
int stk_read_accel_rawdata(struct stk_data *stk);
int stk_reg_init(struct stk_data *stk, stk_rangesel range, int sr_no);
void stk_selftest(struct stk_data *stk);
void stk_read_accel_data(struct stk_data *stk);
#if defined STK_INTERRUPT_MODE || defined STK_POLLING_MODE
    void stk_read_then_report_fifo_data(struct stk_data *stk);
    #ifdef STK_INTERRUPT_MODE
        void stk_work_queue(stk_gpio_info *gpio_info);
    #elif defined STK_POLLING_MODE
        void stk_work_queue(stk_timer_info *t_info);
    #endif
#endif /* defined STK_INTERRUPT_MODE || defined STK_POLLING_MODE */
#ifdef STK_CALI
    #ifdef STK_CALI_FILE
        void stk_get_cali(struct stk_data *stk);
    #endif /* STK_CALI_FILE */
    void stk_set_cali(struct stk_data *stk);
    void stk_reset_cali(struct stk_data *stk);
#endif /* STK_CALI */
#ifdef STK_HW_STEP_COUNTER
    void stk_read_step_data(struct stk_data *stk);
    void stk_turn_step_counter(struct stk_data *stk, bool turn);
#endif /* STK_HW_STEP_COUNTER */
#ifdef STK_FIR
    void stk_low_pass_fir(struct stk_data *stk, s16 *xyz);
#endif /* STK_FIR */

extern const int coordinate_trans[8][3][3];

#endif /* __STK83XX_H__ */
