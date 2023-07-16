/*
 * Generic RTC interface.
 * This version contains the part of the user interface to the Real Time Clock
 * service. It is used with both the legacy mc146818 and also  EFI
 * Struct rtc_time and first 12 ioctl by Paul Gortmaker, 1996 - separated out
 * from <linux/mc146818rtc.h> to this file for 2.4 kernels.
 *
 * Copyright (C) 1999 Hewlett-Packard Co.
 * Copyright (C) 1999 Stephane Eranian <eranian@hpl.hp.com>
 */
#ifndef _LINUX_RTC_H_
#define _LINUX_RTC_H_


#include <linux/types.h>
#include <linux/interrupt.h>
#include <uapi/linux/rtc.h>

extern int rtc_month_days(unsigned int month, unsigned int year);
extern int rtc_year_days(unsigned int day, unsigned int month, unsigned int year);
extern int rtc_valid_tm(struct rtc_time *tm);
extern time64_t rtc_tm_to_time64(struct rtc_time *tm);
extern void rtc_time64_to_tm(time64_t time, struct rtc_time *tm);
ktime_t rtc_tm_to_ktime(struct rtc_time tm);
struct rtc_time rtc_ktime_to_tm(ktime_t kt);

/**
 * Deprecated. Use rtc_time64_to_tm().
 */
static inline void rtc_time_to_tm(unsigned long time, struct rtc_time *tm)
{
	rtc_time64_to_tm(time, tm);
}

/**
 * Deprecated. Use rtc_tm_to_time64().
 */
static inline int rtc_tm_to_time(struct rtc_time *tm, unsigned long *time)
{
	*time = rtc_tm_to_time64(tm);

	return 0;
}

#include <linux/device.h>
#include <linux/seq_file.h>
#include <linux/cdev.h>
#include <linux/poll.h>
#include <linux/mutex.h>
#include <linux/timerqueue.h>
#include <linux/workqueue.h>

extern struct class *rtc_class;

/*
 * For these RTC methods the device parameter is the physical device
 * on whatever bus holds the hardware (I2C, Platform, SPI, etc), which
 * was passed to rtc_device_register().  Its driver_data normally holds
 * device state, including the rtc_device pointer for the RTC.
 *
 * Most of these methods are called with rtc_device.ops_lock held,
 * through the rtc_*(struct rtc_device *, ...) calls.
 *
 * The (current) exceptions are mostly filesystem hooks:
 *   - the proc() hook for procfs
 *   - non-ioctl() chardev hooks:  open(), release(), read_callback()
 *
 * REVISIT those periodic irq calls *do* have ops_lock when they're
 * issued through ioctl() ...
 */
struct rtc_class_ops {
	int (*open)(struct device *);
	void (*release)(struct device *);
	int (*ioctl)(struct device *, unsigned int, unsigned long);
	int (*read_time)(struct device *, struct rtc_time *);
	int (*set_time)(struct device *, struct rtc_time *);
	int (*read_alarm)(struct device *, struct rtc_wkalrm *);
	int (*set_alarm)(struct device *, struct rtc_wkalrm *);
	int (*proc)(struct device *, struct seq_file *);
	int (*set_mmss64)(struct device *, time64_t secs);
	int (*set_mmss)(struct device *, unsigned long secs);
	int (*read_callback)(struct device *, int data);
	int (*alarm_irq_enable)(struct device *, unsigned int enabled);
};

#define RTC_DEVICE_NAME_SIZE 20
typedef struct rtc_task {
	void (*func)(void *private_data);
	void *private_data;
} rtc_task_t;


struct rtc_timer {
	struct rtc_task	task;
	struct timerqueue_node node;
	ktime_t period;
	int enabled;
};


/* flags */
#define RTC_DEV_BUSY 0

struct rtc_device
{
	struct device dev;
	struct module *owner;

	int id;
	char name[RTC_DEVICE_NAME_SIZE];

	const struct rtc_class_ops *ops;
	struct mutex ops_lock;

	struct cdev char_dev;
	unsigned long flags;

	unsigned long irq_data;
	spinlock_t irq_lock;
	wait_queue_head_t irq_queue;
	struct fasync_struct *async_queue;

	struct rtc_task *irq_task;
	spinlock_t irq_task_lock;
	int irq_freq;
	int max_user_freq;

	struct timerqueue_head timerqueue;
	struct rtc_timer aie_timer;
	struct rtc_timer uie_rtctimer;
	struct hrtimer pie_timer; /* sub second exp, so needs hrtimer */
	int pie_enabled;
	struct work_struct irqwork;
	/* Some hardware can't support UIE mode */
	int uie_unsupported;

    /*
     * This offset specifies the update timing of the RTC.
     *
     * tsched     t1 write(t2.tv_sec - 1sec))  t2 RTC increments seconds
     *
     * The offset defines how tsched is computed so that the write to
     * the RTC (t2.tv_sec - 1sec) is correct versus the time required
     * for the transport of the write and the time which the RTC needs
     * to increment seconds the first time after the write (t2).
     *
     * For direct accessible RTCs tsched ~= t1 because the write time
     * is negligible. For RTCs behind slow busses the transport time is
     * significant and has to be taken into account.
     *
     * The time between the write (t1) and the first increment after
     * the write (t2) is RTC specific. For a MC146818 RTC it's 500ms,
     * for many others it's exactly 1 second. Consult the datasheet.
     *
     * The value of this offset is also used to calculate the to be
     * written value (t2.tv_sec - 1sec) at tsched.
     *
     * The default value for this is NSEC_PER_SEC + 10 msec default
     * transport time. The offset can be adjusted by drivers so the
     * calculation for the to be written value at tsched becomes
     * correct:
     *
     *  newval = tsched + set_offset_nsec - NSEC_PER_SEC
     * and  (tsched + set_offset_nsec) % NSEC_PER_SEC == 0
     */
    unsigned long set_offset_nsec;

