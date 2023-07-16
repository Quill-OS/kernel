#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/errno.h>
#include <linux/wakelock.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/vmalloc.h>
#include <asm/uaccess.h>
#include "stk83xx.h"
#include "stk83xx_qualcomm.h"

#ifdef QCOM_SENSORS
	#include <linux/sensors.h>
#endif
#ifdef CONFIG_OF
    #include <linux/of_gpio.h>
#endif
#include "../../../arch/arm/mach-imx/ntx_hwconfig.h"
extern volatile NTX_HWCONFIG *gptHWCFG;
extern int gStk83xx_int;
extern int gSleep_Mode_Suspend;
static void stk_set_fifo_start_time(struct stk_data *stk);

struct attribute_group stk_attribute_accel_group;
struct stk83xx_wrapper *g_stk83xx_data;
/*
 * @brief: Read all register (0x0 ~ 0x3F)
 *
 * @param[in/out] stk: struct stk_data *
 * @param[out] show_buffer: record all register value
 *
 * @return: buffer length or fail
 *          positive value: return buffer length
 *          -1: Fail
 */
static int stk_show_all_reg(stk_data *stk, char *show_buffer)
{
    unsigned short reg;
    int len = 0;
    int err = 0;
    u8 data = 0;

    if (NULL == show_buffer)
        return -1;

    for (reg = 0; reg <= 0x3F; reg++)
    {
        err = STK_REG_READ(stk, reg, &data);

        if (err < 0)
        {
            len = -1;
            goto exit;
        }

        if (0 >= (PAGE_SIZE - len))
        {
            STK_ACC_ERR("print string out of PAGE_SIZE");
            goto exit;
        }

        len += scnprintf(show_buffer + len, PAGE_SIZE - len,
                         "[0x%2X]=0x%2X, ", reg, data);

        if (4 == reg % 5)
        {
            len += scnprintf(show_buffer + len, PAGE_SIZE - len,
                             "\n");
        }
    }

    len += scnprintf(show_buffer + len, PAGE_SIZE - len, "\n");
exit:
    return len;
}

/**
 * @brief: Get power status
 *          Send 0 or 1 to userspace.
 *
 * @param[in] dev: struct device *
 * @param[in] attr: struct device_attribute *
 * @param[in/out] buf: char *
 *
 * @return: ssize_t
 */
static ssize_t stk_enable_show(struct device *dev,
                               struct device_attribute *attr, char *buf)
{
    stk83xx_wrapper *stk_wrapper = dev_get_drvdata(dev);
    stk_data *stk = &stk_wrapper->stk;
    char en;
    en = atomic_read(&stk->enabled);
    return scnprintf(buf, PAGE_SIZE, "%d\n", en);
}

/**
 * @brief: Set power status
 *          Get 0 or 1 from userspace, then set stk83xx power status.
 *
 * @param[in] dev: struct device *
 * @param[in] attr: struct device_attribute *
 * @param[in/out] buf: char *
 * @param[in] count: size_t
 *
 * @return: ssize_t
 */
static ssize_t stk_enable_store(struct device *dev,
                                struct device_attribute *attr, const char *buf, size_t count)
{
    stk83xx_wrapper *stk_wrapper = dev_get_drvdata(dev);
    stk_data *stk = &stk_wrapper->stk;
    unsigned int data;
    int error;
    error = kstrtouint(buf, 10, &data);

    if (error)
    {
        STK_ACC_ERR("kstrtoul failed, error=%d", error);
        return error;
    }

    if ((1 == data) || (0 == data))
        stk_set_enable(stk, data);
    else
        STK_ACC_ERR("invalid argument, en=%d", data);

    return count;
}

/**
 * @brief: Get accel data
 *          Send accel data to userspce.
 *
 * @param[in] dev: struct device *
 * @param[in] attr: struct device_attribute *
 * @param[in/out] buf: char *
 *
 * @return: ssize_t
 */
static ssize_t stk_value_show(struct device *dev,
                              struct device_attribute *attr, char *buf)
{
    stk83xx_wrapper *stk_wrapper = dev_get_drvdata(dev);
    stk_data *stk = &stk_wrapper->stk;
    bool enable = true;

    if (!atomic_read(&stk->enabled)) {
        stk_set_enable(stk, 1);
        enable = false;
    }

    stk_read_accel_data(stk);

    if (!enable){
        stk_set_enable(stk, 0);
	}

    return scnprintf(buf, PAGE_SIZE, "%hd %hd %hd\n",
                     stk->xyz[0], stk->xyz[1], stk->xyz[2]);
}

/**
 * @brief: Get delay value in usec
 *          Send delay in usec to userspce.
 *
 * @param[in] dev: struct device *
 * @param[in] attr: struct device_attribute *
 * @param[in/out] buf: char *
 *
 * @return: ssize_t
 */
static ssize_t stk_delay_show(struct device *dev,
                              struct device_attribute *attr, char *buf)
{
    stk83xx_wrapper *stk_wrapper = dev_get_drvdata(dev);
    stk_data *stk = &stk_wrapper->stk;
    return scnprintf(buf, PAGE_SIZE, "%lld\n", (long long)stk_get_delay(stk) * 1000);
}

/**
 * @brief: Set delay value in usec
 *          Get delay value in usec from userspace, then write to register.
 *
 * @param[in] dev: struct device *
 * @param[in] attr: struct device_attribute *
 * @param[in/out] buf: char *
 * @param[in] count: size_t
 *
 * @return: ssize_t
 */
static ssize_t stk_delay_store(struct device *dev,
                               struct device_attribute *attr, const char *buf, size_t count)
{
    stk83xx_wrapper *stk_wrapper = dev_get_drvdata(dev);
    stk_data *stk = &stk_wrapper->stk;
    long long data;
    int err;
#if 0
    err = kstrtoll(buf, 10, &data);

    if (err)
    {
        STK_ACC_ERR("kstrtoul failed, error=%d", err);
        return err;
    }

    stk_set_delay(stk, (int)(data / 1000));
#endif
    return count;
}

/**
 * @brief: Get offset value
 *          Send X/Y/Z offset value to userspace.
 *
 * @param[in] dev: struct device *
 * @param[in] attr: struct device_attribute *
 * @param[in/out] buf: char *
 *
 * @return: ssize_t
 */
static ssize_t stk_offset_show(struct device *dev,
                               struct device_attribute *attr, char *buf)
{
    stk83xx_wrapper *stk_wrapper = dev_get_drvdata(dev);
    stk_data *stk = &stk_wrapper->stk;
    u8 offset[3] = {0, 0, 0};
    stk_get_offset(stk, offset);
    return scnprintf(buf, PAGE_SIZE, "0x%X 0x%X 0x%X\n",
                     offset[0], offset[1], offset[2]);
}

/**
 * @brief: Set offset value
 *          Get X/Y/Z offset value from userspace, then write to register.
 *
 * @param[in] dev: struct device *
 * @param[in] attr: struct device_attribute *
 * @param[in/out] buf: char *
 * @param[in] count: size_t
 *
 * @return: ssize_t
 */
static ssize_t stk_offset_store(struct device *dev,
                                struct device_attribute *attr, const char *buf, size_t count)
{
    stk83xx_wrapper *stk_wrapper = dev_get_drvdata(dev);
    stk_data *stk = &stk_wrapper->stk;
    char *token[10];
    u8 r_offset[3];
    int err, data, i;

    for (i = 0; i < 3; i++)
        token[i] = strsep((char **)&buf, " ");

    err = kstrtoint(token[0], 16, &data);

    if (err)
    {
        STK_ACC_ERR("kstrtoint failed, error=%d", err);
        return err;
    }

    r_offset[0] = (u8)data;
    err = kstrtoint(token[1], 16, &data);

    if (err)
    {
        STK_ACC_ERR("kstrtoint failed, error=%d", err);
        return err;
    }

    r_offset[1] = (u8)data;
    err = kstrtoint(token[2], 16, &data);

    if (err)
    {
        STK_ACC_ERR("kstrtoint failed, error=%d", err);
        return err;
    }

    r_offset[2] = (u8)data;
    STK_ACC_LOG("offset=0x%X, 0x%X, 0x%X", r_offset[0], r_offset[1], r_offset[2]);
    stk_set_offset(stk, r_offset);
    return count;
}

/**
 * @brief: Register writting
 *          Get address and content from userspace, then write to register.
 *
 * @param[in] dev: struct device *
 * @param[in] attr: struct device_attribute *
 * @param[in/out] buf: char *
 * @param[in] count: size_t
 *
 * @return: ssize_t
 */
static ssize_t stk_send_store(struct device *dev,
                              struct device_attribute *attr, const char *buf, size_t count)
{
    stk83xx_wrapper *stk_wrapper = dev_get_drvdata(dev);
    stk_data *stk = &stk_wrapper->stk;
    char *token[10];
    int addr, cmd, err, i;
    bool enable = false;

    for (i = 0; i < 2; i++)
        token[i] = strsep((char **)&buf, " ");

    err = kstrtoint(token[0], 16, &addr);

    if (err)
    {
        STK_ACC_ERR("kstrtoint failed, err=%d", err);
        return err;
    }

    err = kstrtoint(token[1], 16, &cmd);

    if (err)
    {
        STK_ACC_ERR("kstrtoint failed, err=%d", err);
        return err;
    }

    STK_ACC_LOG("write reg[0x%X]=0x%X", addr, cmd);

    if (!atomic_read(&stk->enabled)){
        stk_set_enable(stk, 1);
	}
    else
        enable = true;

    if (STK_REG_WRITE(stk, (u8)addr, (u8)cmd))
    {
        err = -1;
        goto exit;
    }

exit:

    if (!enable){
        stk_set_enable(stk, 0);
	}

    if (err)
        return -1;

    return count;
}

/**
 * @brief: Read stk_data.recv(from stk_recv_store), then send to userspace.
 *
 * @param[in] dev: struct device *
 * @param[in] attr: struct device_attribute *
 * @param[in/out] buf: char *
 *
 * @return: ssize_t
 */
static ssize_t stk_recv_show(struct device *dev,
                             struct device_attribute *attr, char *buf)
{
    stk83xx_wrapper *stk_wrapper = dev_get_drvdata(dev);
    stk_data *stk = &stk_wrapper->stk;
    return scnprintf(buf, PAGE_SIZE, "0x%X\n", stk->recv);
}

/**
 * @brief: Get the read address from userspace, then store the result to
 *          stk_data.recv.
 *
 * @param[in] dev: struct device *
 * @param[in] attr: struct device_attribute *
 * @param[in/out] buf: char *
 * @param[in] count: size_t
 *
 * @return: ssize_t
 */
static ssize_t stk_recv_store(struct device *dev,
                              struct device_attribute *attr, const char *buf, size_t count)
{
    stk83xx_wrapper *stk_wrapper = dev_get_drvdata(dev);
    stk_data *stk = &stk_wrapper->stk;
    int addr, err;
    bool enable = false;
    err = kstrtoint(buf, 16, &addr);

    if (err)
    {
        STK_ACC_ERR("kstrtoint failed, error=%d", err);
        return err;
    }

    if (!atomic_read(&stk->enabled)){
        stk_set_enable(stk, 1);
	}
    else
        enable = true;

    err = STK_REG_READ(stk, (u8)addr, &stk->recv);

    if (err < 0)
    {
        goto exit;
    }

    STK_ACC_LOG("read reg[0x%X]=0x%X", addr, stk->recv);
exit:

    if (!enable){
        stk_set_enable(stk, 0);
	}
    if (err)
        return -1;

    return count;
}

/**
 * @brief: Read all register value, then send result to userspace.
 *
 * @param[in] dev: struct device *
 * @param[in] attr: struct device_attribute *
 * @param[in/out] buf: char *
 *
 * @return: ssize_t
 */
static ssize_t stk_allreg_show(struct device *dev,
                               struct device_attribute *attr, char *buf)
{
    stk83xx_wrapper *stk_wrapper = dev_get_drvdata(dev);
    stk_data *stk = &stk_wrapper->stk;
    int result;
    result = stk_show_all_reg(stk, buf);

    if (0 > result)
        return result;

    return (ssize_t)result;
}

/**
 * @brief: Check PID, then send chip number to userspace.
 *
 * @param[in] dev: struct device *
 * @param[in] attr: struct device_attribute *
 * @param[in/out] buf: char *
 *
 * @return: ssize_t
 */
static ssize_t stk_chipinfo_show(struct device *dev,
                                 struct device_attribute *attr, char *buf)
{
    stk83xx_wrapper *stk_wrapper = dev_get_drvdata(dev);
    stk_data *stk = &stk_wrapper->stk;

    switch (stk->pid)
    {
        case STK8BA50_R_ID:
            return scnprintf(buf, PAGE_SIZE, "stk8ba50-r\n");

        case STK8BA53_ID:
            return scnprintf(buf, PAGE_SIZE, "stk8ba53\n");

        case STK8323_ID:
            return scnprintf(buf, PAGE_SIZE, "stk8321/8323\n");

        case STK8327_ID:
            return scnprintf(buf, PAGE_SIZE, "stk8327\n");

        case STK8329_ID:
            return scnprintf(buf, PAGE_SIZE, "stk8329\n");

        default:
            return scnprintf(buf, PAGE_SIZE, "unknown\n");
    }

    return scnprintf(buf, PAGE_SIZE, "unknown\n");
}

/**
 * @brief: Read FIFO data, then send to userspace.
 *
 * @param[in] dev: struct device *
 * @param[in] attr: struct device_attribute *
 * @param[in/out] buf: char *
 *
 * @return: ssize_t
 */
static ssize_t stk_fifo_show(struct device *dev,
                             struct device_attribute *attr, char *buf)
{
    stk83xx_wrapper *stk_wrapper = dev_get_drvdata(dev);
    stk_data *stk = &stk_wrapper->stk;
    u8 fifo_wm = 0;
    u8 frame_unit = 0;
    int fifo_len, len = 0, err = 0;

    if (!stk->fifo)
    {
        return scnprintf(buf, PAGE_SIZE, "No fifo data\n");
    }

    err = STK_REG_READ(stk, STK83XX_REG_FIFOSTS, &fifo_wm);

    if (err < 0)
        return scnprintf(buf, PAGE_SIZE, "fail to read FIFO cnt\n");

    fifo_wm = (fifo_wm) & STK83XX_FIFOSTS_FIFO_FRAME_CNT_MASK;

    if (0 == fifo_wm)
        return scnprintf(buf, PAGE_SIZE, "no fifo data yet\n");

    err = STK_REG_READ(stk, STK83XX_REG_CFG2, &frame_unit);

    if (err < 0)
        return scnprintf(buf, PAGE_SIZE, "fail to read FIFO\n");

    frame_unit = (frame_unit) & STK83XX_CFG2_FIFO_DATA_SEL_MASK;

    if (0 == frame_unit)
        fifo_len = fifo_wm * 6; /* xyz * 2 bytes/axis */
    else
        fifo_len = fifo_wm * 2; /* single axis * 2 bytes/axis */

    {
        u8 *fifo = NULL;
        int i;
        /* vzalloc: allocate memory and set to zero. */
        fifo = vzalloc(sizeof(u8) * fifo_len);

        if (!fifo)
        {
            STK_ACC_ERR("memory allocation error");
            return scnprintf(buf, PAGE_SIZE, "fail to read FIFO\n");
        }
        stk_fifo_reading(stk, fifo, fifo_len);
        for (i = 0; i < fifo_wm; i++)
        {
            if (0 == frame_unit)
            {
                s16 x, y, z;
                x = fifo[i * 6 + 1] << 8 | fifo[i * 6];
                x >>= 4;
                y = fifo[i * 6 + 3] << 8 | fifo[i * 6 + 2];
                y >>= 4;
                z = fifo[i * 6 + 5] << 8 | fifo[i * 6 + 4];
                z >>= 4;
                len += scnprintf(buf + len, PAGE_SIZE - len,
                                 "%dth x:%d, y:%d, z:%d\n", i, x, y, z);
            }
            else
            {
                s16 xyz;
                xyz = fifo[i * 2 + 1] << 8 | fifo[i * 2];
                xyz >>= 4;
                len += scnprintf(buf + len, PAGE_SIZE - len,
                                 "%dth fifo:%d\n", i, xyz);
            }

            if ( 0 >= (PAGE_SIZE - len))
            {
                STK_ACC_ERR("print string out of PAGE_SIZE");
                break;
            }
        }

        vfree(fifo);
    }
    return len;
}

/**
 * @brief: Read water mark from userspace, then send to register.
 *
 * @param[in] dev: struct device *
 * @param[in] attr: struct device_attribute *
 * @param[in/out] buf: char *
 * @param[in] count: size_t
 *
 * @return: ssize_t
 */
static ssize_t stk_fifo_store(struct device *dev,
                              struct device_attribute *attr, const char *buf, size_t count)
{
    stk83xx_wrapper *stk_wrapper = dev_get_drvdata(dev);
    stk_data *stk = &stk_wrapper->stk;
    int wm, err;

    if (!stk->fifo)
    {
        STK_ACC_ERR("not support fifo");
        return count;
    }

    err = kstrtoint(buf, 10, &wm);

    if (err)
    {
        STK_ACC_ERR("kstrtoint failed, error=%d", err);
        return err;
    }

    if (stk_change_fifo_status(stk, (u8)wm))
    {
        return -1;
    }