    unsigned long features[BITS_TO_LONGS(RTC_FEATURE_CNT)];

    time64_t range_min;
    timeu64_t range_max;
    time64_t start_secs;
    time64_t offset_secs;
    bool set_start_time;

#ifdef CONFIG_RTC_INTF_DEV_UIE_EMUL
	struct work_struct uie_task;
	struct timer_list uie_timer;
	/* Those fields are protected by rtc->irq_lock */
	unsigned int oldsecs;
	unsigned int uie_irq_active:1;
	unsigned int stop_uie_polling:1;
	unsigned int uie_task_active:1;
	unsigned int uie_timer_active:1;
#endif
};
#define to_rtc_device(d) container_of(d, struct rtc_device, dev)

/* useful timestamps */
#define RTC_TIMESTAMP_BEGIN_0000    -62167219200ULL /* 0000-01-01 00:00:00 */
#define RTC_TIMESTAMP_BEGIN_1900    -2208988800LL /* 1900-01-01 00:00:00 */
#define RTC_TIMESTAMP_BEGIN_2000    946684800LL /* 2000-01-01 00:00:00 */
#define RTC_TIMESTAMP_END_2063      2966371199LL /* 2063-12-31 23:59:59 */
#define RTC_TIMESTAMP_END_2079      3471292799LL /* 2079-12-31 23:59:59 */
#define RTC_TIMESTAMP_END_2099      4102444799LL /* 2099-12-31 23:59:59 */
#define RTC_TIMESTAMP_END_2199      7258118399LL /* 2199-12-31 23:59:59 */
#define RTC_TIMESTAMP_END_9999      253402300799LL /* 9999-12-31 23:59:59 */

extern struct rtc_device *rtc_device_register(const char *name,
					struct device *dev,
					const struct rtc_class_ops *ops,
					struct module *owner);
extern struct rtc_device *devm_rtc_device_register(struct device *dev,
					const char *name,
					const struct rtc_class_ops *ops,
					struct module *owner);
extern void rtc_device_unregister(struct rtc_device *rtc);
extern void devm_rtc_device_unregister(struct device *dev,
					struct rtc_device *rtc);

extern int rtc_read_time(struct rtc_device *rtc, struct rtc_time *tm);
extern int rtc_set_time(struct rtc_device *rtc, struct rtc_time *tm);
extern int rtc_set_mmss(struct rtc_device *rtc, unsigned long secs);
extern int rtc_set_ntp_time(struct timespec64 now);
int __rtc_read_alarm(struct rtc_device *rtc, struct rtc_wkalrm *alarm);
extern int rtc_read_alarm(struct rtc_device *rtc,
			struct rtc_wkalrm *alrm);
extern int rtc_set_alarm(struct rtc_device *rtc,
				struct rtc_wkalrm *alrm);
extern int rtc_initialize_alarm(struct rtc_device *rtc,
				struct rtc_wkalrm *alrm);
extern void rtc_update_irq(struct rtc_device *rtc,
			unsigned long num, unsigned long events);

extern struct rtc_device *rtc_class_open(const char *name);
extern void rtc_class_close(struct rtc_device *rtc);

extern int rtc_irq_register(struct rtc_device *rtc,
				struct rtc_task *task);
extern void rtc_irq_unregister(struct rtc_device *rtc,
				struct rtc_task *task);
extern int rtc_irq_set_state(struct rtc_device *rtc,
				struct rtc_task *task, int enabled);
extern int rtc_irq_set_freq(struct rtc_device *rtc,
				struct rtc_task *task, int freq);
extern int rtc_update_irq_enable(struct rtc_device *rtc, unsigned int enabled);
extern int rtc_alarm_irq_enable(struct rtc_device *rtc, unsigned int enabled);
extern int rtc_dev_update_irq_enable_emul(struct rtc_device *rtc,
						unsigned int enabled);

void rtc_handle_legacy_irq(struct rtc_device *rtc, int num, int mode);
void rtc_aie_update_irq(void *private);
void rtc_uie_update_irq(void *private);
enum hrtimer_restart rtc_pie_update_irq(struct hrtimer *timer);

int rtc_register(rtc_task_t *task);
int rtc_unregister(rtc_task_t *task);
int rtc_control(rtc_task_t *t, unsigned int cmd, unsigned long arg);

void rtc_timer_init(struct rtc_timer *timer, void (*f)(void* p), void* data);
int rtc_timer_start(struct rtc_device *rtc, struct rtc_timer* timer,
			ktime_t expires, ktime_t period);
int rtc_timer_cancel(struct rtc_device *rtc, struct rtc_timer* timer);
void rtc_timer_do_work(struct work_struct *work);

static inline bool is_leap_year(unsigned int year)
{
	return (!(year % 4) && (year % 100)) || !(year % 400);
}

#ifdef CONFIG_RTC_HCTOSYS_DEVICE
extern int rtc_hctosys_ret;
#else
#define rtc_hctosys_ret -ENODEV
#endif

#endif /* _LINUX_RTC_H_ */