    return count;
}

/**
 * @brief: Show self-test result.
 *
 * @param[in] dev: struct device *
 * @param[in] attr: struct device_attribute *
 * @param[in/out] buf: char *
 *
 * @return: ssize_t
 */
static ssize_t stk_selftest_show(struct device *dev,
                                 struct device_attribute *attr, char *buf)
{
    stk83xx_wrapper *stk_wrapper = dev_get_drvdata(dev);
    stk_data *stk = &stk_wrapper->stk;
    u8 result = atomic_read(&stk->selftest);

    if (STK_SELFTEST_RESULT_NA == result)
        return scnprintf(buf, PAGE_SIZE, "No result\n");

    if (STK_SELFTEST_RESULT_RUNNING == result)
        return scnprintf(buf, PAGE_SIZE, "selftest is running\n");
    else if (STK_SELFTEST_RESULT_NO_ERROR == result)
        return scnprintf(buf, PAGE_SIZE, "No error\n");
    else
        return scnprintf(buf, PAGE_SIZE, "Error code:0x%2X\n", result);
}

/**
 * @brief: Do self-test.
 *
 * @param[in] dev: struct device *
 * @param[in] attr: struct device_attribute *
 * @param[in/out] buf: char *
 * @param[in] count: size_t
 *
 * @return: ssize_t
 */
static ssize_t stk_selftest_store(struct device *dev,
                                  struct device_attribute *attr, const char *buf, size_t count)
{
    stk83xx_wrapper *stk_wrapper = dev_get_drvdata(dev);
    stk_data *stk = &stk_wrapper->stk;
    stk_selftest(stk);
    return count;
}

/**
 * @brief: Get range value
 *          Send range to userspce.
 *
 * @param[in] ddri: struct device_driver *
 * @param[in/out] buf: char *
 *
 * @return: ssize_t
 */
static ssize_t stk_range_show(struct device *dev,
                              struct device_attribute *attr, char *buf)
{
    stk83xx_wrapper *stk_wrapper = dev_get_drvdata(dev);
    stk_data *stk = &stk_wrapper->stk;

    switch (stk->sensitivity)
    {
        //2G , for 16it , 12 bit
        // 2^16 / 2x2 , 2^12/2x2
        case 16384:
        case 1024:
            return scnprintf(buf, PAGE_SIZE, "2\n");

        //4G , for 16it , 12 bit,10bit
        //2^16 / 2x4 , 2^12/2x4, 2^10/2x4
        case 8192:
        case 512:
        case 128:
            return scnprintf(buf, PAGE_SIZE, "4\n");

        //8G , for 16it ,10bit
        //2^16 / 2x8 , 2^10/2x8
        case 4096:
        case 64:
            return scnprintf(buf, PAGE_SIZE, "8\n");

        //2G, 10bit  ,8G,12bit
        //2^10/2x2 , 2^12/2x8
        case 256:
            if (stk->pid == STK8BA50_R_ID)
                return scnprintf(buf, PAGE_SIZE, "2\n");
            else
                return scnprintf(buf, PAGE_SIZE, "8\n");

        //16G , for 16it
        //2^16 / 2x16
        case 2048:
            return scnprintf(buf, PAGE_SIZE, "16\n");

        default:
            return scnprintf(buf, PAGE_SIZE, "unkown\n" );
    }

    return scnprintf(buf, PAGE_SIZE, "unkown\n");
}

/**
 * @brief: Set range value
 *         Get range value from userspace, then write to register.
 *
 * @param[in] dev: struct device *
 * @param[in] attr: struct device_attribute *
 * @param[in/out] buf: char *
 * @param[in] count: size_t
 *
 * @return: ssize_t
 */
static ssize_t stk_range_store(struct device *dev,
                               struct device_attribute *attr, const char *buf, size_t count)
{
    stk83xx_wrapper *stk_wrapper = dev_get_drvdata(dev);
    stk_data *stk = &stk_wrapper->stk;
    long long data;
    int err;
    stk_rangesel range;
    err = kstrtoll(buf, 10, &data);

    if (err)
    {
        STK_ACC_ERR("kstrtoul failed, error=%d", err);
        return err;
    }

    if (stk->pid != STK8327_ID)
    {
        if (data == 16)
        {
            STK_ACC_LOG(" This chip not support 16G,auto switch to 8G");
            data = 8;
        }
    }

    switch (data)
    {
        case 2:
        default:
            range = STK83XX_RANGESEL_2G;
            break;

        case 4:
            range = STK83XX_RANGESEL_4G;
            break;

        case 8:
            range = STK83XX_RANGESEL_8G;
            break;

        case 16:
            range = STK83XX_RANGESEL_16G;
            break;
    }

    stk_range_selection(stk, range);
    return count;
}

#ifdef STK_CALI
/**
 * @brief: Get calibration status
 *          Send calibration status to userspace.
 *
 * @param[in] dev: struct device *
 * @param[in] attr: struct device_attribute *
 * @param[in/out] buf: char *
 *
 * @return: ssize_t
 */
static ssize_t stk_cali_show(struct device *dev,
                             struct device_attribute *attr, char *buf)
{
    stk83xx_wrapper *stk_wrapper = dev_get_drvdata(dev);
    stk_data *stk = &stk_wrapper->stk;
    int result = atomic_read(&stk->cali_status);
#ifdef STK_CALI_FILE

    if (STK_K_RUNNING != result)
    {
        stk_get_cali(stk);
    }

#endif /* STK_CALI_FILE */
    return scnprintf(buf, PAGE_SIZE, "0x%X\n", result);
}

/**
 * @brief: Trigger to calculate calibration data
 *          Get 1 from userspace, then start to calculate calibration data.
 *
 * @param[in] dev: struct device *
 * @param[in] attr: struct device_attribute *
 * @param[in/out] buf: char *
 * @param[in] count: size_t
 *
 * @return: ssize_t
 */
static ssize_t stk_cali_store(struct device *dev,
                              struct device_attribute *attr, const char *buf, size_t count)
{
    stk83xx_wrapper *stk_wrapper = dev_get_drvdata(dev);
    stk_data *stk = &stk_wrapper->stk;

    if (sysfs_streq(buf, "1"))
    {
        stk_set_cali(stk);
    }
    else
    {
        STK_ACC_ERR("invalid value %d", *buf);
        return -EINVAL;
    }

    return count;
}
#endif /* STK_CALI */

#ifdef STK_HW_STEP_COUNTER
/**
 * @brief: Read step counter data, then send to userspace.
 *
 * @param[in] dev: struct device *
 * @param[in] attr: struct device_attribute *
 * @param[in/out] buf: char *
 *
 * @return: ssize_t
 */
static ssize_t stk_stepcount_show(struct device *dev,
                                  struct device_attribute *attr, char *buf)
{
    stk83xx_wrapper *stk_wrapper = dev_get_drvdata(dev);
    stk_data *stk = &stk_wrapper->stk;

    if (STK8323_ID != stk->pid)
    {
        STK_ACC_ERR("not support step counter");
        return scnprintf(buf, PAGE_SIZE, "Not support\n");
    }

    stk_read_step_data(stk);
    return scnprintf(buf, PAGE_SIZE, "%d\n", stk->steps);
}

/**
 * @brief: Read step counter settins from userspace, then send to register.
 *
 * @param[in] dev: struct device *
 * @param[in] attr: struct device_attribute *
 * @param[in/out] buf: char *
 * @param[in] count: size_t
 *
 * @return: ssize_t
 */
static ssize_t stk_stepcount_store(struct device *dev,
                                   struct device_attribute *attr, const char *buf, size_t count)
{
    stk83xx_wrapper *stk_wrapper = dev_get_drvdata(dev);
    stk_data *stk = &stk_wrapper->stk;
    int step, err;

    if (STK8323_ID != stk->pid)
    {
        STK_ACC_ERR("not support step counter");
        return count;
    }

    err = kstrtoint(buf, 10, &step);

    if (err)
    {
        STK_ACC_ERR("kstrtoint failed, err=%d", err);
        return err;
    }

    if (step)
        stk_turn_step_counter(stk, true);
    else
        stk_turn_step_counter(stk, false);

    return count;
}
#endif /* STK_HW_STEP_COUNTER */

#ifdef STK_FIR
/**
 * @brief: Get FIR parameter, then send to userspace.
 *
 * @param[in] dev: struct device *
 * @param[in] attr: struct device_attribute *
 * @param[in/out] buf: char *
 *
 * @return: ssize_t
 */
static ssize_t stk_firlen_show(struct device *dev,
                               struct device_attribute *attr, char *buf)
{
    stk83xx_wrapper *stk_wrapper = dev_get_drvdata(dev);
    stk_data *stk = &stk_wrapper->stk;
    int len = atomic_read(&stk->fir_len);

    if (len)
    {
        STK_ACC_LOG("FIR count=%2d, idx=%2d", stk->fir.count, stk->fir.idx);
        STK_ACC_LOG("sum = [\t%d \t%d \t%d]",
                    stk->fir.sum[0], stk->fir.sum[1], stk->fir.sum[2]);
        STK_ACC_LOG("avg = [\t%d \t%d \t%d]",
                    stk->fir.sum[0] / len, stk->fir.sum[1] / len, stk->fir.sum[2] / len);
    }

    return scnprintf(buf, PAGE_SIZE, "%d\n", len);
}

/**
 * @brief: Get FIR length from userspace, then write to stk_data.fir_len.
 *
 * @param[in] dev: struct device *
 * @param[in] attr: struct device_attribute *
 * @param[in/out] buf: char *
 * @param[in] count: size_t
 *
 * @return: ssize_t
 */
static ssize_t stk_firlen_store(struct device *dev,
                                struct device_attribute *attr, const char *buf, size_t count)
{
    stk83xx_wrapper *stk_wrapper = dev_get_drvdata(dev);
    stk_data *stk = &stk_wrapper->stk;
    int firlen, err;
    err = kstrtoint(buf, 10, &firlen);

    if (err)
    {
        STK_ACC_ERR("kstrtoint failed, error=%d", err);
        return err;
    }

    if (STK_FIR_LEN_MAX < firlen)
    {
        STK_ACC_ERR("maximum FIR length is %d", STK_FIR_LEN_MAX);
    }
    else
    {
        memset(&stk->fir, 0, sizeof(struct data_fir));
        atomic_set(&stk->fir_len, firlen);
    }

    return count;
}
#endif /* STK_FIR */



enum {
	EBRMAIN_UNKOWN=0,
	EBRMAIN_UP = 1,
	EBRMAIN_DWON = 2,
	EBRMAIN_RIGHT = 3,
	EBRMAIN_LEFT = 4 ,
	EBRMAIN_FACE_UP =5 ,
	EBRMAIN_FACE_DOWN = 6
};

static ssize_t stk_sixdthd_show(struct device *dev,
                               struct device_attribute *attr, char *buf)
{
	stk83xx_wrapper *stk_wrapper = dev_get_drvdata(dev);
	stk_data *stk = &stk_wrapper->stk;
	int ret = 0;
	u8 data = 0;

	//stk_set_enable(stk, 0);
	ret = STK_REG_READ(stk, STK83XX_REG_SIXDTHD, &data);
	if(ret<0)
		printk("[%s_%d] Read Failed !!\n",__FUNCTION__,__LINE__);
	printk("STK83XX_REG_SIXDTHD =0x%x", data);

	return sprintf(buf, "%d\n", data);
}

static ssize_t stk_sixdthd_store(struct device *dev,
                                struct device_attribute *attr, const char *buf, size_t count)
{
	stk83xx_wrapper *stk_wrapper = dev_get_drvdata(dev);
	stk_data *stk = &stk_wrapper->stk;
	int ret = 0;
	unsigned long value;
	char *pHead = NULL;
	u8 data = 0;

	if( pHead = strstr(buf,"0x") )
		value = simple_strtol(buf, NULL, 16);
	else
		value = simple_strtoul(buf, NULL, 10);

	ret = STK_REG_WRITE(stk, STK83XX_REG_SIXDTHD, value);
	if(ret<0)
		printk("[%s_%d] Set Failed !!\n",__FUNCTION__,__LINE__);

	ret = STK_REG_READ(stk, STK83XX_REG_SIXDTHD, &data);
	printk("STK83XX_REG_SIXDTHD =0x%x", data);

	return count;
}

static ssize_t stk_tilt_angle_show(struct device *dev,
                               struct device_attribute *attr, char *buf)
{
	stk83xx_wrapper *stk_wrapper = dev_get_drvdata(dev);
	stk_data *stk = &stk_wrapper->stk;
	int ret = 0;
	u8 data = 0;

	//stk_set_enable(stk, 0);
	ret = STK_REG_READ(stk, STK83XX_REG_TILTTHD, &data);
	if(ret<0)
		printk("[%s_%d] Read Failed !!\n",__FUNCTION__,__LINE__);
	printk("STK83XX_REG_TILTTHD =0x%x", data);

	return sprintf(buf, "%d\n", data);
}

static ssize_t stk_tilt_angle_store(struct device *dev,
                                struct device_attribute *attr, const char *buf, size_t count)
{
	stk83xx_wrapper *stk_wrapper = dev_get_drvdata(dev);
	stk_data *stk = &stk_wrapper->stk;
	int ret = 0;
	unsigned long value;
	char *pHead = NULL;
	u8 data = 0;

	if( pHead = strstr(buf,"0x") )
		value = simple_strtol(buf, NULL, 16);
	else
		value = simple_strtoul(buf, NULL, 10);

	ret = STK_REG_WRITE(stk, STK83XX_REG_TILTTHD, value);
	if(ret<0)
		printk("[%s_%d] Set Failed !!\n",__FUNCTION__,__LINE__);

	ret = STK_REG_READ(stk, STK83XX_REG_TILTTHD, &data);
	printk("STK83XX_REG_TILTTHD =0x%x", data);

	return count;
}

static ssize_t stk_status_AP_show(struct device *dev,
                               struct device_attribute *attr, char *buf)
{
    stk83xx_wrapper *stk_wrapper = dev_get_drvdata(dev);
    stk_data *stk = &stk_wrapper->stk;
	int ret = 0;
	int val=0 ;
	u8 data = 0;
	u8 int_value = ~(STK83XX_SIXDCFG_6D_CTRL_LOW_X | STK83XX_SIXDCFG_6D_CTRL_HIGH_X |
					STK83XX_SIXDCFG_6D_CTRL_LOW_Y | STK83XX_SIXDCFG_6D_CTRL_HIGH_Y |
					STK83XX_SIXDCFG_6D_CTRL_LOW_Z | STK83XX_SIXDCFG_6D_CTRL_HIGH_Z |
					STK83XX_SIXDCFG_6D_MODE_MASK);

	ret = STK_REG_READ(stk, STK83XX_REG_SIXDSTS, &data);

	STK_ACC_LOG("stk_process_6d_tilt_status:: 6D_status=0x%x", data);
	switch (data){
		case STK83XX_SIXDSTS_X_L_MASK:
			int_value |= STK83XX_SIXDCFG_6D_CTRL_LOW_X;
			if(0x65 == gptHWCFG->m_val.bPCB){
				printk(KERN_INFO"[stk83xx] Up\n");
				val = EBRMAIN_UP;
			}
			break;
		case STK83XX_SIXDSTS_X_H_MASK:
			int_value |= STK83XX_SIXDCFG_6D_CTRL_HIGH_X;
			if(0x65 == gptHWCFG->m_val.bPCB){
				printk(KERN_INFO"[stk83xx] Down\n");
				val = EBRMAIN_DWON;
			}
			break;
		case STK83XX_SIXDSTS_Y_L_MASK:
			int_value |= STK83XX_SIXDCFG_6D_CTRL_LOW_Y;
			if(0x65 == gptHWCFG->m_val.bPCB){
				printk(KERN_INFO"[stk83xx] Left\n");
				val = EBRMAIN_LEFT;
			}
			break;
		case STK83XX_SIXDSTS_Y_H_MASK:
			int_value |= STK83XX_SIXDCFG_6D_CTRL_HIGH_Y;
			if(0x65 == gptHWCFG->m_val.bPCB){
				printk(KERN_INFO"[stk83xx] Right\n");
				val = EBRMAIN_RIGHT;	
			}
			break;
		case STK83XX_SIXDSTS_Z_L_MASK:
			int_value |= STK83XX_SIXDCFG_6D_CTRL_LOW_Z;
			if(0x65 == gptHWCFG->m_val.bPCB){
				printk(KERN_INFO"[stk83xx] Front\n");
				val = EBRMAIN_FACE_UP;
			}
			break;
		case STK83XX_SIXDSTS_Z_H_MASK:
			int_value |= STK83XX_SIXDCFG_6D_CTRL_HIGH_Z;
			if(0x65 == gptHWCFG->m_val.bPCB){
				printk(KERN_INFO"[stk83xx] Back\n");
				val = EBRMAIN_FACE_DOWN;	
			}
			break;
		default:
			STK_ACC_ERR("stk_process_6d_tilt_status:: Two direction, wait next trigger");
			break;
	}

	//disable current int
	int_value = ~int_value;
	//STK_ACC_ERR("stk_process_6d_tilt_status:: 0x1B=0x%x", int_value);
	STK_REG_WRITE(stk, STK83XX_REG_SIXDCFG, int_value);
	return sprintf(buf, "%d\n", val);
}


static DEVICE_ATTR(enable, 0664, stk_enable_show, stk_enable_store);
static DEVICE_ATTR(value, 0444, stk_value_show, NULL);
static DEVICE_ATTR(delay, 0664, stk_delay_show, stk_delay_store);
static DEVICE_ATTR(offset, 0664, stk_offset_show, stk_offset_store);
static DEVICE_ATTR(send, 0220, NULL, stk_send_store);
static DEVICE_ATTR(recv, 0664, stk_recv_show, stk_recv_store);
static DEVICE_ATTR(allreg, 0444, stk_allreg_show, NULL);
static DEVICE_ATTR(status_AP, 0444, stk_status_AP_show, NULL);
static DEVICE_ATTR(tilt_angle, 0664, stk_tilt_angle_show, stk_tilt_angle_store);
static DEVICE_ATTR(sixdthd, 0664, stk_sixdthd_show, stk_sixdthd_store);
static DEVICE_ATTR(chipinfo, 0444, stk_chipinfo_show, NULL);
static DEVICE_ATTR(fifo, 0664, stk_fifo_show, stk_fifo_store);
static DEVICE_ATTR(selftest, 0664, stk_selftest_show, stk_selftest_store);
static DEVICE_ATTR(range, 0664, stk_range_show, stk_range_store);
#ifdef STK_CALI
    static DEVICE_ATTR(cali, 0664, stk_cali_show, stk_cali_store);
#endif /* STK_CALI */
#ifdef STK_HW_STEP_COUNTER
    static DEVICE_ATTR(stepcount, 0664, stk_stepcount_show, stk_stepcount_store);
#endif /* STK_HW_STEP_COUNTER */
#ifdef STK_FIR
    static DEVICE_ATTR(firlen, 0664, stk_firlen_show, stk_firlen_store);
#endif /* STK_FIR */

static struct attribute *stk_attribute_accel[] =
{
    &dev_attr_enable.attr,
    &dev_attr_value.attr,
    &dev_attr_delay.attr,
    &dev_attr_offset.attr,
    &dev_attr_send.attr,
    &dev_attr_recv.attr,
    &dev_attr_allreg.attr,
    &dev_attr_chipinfo.attr,
    &dev_attr_fifo.attr,
    &dev_attr_selftest.attr,
    &dev_attr_range.attr,
	&dev_attr_status_AP.attr,
	&dev_attr_tilt_angle.attr,
	&dev_attr_sixdthd.attr,
#ifdef STK_CALI
    &dev_attr_cali.attr,
#endif /* STK_CALI */
#ifdef STK_HW_STEP_COUNTER
    &dev_attr_stepcount.attr,
#endif /* STK_HW_STEP_COUNTER */
#ifdef STK_FIR
    &dev_attr_firlen.attr,
#endif /* STK_FIR */
    NULL
};

struct attribute_group stk_attribute_accel_group =
{
    .name = "stk83xx",
    .attrs = stk_attribute_accel,
};

static struct stk83xx_platform_data stk_plat_data =
{
    .direction              = 1,
    .interrupt_int1_pin     = 117,
};

#ifdef QCOM_SENSORS
/* Accelerometer information read by HAL */
static struct sensors_classdev stk_cdev =
{
    .name = "stk83xx",
    .vendor = "Sensortek",
    .version = 1,
    .handle = SENSORS_ACCELERATION_HANDLE,
    .type = SENSOR_TYPE_ACCELEROMETER,
    .max_range = "39.24", /* 4G mode: 4.0f*9.81f=39.24f */
    .resolution = "0.01916", /* 4G mode,12-bit resolution: 9.81f/512.f=0.01916f */
    .sensor_power = "0.138",
    .min_delay = STK_MIN_DELAY_US,
    .max_delay = STK_MAX_DELAY_US,
    .delay_msec = 16,
    .fifo_reserved_event_count = 0,
    .fifo_max_event_count = 0,
    .enabled = 0,
    .max_latency = 0,
    .flags = 0, /* SENSOR_FLAG_CONTINUOUS_MODE */
    .sensors_enable = NULL,
    .sensors_poll_delay = NULL,
    .sensors_enable_wakeup = NULL,
    .sensors_set_latency = NULL,
    .sensors_flush = NULL,
    .sensors_calibrate = NULL,
    .sensors_write_cal_params = NULL,
};
#endif

#ifdef QCOM_SENSORS
/*
 * @brief: The handle for enable and disable sensor.
 *          include/linux/sensors.h
 *
 * @param[in] *sensors_cdev: struct sensors_classdev
 * @param[in] enabled:
 */
static int stk_cdev_sensors_enable(struct sensors_classdev *sensors_cdev,
                                   unsigned int enabled)
{
    struct stk83xx_wrapper *stk_wrapper = container_of(sensors_cdev, stk83xx_wrapper, accel_cdev);
    struct stk_data *stk = &stk_wrapper->stk;

    if (0 == enabled)
    {
        stk_set_enable(stk, 0);
    }
    else if (1 == enabled)
    {
        stk_set_enable(stk, 1);
    }
    else
    {
        STK_ACC_ERR("Invalid vlaue of input, input=%d", enabled);
        return -EINVAL;
    }

    return 0;
}

/*
 * @brief: The handle for set the sensor polling delay time.
 *          include/linux/sensors.h
 *
 * @param[in] *sensors_cdev: struct sensors_classdev
 * @param[in] delay_msec:
 */
static int stk_cdev_sensors_poll_delay(struct sensors_classdev *sensors_cdev,
                                       unsigned int delay_msec)
{
    struct stk83xx_wrapper *stk_wrapper = container_of(sensors_cdev, stk83xx_wrapper, accel_cdev);
    struct stk_data *stk = &stk_wrapper->stk;
    stk_set_delay(stk, delay_msec * 1000UL);
    return 0;
}
#endif
/*
 * @brief:
 *          include/linux/sensors.h
 *
 * @param[in] *sensors_cdev: struct sensors_classdev
 * @param[in] enable:
 */
static int stk_cdev_sensors_enable_wakeup(struct sensors_classdev *sensors_cdev,
        unsigned int enable)
{
    STK_ACC_LOG("enable=%d", enable);
    return 0;
}

#ifdef QCOM_SENSORS
/*
 * @brief: Set the max report latency of the sensor.
 *          include/linux/sensors.h
 *
 * @param[in] *sensors_cdev: struct sensors_classdev
 * @param[in] max_latency: msec
 */
static int stk_cdev_sensors_set_latency(struct sensors_classdev *sensors_cdev,
                                        unsigned int max_latency)
{
    struct stk83xx_wrapper *stk_wrapper = container_of(sensors_cdev, stk83xx_wrapper, accel_cdev);
    struct stk_data *stk = &stk_wrapper->stk;
    const int sample_rate_us = stk_get_delay(stk);
    int latency_us = stk_get_delay(stk);

    if (!stk->fifo)
    {
        return 0;
    }

    if (max_latency * 1000 > sample_rate_us)
    {
        latency_us = max_latency * 1000;
    }

    if (latency_us != sample_rate_us)
    {
        u8 wm = latency_us / sample_rate_us;

        if (0 != latency_us % sample_rate_us)
        {
            wm++;
        }

        if (STK83XX_CFG1_XYZ_FRAME_MAX < wm)
        {
            wm = STK83XX_CFG1_XYZ_FRAME_MAX;
            STK_ACC_ERR("wm out of range. latency(us)=%d, odr(us)=%d",
                        latency_us, sample_rate_us);
        }

        if (stk_change_fifo_status(stk, wm))
        {
            STK_ACC_ERR("failed");
        }
        else
        {
#ifdef STK_POLLING_MODE
            STK_TIMER_STOP(stk, &stk->stk_timer_info);
            stk->stk_timer_info.interval_time = sample_rate_us * wm;
            STK_TIMER_START(stk, &stk->stk_timer_info);
#endif /* STK_POLLING_MODE */
            stk_set_fifo_start_time(stk);
        }
    }

    return 0;
}

/*
 * @brief: Flush sensor events in FIFO and report it to user space.
 *          include/linux/sensors.h
 *
 * @param[in] *sensors_cdev: struct sensors_classdev
 */
static int stk_cdev_sensors_flush(struct sensors_classdev *sensors_cdev)
{
    struct stk83xx_wrapper *stk_wrapper = container_of(sensors_cdev, stk83xx_wrapper, accel_cdev);
    struct stk_data *stk = &stk_wrapper->stk;

    if (!stk->fifo)
    {
        return 0;
    }

    stk_read_then_report_fifo_data(stk);
    return 0;
}
#endif

#ifdef QCOM_SENSORS
#ifdef STK_CALI
/*
 * @brief: Self calibration.
 *          include/linux/sensors.h
 *
 * @param[in] *sensors_cdev: struct sensors_classdev
 * @param[in] axis
 * @param[in] apply_now
 */
static int stk_cdev_sensors_calibrate(struct sensors_classdev *sensors_cdev,
                                      int axis, int apply_now)
{
    struct stk83xx_wrapper *stk_wrapper = container_of(sensors_cdev, stk83xx_wrapper, accel_cdev);
    struct stk_data *stk = &stk_wrapper->stk;
    stk_set_cali(stk);
    return 0;
}

/*
 * @brief:
 *          include/linux/sensors.h
 *
 * @param[in] *sensors_cdev: struct sensors_classdev
 * @param[in] *cal_result:
 */
static int stk_cdev_sensors_cal_params(struct sensors_classdev *sensors_cdev,
                                       struct cal_result_t *cal_result)
{
    struct stk83xx_wrapper *stk_wrapper = container_of(sensors_cdev, stk83xx_wrapper, accel_cdev);
    struct stk_data *stk = &stk_wrapper->stk;
    stk->cali_sw[0] = cal_result->offset_x;
    stk->cali_sw[1] = cal_result->offset_y;
    stk->cali_sw[2] = cal_result->offset_z;
    return 0;
}
#endif /* STK_CALI */
#endif

#ifdef STK_AMD
/*
 * @brief: report any motion result to /sys/class/input/inputX/capabilities/abs
 *
 * @param[in/out] stk: struct stk_data *
 * @param[in] flag: ANY MOTION status
 *              true: set 1 to /sys/class/input/inputX/capabilities/abs
 *              false: set 0 to /sys/class/input/inputX/capabilities/abs
 */
void stk_report_ANYMOTION(struct stk_data *stk, bool flag)
{
    if (!stk->input_dev_amd)
    {
        STK_ACC_ERR("No input device for ANY motion");
        return;
    }

    if (flag)
    {
        input_report_abs(stk->input_dev_amd, ABS_MISC, 0x1);
        STK_ACC_LOG("trigger");
    }
    else
    {
        input_report_abs(stk->input_dev_amd, ABS_MISC, 0x0);
        STK_ACC_LOG("no ANY motion");
    }

    input_sync(stk->input_dev_amd);
}

#endif /* STK_AMD */

/*
 * @brief: File system setup for accel and any motion
 *
 * @param[in/out] stk: struct stk_data *
 *
 * @return: Success or fail
 *          0: Success
 *          others: Fail
 */
static int stk_input_setup(stk83xx_wrapper *stk_wrapper)
{
	int err = 0;
	/* input device: setup for accel */
	stk_wrapper->input_dev_accel = input_allocate_device();
	if (!stk_wrapper->input_dev_accel) {
		STK_ACC_ERR("input_allocate_device for accel failed");
		return -ENOMEM;
	}

	stk_wrapper->input_dev_accel->name = "accelerometer";
	stk_wrapper->input_dev_accel->id.bustype = BUS_I2C;
	input_set_capability(stk_wrapper->input_dev_accel, EV_ABS, ABS_MISC);
	input_set_abs_params(stk_wrapper->input_dev_accel, ABS_X, 32767, -32768, 0, 0);
	input_set_abs_params(stk_wrapper->input_dev_accel, ABS_Y, 32767, -32768, 0, 0);
	input_set_abs_params(stk_wrapper->input_dev_accel, ABS_Z, 32767, -32768, 0, 0);
	stk_wrapper->input_dev_accel->dev.parent = &stk_wrapper->i2c_mgr.client->dev;

	input_set_drvdata(stk_wrapper->input_dev_accel, stk_wrapper);
	err = input_register_device(stk_wrapper->input_dev_accel);

	if (err){
		STK_ACC_ERR("Unable to register input device: %s", stk_wrapper->input_dev_accel->name);
		input_free_device(stk_wrapper->input_dev_accel);
		return err;
	}

#ifdef STK_AMD
    /* input device: setup for any motion */
    stk_wrapper->input_dev_amd = input_allocate_device();

    if (!stk_wrapper->input_dev_amd)
    {
        STK_ACC_ERR("input_allocate_device for ANY MOTION failed");
        input_free_device(stk_wrapper->input_dev_accel);
        input_unregister_device(stk_wrapper->input_dev_accel);
        return -ENOMEM;
    }

    stk_wrapper->input_dev_amd->name = "any motion";
    stk_wrapper->input_dev_amd->id.bustype = BUS_I2C;
    input_set_capability(stk_wrapper->input_dev_amd, EV_ABS, ABS_MISC);
    stk_wrapper->input_dev_amd->dev.parent = &stk_wrapper->i2c_mgr->client->dev;
    input_set_drvdata(stk_wrapper->input_dev_amd, stk_wrapper);
    err = input_register_device(stk_wrapper->input_dev_amd);

    if (err)
    {
        STK_ACC_ERR("Unable to register input device: %s",
                    stk_wrapper->input_dev_amd->name);
        input_free_device(stk_wrapper->input_dev_amd);
        input_free_device(stk_wrapper->input_dev_accel);
        input_unregister_device(stk_wrapper->input_dev_accel);
        return err;
    }

#endif /* STK_AMD */
	return 0;
}

/*
 * @brief:
 *
 * @param[in/out] stk: struct stk_data *
 *
 * @return:
 *      0: Success
 *      others: Fail
 */
static int stk_init_qualcomm(stk83xx_wrapper *stk_wrapper)
{
    int err = 0;
    struct stk_data *stk = &stk_wrapper->stk;
	if (stk_input_setup(stk_wrapper)) {
		return -1;
	}

    /* sysfs: create file system */
    err = sysfs_create_group(&stk_wrapper->i2c_mgr.client->dev.kobj,
                             &stk_attribute_accel_group);

    if (err)
    {
        STK_ACC_ERR("Fail in sysfs_create_group, err=%d", err);
        goto err_sysfs_creat_group;
    }

#ifdef QCOM_SENSORS
	stk_wrapper->accel_cdev = stk_cdev;

	if (STK8BA50_R_ID == stk->pid) {
		stk_wrapper->accel_cdev.name = "stk8ba50r";
    }
	else if (STK8BA53_ID == stk->pid) {
		stk_wrapper->accel_cdev.name = "stk8ba53";
	}
	else if (STK8323_ID == stk->pid || STK8327_ID == stk->pid) {
		stk_wrapper->accel_cdev.name = "stk832x";
		stk_wrapper->accel_cdev.fifo_reserved_event_count = 30,
		stk_wrapper->accel_cdev.fifo_max_event_count = 32,
		stk_wrapper->accel_cdev.sensors_set_latency = stk_cdev_sensors_set_latency;
	}

#if (STK83XX_RANGESEL_2G == STK83XX_RANGESEL_DEF)
	stk_wrapper->accel_cdev.max_range = "19.62"; /* 2G mode: 2.0f*9.81f=19.62f */

	if (STK8BA50_R_ID == stk->pid) {
		stk_wrapper->accel_cdev.resolution = "0.03832";
	}
	else {
		/* 2G mode,12-bit resolution: 9.81f/1024.f=0.00958f */
		stk_wrapper->accel_cdev.resolution = "0.00958";
	}

#elif (STK83XX_RANGESEL_4G == STK83XX_RANGESEL_DEF)
	stk_wrapper->accel_cdev.max_range = "39.24"; /* 4G mode: 4.0f*9.81f=39.24f */

	if (STK8BA50_R_ID == stk->pid) {
		stk_wrapper->accel_cdev.resolution = "0.07664";
	}
	else {
		/* 4G mode,12-bit resolution: 9.81f/512.f=0.01916f */
		stk_wrapper->accel_cdev.resolution = "0.01916";
	}

#elif (STK83XX_RANGESEL_8G == STK83XX_RANGESEL_DEF)
	stk_wrapper->accel_cdev.max_range = "78.48"; /* 8G mode: 8.0f*9.81f=78.48f */

	if (STK8BA50_R_ID == stk->pid) {
		stk_wrapper->accel_cdev.resolution = "0.15328";
	}
	else {
		/* 8G mode,12-bit resolution: 9.81f/256.f=0.03832f */
		stk_wrapper->accel_cdev.resolution = "0.03832";
	}

#endif /* mapping for STK83XX_RANGESEL_DEF */
	stk_wrapper->accel_cdev.sensors_enable = stk_cdev_sensors_enable;
	stk_wrapper->accel_cdev.sensors_poll_delay = stk_cdev_sensors_poll_delay;
	stk_wrapper->accel_cdev.sensors_enable_wakeup = stk_cdev_sensors_enable_wakeup;
	stk_wrapper->accel_cdev.sensors_flush = stk_cdev_sensors_flush;
#ifdef STK_CALI
	stk_wrapper->accel_cdev.sensors_calibrate = stk_cdev_sensors_calibrate;
	stk_wrapper->accel_cdev.sensors_write_cal_params = stk_cdev_sensors_cal_params;
#else /* no STK_CALI */
	stk_wrapper->accel_cdev.sensors_calibrate = NULL;
	stk_wrapper->accel_cdev.sensors_write_cal_params = NULL;
#endif /* STK_CALI */

#endif // QCOM_SENSORS



#ifdef QCOM_SENSORS
	err = sensors_classdev_register(&stk_wrapper->input_dev_accel->dev, &stk_wrapper->accel_cdev);
	if (err) {
		STK_ACC_ERR("Fail in sensors_classdev_register, err=%d", err);
		goto err_sensors_classdev_register;
	}
#endif


    return 0;
err_sensors_classdev_register:
    sysfs_remove_group(&stk_wrapper->i2c_mgr.client->dev.kobj, &stk_attribute_accel_group);
err_sysfs_creat_group:
#ifdef STK_AMD
    input_free_device(stk_wrapper->input_dev_amd);
    input_unregister_device(stk_wrapper->input_dev_amd);
#endif /* STK_AMD */

#ifdef QCOM_SENSORS
	input_free_device(stk_wrapper->input_dev_accel);
	input_unregister_device(stk_wrapper->input_dev_accel);
#endif
    return -1;
}

/*
 * @brief: Exit qualcomm related settings safely.
 *
 * @param[in/out] stk: struct stk_data *
 */
static void stk_exit_qualcomm(struct stk83xx_wrapper *stk_wrapper)
{
#ifdef QCOM_SENSORS
	sensors_classdev_unregister(&stk_wrapper->accel_cdev);
#endif
    sysfs_remove_group(&stk_wrapper->i2c_mgr.client->dev.kobj,
                       &stk_attribute_accel_group);
#ifdef STK_AMD
    input_free_device(stk_wrapper->input_dev_amd);
    input_unregister_device(stk_wrapper->input_dev_amd);
#endif /* STK_AMD */

#ifdef QCOM_SENSORS
	input_free_device(stk_wrapper->input_dev_accel);
	input_unregister_device(stk_wrapper->input_dev_accel);
#endif
}

#ifdef CONFIG_OF
/*
 * @brief: Parse data in device tree
 *
 * @param[in] dev: struct device *
 * @param[in/out] pdata: struct stk83xx_platform_data *
 *
 * @return: Success or fail
 *          0: Success
 *          others: Fail
 */
static int stk_parse_dt(struct device *dev,
                        struct stk83xx_platform_data *pdata)
{
    struct device_node *np = dev->of_node;
    const int *p;
    uint32_t int_flags;
    p = of_get_property(np, "stk,direction", NULL);

    if (p)
        pdata->direction = be32_to_cpu(*p);

    pdata->interrupt_int1_pin = of_get_named_gpio_flags(np,
                                "stk83xx,irq-gpio", 0, &int_flags);

    if (pdata->interrupt_int1_pin < 0)
    {
        STK_ACC_ERR("Unable to read stk83xx,irq-gpio");
#ifdef STK_INTERRUPT_MODE
        return pdata->interrupt_int1_pin;
#else /* no STK_INTERRUPT_MODE */
        return 0;
#endif /* STK_INTERRUPT_MODE */
    }

    return 0; /* SUCCESS */
}
#else
static int stk_parse_dt(struct device *dev,
                        struct stk83xx_platform_data *pdata)
{
    return -ENODEV
}
#endif /* CONFIG_OF */

/*
 * @brief: Get platform data
 *
 * @param[in/out] stk: struct stk_data *
 *
 * @return: Success or fail
 *          0: Success
 *          others: Fail
 */
static int get_platform_data(stk83xx_wrapper *stk_wrapper)
{
    int err = 0;
    struct stk_data *stk = &stk_wrapper->stk;
    struct i2c_client *client = stk_wrapper->i2c_mgr.client;
    struct stk83xx_platform_data *stk_platdata;

    if (client->dev.of_node)
    {
        STK_ACC_FUN();
        stk_platdata = devm_kzalloc(&client->dev,
                                    sizeof(struct stk83xx_platform_data), GFP_KERNEL);

        if (!stk_platdata)
        {
            STK_ACC_ERR("Failed to allocate memory");
            return -ENOMEM;
        }

        err = stk_parse_dt(&client->dev, stk_platdata);
        if (err)
        {
            STK_ACC_ERR("stk_parse_dt err=%d", err);
            return err;
        }
    }
    else
    {
        if (NULL != client->dev.platform_data)
        {
            STK_ACC_ERR("probe with platform data");
            stk_platdata = client->dev.platform_data;
        }
        else
        {
            STK_ACC_ERR("probe with private platform data");
            stk_platdata = &stk_plat_data;
        }
    }

#ifdef STK_INTERRUPT_MODE
    stk->gpio_info.int_pin = stk_platdata->interrupt_int1_pin;
#endif /* STK_INTERRUPT_MODE */
    stk->direction = stk_platdata->direction;
    return 0;
}

/*
 * @brief: Updata fifo_start_ns
 *
 * @param[in/out] stk: struct stk_data *
 */
static void stk_set_fifo_start_time(struct stk_data *stk)
{
    stk83xx_wrapper *stk_wrapper = container_of(stk, stk83xx_wrapper, stk);
    struct timespec ts;
    get_monotonic_boottime(&ts);
    stk_wrapper->fifo_start_ns = timespec_to_ns(&ts);
}

void read_data_callback(struct stk_data *stk)
{
    int ii = 0;
    s16 coor_trans[3] = {0};
    stk83xx_wrapper *stk_wrapper = container_of(stk, stk83xx_wrapper, stk);
    stk_wrapper->timestamp = ktime_get_boottime();
    //STK_ACC_LOG("xyz before coordinate trans %d %d %d with direction:%d",stk->xyz[0], stk->xyz[1], stk->xyz[2], stk->direction);

    for (ii = 0; ii < 3; ii++)
    {
        coor_trans[0] += stk->xyz[ii] * coordinate_trans[stk->direction][0][ii];
        coor_trans[1] += stk->xyz[ii] * coordinate_trans[stk->direction][1][ii];
        coor_trans[2] += stk->xyz[ii] * coordinate_trans[stk->direction][2][ii];
    }

    stk->xyz[0] = coor_trans[0];
    stk->xyz[1] = coor_trans[1];
    stk->xyz[2] = coor_trans[2];
    //STK_ACC_LOG("xyz after coordinate trans %d %d %d",stk->xyz[0], stk->xyz[1], stk->xyz[2]);
}

#if defined STK_INTERRUPT_MODE || defined STK_POLLING_MODE
/*
 * @brief: Report accel data to /sys/class/input/inputX/capabilities/rel
 *
 * @param[in/out] stk: struct stk_data *
 */
void stk_report_accel_data(struct stk_data *stk)
{
	stk83xx_wrapper *stk_wrapper = container_of(stk, stk83xx_wrapper, stk);

	if (!g_stk83xx_data->input_dev_accel)
	{
		STK_ACC_ERR("stk_report_accel_data:: No input device for accel data");
		return;
	}
	STK_ACC_LOG("stk_report_accel_data:: Report accel data");
	input_report_abs(g_stk83xx_data->input_dev_accel, ABS_X, stk->xyz[0]);
	input_report_abs(g_stk83xx_data->input_dev_accel, ABS_Y, stk->xyz[1]);
	input_report_abs(g_stk83xx_data->input_dev_accel, ABS_Z, stk->xyz[2]);
	input_report_abs(g_stk83xx_data->input_dev_accel, ABS_MISC, stk->sensitivity);
#ifdef QCOM_SENSORS	
	input_event(stk_wrapper->input_dev_accel, EV_SYN, SYN_TIME_SEC,
	ktime_to_timespec(stk_wrapper->timestamp).tv_sec);
	input_event(stk_wrapper->input_dev_accel, EV_SYN, SYN_TIME_NSEC,
	ktime_to_timespec(stk_wrapper->timestamp).tv_nsec);
	input_sync(stk_wrapper->input_dev_accel);
#endif
}

/*
 * @brief: Report accel data to /sys/class/input/inputX/capabilities/rel
 *
 * @param[in/out] stk: struct stk_data *
 */
void stk_report_fifo_data(struct stk_data *stk, s16 *xyz, int len)
{
    stk83xx_wrapper *stk_wrapper = container_of(stk, stk83xx_wrapper, stk);
    u64 sec, ns;

#ifdef QCOM_SENSORS
	if (!stk_wrapper->input_dev_accel)
	{
		STK_ACC_ERR("No input device for accel data");
		return;
	}
#endif

    stk_wrapper->fifo_start_ns += stk_get_delay(stk);
    sec = stk_wrapper->fifo_start_ns;
    ns = do_div(sec, NSEC_PER_SEC);
#ifdef QCOM_SENSORS
	input_report_abs(stk_wrapper->input_dev_accel, ABS_X, xyz[0]);
	input_report_abs(stk_wrapper->input_dev_accel, ABS_Y, xyz[1]);
	input_report_abs(stk_wrapper->input_dev_accel, ABS_Z, xyz[2]);
	input_event(stk_wrapper->input_dev_accel, EV_SYN, SYN_TIME_SEC, (int)sec);
	input_event(stk_wrapper->input_dev_accel, EV_SYN, SYN_TIME_NSEC, (int)ns);
	input_sync(stk_wrapper->input_dev_accel);
#endif
}

#endif /* defined STK_INTERRUPT_MODE || defined STK_POLLING_MODE */

/*
 * @brief: Probe function for i2c_driver.
 *
 * @param[in] client: struct i2c_client *
 * @param[in] stk_bus_ops: const struct stk_bus_ops *
 *
 * @return: Success or fail
 *          0: Success
 *          others: Fail
 */
int stk_i2c_probe(struct i2c_client *client,
                  struct common_function *common_fn)
{
	int err = 0;
	stk83xx_wrapper *stk_wrapper;
	struct stk_data *stk;
	STK_ACC_LOG("STK_HEADER_VERSION: %s ", STK_HEADER_VERSION);
	STK_ACC_LOG("STK_C_VERSION: %s ", STK_C_VERSION);
	STK_ACC_LOG("STK_DRV_I2C_VERSION: %s ", STK_DRV_I2C_VERSION);
	STK_ACC_LOG("STK_QUALCOMM_VERSION: %s ", STK_QUALCOMM_VERSION);

	if (NULL == client) {
		return -ENOMEM;
	}
	else if (!common_fn) {
		STK_ACC_ERR("cannot get common function. EXIT");
		return -EIO;
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		err = i2c_get_functionality(client->adapter);
		STK_ACC_ERR("i2c_check_functionality error, functionality=0x%x", err);
		return -EIO;
	}
	
	/* kzalloc: allocate memory and set to zero. */
	stk_wrapper = kzalloc(sizeof(stk83xx_wrapper), GFP_KERNEL);

	if (!stk_wrapper) {
		STK_ACC_ERR("memory allocation error");
		return -ENOMEM;
	}

	stk = &stk_wrapper->stk;
	if (!stk) {
		printk(KERN_ERR "%s: failed to allocate stk3a8x_data\n", __func__);
		return -ENOMEM;
	}

	g_stk83xx_data = stk_wrapper;
    stk_wrapper->i2c_mgr.client = client;
    stk_wrapper->i2c_mgr.addr_type = ADDR_8BIT;
    stk->bops   = common_fn->bops;
    stk->tops   = common_fn->tops;
    stk->gops   = common_fn->gops;
    stk->sops   = common_fn->sops;
    stk->read_data_cb = read_data_callback;
    stk->accel_report_cb = stk_report_accel_data;
    stk->read_fifo_cb = stk_set_fifo_start_time;
    stk->fifo_report_cb = stk_report_fifo_data;
#ifdef STK_AMD
    stk->reportamd_cb = stk_report_ANYMOTION;
#endif
    i2c_set_clientdata(client, stk_wrapper);
    mutex_init(&stk_wrapper->i2c_mgr.lock);
    stk->bus_idx = stk->bops->init(&stk_wrapper->i2c_mgr);

    if (stk->bus_idx < 0)
    {
        goto err_free_mem;
    }

    if (get_platform_data(stk_wrapper))
        goto err_free_mem;

    err = stk_get_pid(stk);

    if (err)
        goto err_free_mem;

    STK_ACC_LOG("PID 0x%x", stk->pid);
    stk_data_initialize(stk);
    stk_set_fifo_start_time(stk);

	if (stk_init_qualcomm(stk_wrapper)) {
		STK_ACC_ERR("stk_init_qualcomm failed");
		goto err_exit;
	}
	if (stk_reg_init(stk, STK83XX_RANGESEL_DEF, stk->sr_no)) {
		STK_ACC_ERR("stk83xx initialization failed");
		goto err_exit;
	}

#ifdef STK_INTERRUPT_MODE
    strcpy(stk->gpio_info.wq_name, "stk_acc_int");
    strcpy(stk->gpio_info.device_name, "stk_acc_irq");
    stk->gpio_info.gpio_cb = stk_work_queue;
    stk->gpio_info.trig_type = TRIGGER_FALLING;
    stk->gpio_info.is_active = false;
    stk->gpio_info.is_exist = false;
    stk->gpio_info.any = stk;
    err = STK_GPIO_IRQ_REGISTER(stk, &stk->gpio_info);
    err |= STK_GPIO_IRQ_START(stk, &stk->gpio_info);

	if (0 > err) {
		goto err_cancel_work_sync;
	}

#elif defined STK_POLLING_MODE
	strcpy(stk->stk_timer_info.wq_name, "stk_wq");
	stk->stk_timer_info.timer_unit = U_SECOND;
	stk->stk_timer_info.interval_time = stk_get_delay(stk);
	stk->stk_timer_info.timer_cb = stk_work_queue;
	stk->stk_timer_info.is_active = false;
	stk->stk_timer_info.is_exist = false;
	stk->stk_timer_info.any = stk;
	STK_TIMER_REGISTER(stk, &stk->stk_timer_info);
#endif /* STK_INTERRUPT_MODE, STK_POLLING_MODE */

	// enable g-sensor
	stk_set_enable(stk, 1);

	// set angle
	// stk->nRotate_Angle


	STK_ACC_LOG("Success");
	return 0;
err_exit:
#ifdef STK_INTERRUPT_MODE
err_cancel_work_sync:
    STK_GPIO_IRQ_REMOVE(stk, &stk->gpio_info);
#elif defined STK_POLLING_MODE
    STK_TIMER_REMOVE(stk, &stk->stk_timer_info);
#endif /* STK_INTERRUPT_MODE, STK_POLLING_MODE */
err_free_mem:
    stk->bops->remove(&stk_wrapper->i2c_mgr);
    mutex_destroy(&stk_wrapper->i2c_mgr.lock);
    kfree(stk_wrapper);
    return err;
}

/*
 * @brief: Remove function for i2c_driver.
 *
 * @param[in] client: struct i2c_client *
 *
 * @return: 0
 */
int stk_i2c_remove(struct i2c_client *client)
{
    stk83xx_wrapper *stk_wrapper = i2c_get_clientdata(client);
    struct stk_data *stk = &stk_wrapper->stk;
    stk_exit_qualcomm(stk_wrapper);
#ifdef STK_INTERRUPT_MODE
    STK_GPIO_IRQ_REMOVE(stk, &stk->gpio_info);
#elif defined STK_POLLING_MODE
    STK_TIMER_REMOVE(stk, &stk->stk_timer_info);
#endif /* STK_INTERRUPT_MODE, STK_POLLING_MODE */
    stk->bops->remove(&stk_wrapper->i2c_mgr);
    mutex_destroy(&stk_wrapper->i2c_mgr.lock);
    kfree(stk_wrapper);
    return 0;
}

/*
 * @brief: Suspend function for dev_pm_ops.
 *
 * @param[in] dev: struct device *
 *
 * @return: 0
 */
int stk83xx_suspend(struct device *dev)
{
	stk83xx_wrapper *stk_wrapper = dev_get_drvdata(dev);
	struct stk_data *stk = &stk_wrapper->stk;

	if (gSleep_Mode_Suspend) {
		if (atomic_read(&stk->enabled)) {
			stk_set_enable(stk, 0);
			stk->temp_enable = true;
			
		}
		else
			stk->temp_enable = false;
	}
	else {
		printk(KERN_ERR"stk83xx,enable irq wakeup source %d\n",gStk83xx_int);
		enable_irq_wake(gStk83xx_int);
	}
	
	return 0;
}

/*
 * @brief: Resume function for dev_pm_ops.
 *
 * @param[in] dev: struct device *
 *
 * @return: 0
 */
int stk83xx_resume(struct device *dev)
{
	stk83xx_wrapper *stk_wrapper = dev_get_drvdata(dev);
	struct stk_data *stk = &stk_wrapper->stk;

	if (gSleep_Mode_Suspend) {
		int ret,err; 
		if (stk_reg_init(stk, STK83XX_RANGESEL_DEF, stk->sr_no)) {
			STK_ACC_ERR("stk83xx initialization failed");

		}
		if (stk->temp_enable){
			stk_set_enable(stk, 1);
		}
		stk->temp_enable = false;
	}
	else {
		printk("stk83xx_resume,disable irq wakeup source %d\n",gStk83xx_int);
		disable_irq_wake(gStk83xx_int);
	}

	return 0;
}
