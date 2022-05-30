#if 1 /* E_BOOK */
/**
 * For Portable Reader System
 * 		past logging control
 *
 * file	pastlog.c
 * date		2011/02/17
 * Version	20110217
 *
 * Copyright (C) 2011, Sony Corporation.
 *
 *  THIS  SOFTWARE  IS PROVIDED   ``AS  IS'' AND   ANY  EXPRESS OR IMPLIED
 *  WARRANTIES,   INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 *  NO  EVENT  SHALL   THE AUTHOR  BE    LIABLE FOR ANY   DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *  NOT LIMITED   TO, PROCUREMENT OF  SUBSTITUTE GOODS  OR SERVICES; LOSS OF
 *  USE, DATA,  OR PROFITS; OR  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *  ANY THEORY OF LIABILITY, WHETHER IN  CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 *  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  version 2 of the  License.
 *
 */

#include <asm/uaccess.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/rwsem.h>
#include <linux/miscdevice.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/pastlog.h>
#include <linux/slab.h>
#include <linux/kthread.h>
#include <linux/timer.h>
#include <linux/mmc/rawdatatable.h>
#include <linux/kernel_logger.h>
#ifdef CONFIG_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif /* CONFIG_EARLYSUSPEND */

/*****************************************************************************
 * Defines
 */
#ifdef CONFIG_PAST_LOG_DEBUG
//#define	LOCAL_DEBUG(fmt, arg...)		PASTLOG_DEBUG(fmt, ##arg)
#define	LOCAL_DEBUG(fmt, arg...)
#else /* CONFIG_PAST_LOG_DEBUG */
#define	LOCAL_DEBUG(fmt, arg...)
//#define	LOCAL_DEBUG(fmt, arg...)		pastlog_degug(KERN_WARNING "[PASTLOG Info:%s(%d)]" fmt, __FUNCTION__, __LINE__, ##arg);
#endif /* CONFIG_PAST_LOG_DEBUG */

#define	PASTLOG_READ_SIZE		4096
#define	PASTLOG_WRITE_SIZE		4096
#define	PASTLOG_PRINTK_SIZE		1024

typedef enum {
	PASTLOG_THREAD_NOTINIT = -1,
	PASTLOG_THREAD_NONE,
	PASTLOG_THREAD_PM_IN,
	PASTLOG_THREAD_PM_SLEEP,
	PASTLOG_THREAD_PM,
	PASTLOG_THREAD_WRITE,
	PASTLOG_THREAD_FLUSH_IN,
	PASTLOG_THREAD_FLUSH,
	PASTLOG_THREAD_TERM
} PASTLOG_THREAD_STATE;

#define PASTLOG_SET_TIMER			mod_timer(&pastlog_timer, jiffies + msecs_to_jiffies(45000))	/* 45sec */
#define PASTLOG_SET_TIMER_RESUME	mod_timer(&pastlog_timer, jiffies + msecs_to_jiffies(1000))		/*  1sec */
#define PASTLOG_KILL_TIMER			del_timer(&pastlog_timer)

/*****************************************************************************
 * I/F Functions Prototype
 */
static int		__init pastlog_init(void);
static void		__exit pastlog_exit(void);
static int		pastlog_open(struct inode * inode, struct file * file);
static int		pastlog_release(struct inode * inode, struct file * file);
static ssize_t	pastlog_read(struct file *file, char __user *buf, size_t count, loff_t *ppos);
static ssize_t	pastlog_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos);
static int		pastlog_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg);

/*****************************************************************************
 * pastlog Thread Functions Prototype
 */
static int pastlog_thread(void* data);

/*****************************************************************************
 * pastlog Timer Functions Prototype
 */
static void		pastlog_timer_handler(unsigned long ptr);

#ifdef CONFIG_EARLYSUSPEND
/*****************************************************************************
 * EarlySuspend Functions Prototype
 */
static void pastlog_early_suspend(struct early_suspend *h);
static void pastlog_late_resume(struct early_suspend *h);
#endif /* CONFIG_EARLYSUSPEND */

/*****************************************************************************
 * Helper Functions Prototype
 */
static void pastlog_rawinit_check(void);
static void pastlog_wait_thread(PASTLOG_THREAD_STATE start, PASTLOG_THREAD_STATE active);
static bool	pastlog_mix2past_write(void);
static void pastlog_mix2past_flush(void);

/*****************************************************************************
 * Static Variables
 */
static struct semaphore		pastlog_state_sem;
static PASTLOG_THREAD_STATE	pastlog_state = PASTLOG_THREAD_NOTINIT;
static u_char*				pastlog_wbuf = NULL;
static u_char*				pastlog_rbuf = NULL;
static struct task_struct*	pastlog_thread_data = NULL;
static struct timer_list	pastlog_timer;
static bool					pastlog_write_start = false;
static unsigned int			pastlog_emmc = 0;
static unsigned int			pastlog_h = 0;
static PASTLOGH_DATA		pastlog_hdata = 0;
#ifdef CONFIG_EARLYSUSPEND
static struct early_suspend	pastlog_earlysuspend = {
	.level = EARLY_SUSPEND_LEVEL_DISABLE_FB,
	.suspend = pastlog_early_suspend,
	.resume = pastlog_late_resume,
};
#endif /* CONFIG_EARLYSUSPEND */

module_param(pastlog_state, int, S_IRUGO);
module_param(pastlog_emmc, int, S_IRUGO);
module_param(pastlog_h, int, S_IRUGO);

static struct file_operations pastlog_fops = {
	owner:		THIS_MODULE,
	open:		pastlog_open,
	release:	pastlog_release,
	read:		pastlog_read,
	write:		pastlog_write,
	ioctl:		pastlog_ioctl,
};

static struct miscdevice pastlog_miscdev = {
	name:		PASTLOG_DEVNAME,
	fops:		&pastlog_fops,
	minor:		MISC_DYNAMIC_MINOR,
};

/*****************************************************************************
 * Variables
 */

MODULE_AUTHOR("Sony");
MODULE_DESCRIPTION("Sony past log manager");
MODULE_LICENSE("GPL");

/*****************************************************************************
 * I/F Functions
 */
static int __init pastlog_init(void)
{
	int 	ret = -EFAULT;

	LOCAL_DEBUG("Start\n");

	printk("Past log manager.\n");
	pastlog_wbuf = (u_char*)kmalloc(PASTLOG_WRITE_SIZE, GFP_KERNEL);
	if(!pastlog_wbuf) {
		printk(KERN_ERR "  out of memory\n");
		goto error;
	}

	pastlog_rbuf = (u_char*)kmalloc(PASTLOG_READ_SIZE, GFP_KERNEL);
	if(!pastlog_rbuf) {
		printk(KERN_ERR "  out of memory\n");
		goto error;
	}

	ret = misc_register(&pastlog_miscdev);
	if(ret < 0) {
		printk(KERN_ERR "  %s: can't register\n", PASTLOG_DEVNAME);
		goto error;
	}

	sema_init(&pastlog_state_sem, 1);

	pastlog_state = PASTLOG_THREAD_WRITE;

	pastlog_thread_data = kthread_run(pastlog_thread, NULL, "pastlog_thread");
	if (IS_ERR(pastlog_thread_data)) {
		printk(KERN_ERR "  cannot run pastlog thread\n");
		ret = PTR_ERR(pastlog_thread_data);
		goto error_unreg;
	}

	init_timer(&pastlog_timer);
	pastlog_timer.function = pastlog_timer_handler;
	pastlog_write_start = true;
	PASTLOG_SET_TIMER;

	past_mmc_initialize();
	past_logger_init();
	pastlog_rawinit_check();

	ret = 0;

#ifdef CONFIG_EARLYSUSPEND
	register_early_suspend(&pastlog_earlysuspend);
#endif /* CONFIG_EARLYSUSPEND */
	goto end;

error_unreg:
	pastlog_state = PASTLOG_THREAD_NOTINIT;
	misc_deregister(&pastlog_miscdev);

error:
	if(pastlog_rbuf) {
		kfree(pastlog_rbuf);
		pastlog_rbuf = NULL;
	}
	if(pastlog_wbuf) {
		kfree(pastlog_wbuf);
		pastlog_wbuf = NULL;
	}

end:
	LOCAL_DEBUG("End %d\n", ret);
	return ret;
}

static void __exit pastlog_exit(void)
{
	LOCAL_DEBUG("Start\n");

	PASTLOG_KILL_TIMER;

	down(&pastlog_state_sem);
	pastlog_state = PASTLOG_THREAD_TERM;
	up(&pastlog_state_sem);
	kthread_stop(pastlog_thread_data);
	while(pastlog_state != PASTLOG_THREAD_NONE) {
		schedule();
	}

	pastlog_state = PASTLOG_THREAD_NOTINIT;

	misc_deregister(&pastlog_miscdev);

	if(pastlog_rbuf) {
		kfree(pastlog_rbuf);
	}
	if(pastlog_wbuf) {
		kfree(pastlog_wbuf);
	}

	LOCAL_DEBUG("End\n");
}

module_init(pastlog_init);
module_exit(pastlog_exit);

static int pastlog_open(struct inode * inode, struct file * file)
{
	return 0;
}

static int pastlog_release(struct inode * inode, struct file * file)
{
	return 0;
}

static ssize_t pastlog_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
	ssize_t	ret = 0;

	return ret;
}

static ssize_t pastlog_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
	ssize_t	ret = 0;

	LOCAL_DEBUG("Start : count=%u pos=%ld\n", count, *ppos);

	if(!pastlog_wbuf) {
		PASTLOG_DEBUG("not inited\n");
	}

	while(ret < (ssize_t)count)
	{
		size_t	size = (count - ret) >= PASTLOG_PRINTK_SIZE? PASTLOG_PRINTK_SIZE - 1 : count - ret;
		size_t	w_size = 0;
		LOCAL_DEBUG("ret=%u size=%u\n", ret, size);

		memset(pastlog_wbuf, 0, PASTLOG_WRITE_SIZE);
		if(copy_from_user(pastlog_wbuf, &buf[ret], size) != 0) {
			PASTLOG_DEBUG("copy_from_user error\n");
			break;
		}

		for(; pastlog_wbuf[w_size]; w_size++) {
			if(pastlog_wbuf[w_size] == '\n') {
				pastlog_wbuf[++w_size] = 0;
				break;
			}
		}

//		pastdev2logger(pastlog_wbuf);
		printk(pastlog_wbuf);

		ret += (ssize_t)w_size;
	}
	*ppos += (loff_t)ret;

	LOCAL_DEBUG("End : ret=%u pos=%ld\n", ret, *ppos);

	return ret;
}

static int pastlog_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = -EFAULT;

	LOCAL_DEBUG("Start : cmd=%u\n", cmd);
	pastlog_rawinit_check();

	down(&pastlog_state_sem);
	if(!pastlog_emmc ||
	   pastlog_state == PASTLOG_THREAD_NOTINIT ||
	   pastlog_state == PASTLOG_THREAD_PM ||
	   pastlog_state == PASTLOG_THREAD_TERM) {
		up(&pastlog_state_sem);
		return ret;
	}
	up(&pastlog_state_sem);

	switch (cmd) {
	case PASTLOGFLUSH:
		past_wait_flush();
		ret = 0;
		break;

	case PASTLOGWRITE:
		past_delay_write();
		ret = 0;
		break;

	case PASTLOGH1:
		if(pastlog_h != 0) {
			pastlog_h = 0;
			break;
		}

		pastlog_hdata = (PASTLOGH_DATA)pastlog_thread_data;
		if(copy_to_user((void __user *)arg, &pastlog_hdata, sizeof(PASTLOGH_DATA)))
			break;

		pastlog_hdata ^= (PASTLOGH_DATA)-1;

		pastlog_h = cmd;
		ret       = 0;
		break;

	case PASTLOGH2:
	{
		PASTLOGH_DATA	data;

		if(pastlog_h != PASTLOGH1 ||
		   copy_from_user(&data, (void __user *)arg, sizeof(PASTLOGH_DATA)) ||
		   data != pastlog_hdata) {
			pastlog_h = 0;
			break;
		}

		pastlog_hdata |= (PASTLOGH_DATA)pastlog_rbuf;
		if(copy_to_user((void __user *)arg, &pastlog_hdata, sizeof(PASTLOGH_DATA))) {
			pastlog_h = 0;
			break;
		}

		pastlog_hdata ^= (PASTLOGH_DATA)0xdeadbeef;

		pastlog_h = cmd;
		ret       = 0;
	}
		break;

	case PASTLOGH3:
	{
		PASTLOGH_DATA	data;

		if(pastlog_h != PASTLOGH2 ||
		   copy_from_user(&data, (void __user *)arg, sizeof(PASTLOGH_DATA)) ||
		   data != pastlog_hdata) {
			pastlog_h = 0;
			break;
		}

		pastlog_h = cmd;
		ret       = 0;
	}
		break;

#ifdef PASTLOGTESTDIE
	case PASTLOGTESTDIE:
	{
		char*	p = NULL;
		*p = 0;
	}
		break;
#endif /* PASTLOGTESTDIE */

	default:
		break;
	}

	LOCAL_DEBUG("End : ret=%d\n", ret);

	return ret;
}

bool past_can_read(void)
{
	bool ret = (pastlog_h == PASTLOGH3);
	pastlog_h = 0;
	return ret;
}

int past_delay_write(void)
{
	pastlog_rawinit_check();
	if(!pastlog_emmc ||
	   !pastlog_thread_data ||
	   pastlog_write_start)
		return -1;

	down(&pastlog_state_sem);
	if(pastlog_state != PASTLOG_THREAD_NONE &&
	   pastlog_state != PASTLOG_THREAD_WRITE) {
		up(&pastlog_state_sem);
		return 1;
	}
//	LOCAL_DEBUG("Write\n");
	pastlog_state = PASTLOG_THREAD_WRITE;
	wake_up_process(pastlog_thread_data);
	up(&pastlog_state_sem);
	return 0;
}

int past_suspend(const char* devname)
{
	int	ret = 0;

	pastlog_rawinit_check();
	if(!pastlog_emmc ||
	   !pastlog_thread_data ||
	   !is_rawdatadev(devname))
		return ret;

	LOCAL_DEBUG("Start %u\n", pastlog_state);

	PASTLOG_KILL_TIMER;

	down(&pastlog_state_sem);
	if(pastlog_state == PASTLOG_THREAD_NOTINIT ||
	   pastlog_state == PASTLOG_THREAD_PM ||
	   pastlog_state == PASTLOG_THREAD_TERM) {
		up(&pastlog_state_sem);
		LOCAL_DEBUG("None\n");
		return	ret;
	}

	if(pastlog_state != PASTLOG_THREAD_PM_SLEEP) {
		if(pastlog_state != PASTLOG_THREAD_PM_IN) {
			pastlog_state = PASTLOG_THREAD_PM_IN;
			wake_up_process(pastlog_thread_data);
		}

		up(&pastlog_state_sem);
		while(pastlog_state == PASTLOG_THREAD_PM_IN) {
			schedule();
		}
		down(&pastlog_state_sem);
	}
	pastlog_state = PASTLOG_THREAD_PM;
	up(&pastlog_state_sem);

	pastlog_write_start = true;

	LOCAL_DEBUG("End %u\n", pastlog_state);
	return ret;
}

int past_resume(const char* devname)
{
	int	ret = 0;

	pastlog_rawinit_check();
	if(!pastlog_emmc ||
	   !pastlog_thread_data ||
	   !is_rawdatadev(devname))
		return ret;

	LOCAL_DEBUG("Start %u\n", pastlog_state);
	pastlog_write_start = true;

	down(&pastlog_state_sem);
	if(pastlog_state == PASTLOG_THREAD_NOTINIT ||
	   pastlog_state == PASTLOG_THREAD_TERM) {
		up(&pastlog_state_sem);
		LOCAL_DEBUG("None\n");
		return ret;
	}
	pastlog_state = PASTLOG_THREAD_NONE;
	up(&pastlog_state_sem);

	PASTLOG_SET_TIMER_RESUME;

	LOCAL_DEBUG("End %u\n", pastlog_state);
	return ret;
}

void past_flush(void)
{
	pastlog_rawinit_check();
	if(!pastlog_emmc ||
	   !pastlog_thread_data)
		return;

	PASTLOG_KILL_TIMER;

	LOCAL_DEBUG("Flash start\n");
	klog_flush();
	pastlog_state = PASTLOG_THREAD_FLUSH;
	pastlog_mix2past_flush();
	pastlog_state = PASTLOG_THREAD_NONE;
	LOCAL_DEBUG("Flash end\n");
}

void past_wait_flush(void)
{
	LOCAL_DEBUG("Flash start\n");
	pastlog_wait_thread(PASTLOG_THREAD_FLUSH_IN, PASTLOG_THREAD_FLUSH);
	LOCAL_DEBUG("Flash end\n");
}

/*****************************************************************************
 * pastlog Thread Function
 */
static int pastlog_thread(void* data)
{
	LOCAL_DEBUG("Start\n");

	while(!kthread_should_stop()) {
		down(&pastlog_state_sem);

		while(pastlog_state != PASTLOG_THREAD_NONE) {

			if(pastlog_state == PASTLOG_THREAD_PM_IN) {

				pastlog_state = PASTLOG_THREAD_PM_SLEEP;
				break;

			} else if(pastlog_state == PASTLOG_THREAD_WRITE) {

				bool ret;
				pastlog_state = PASTLOG_THREAD_NONE;

				if(pastlog_write_start) {
					LOCAL_DEBUG("Write start wait\n");
					break;
				}

				up(&pastlog_state_sem);

				ret = pastlog_mix2past_write();

				down(&pastlog_state_sem);
				if(pastlog_state == PASTLOG_THREAD_NONE ||
				   pastlog_state == PASTLOG_THREAD_WRITE) {
					struct timespec	now;
					unsigned long	wait_end = 0;
					if(!ret) {
						LOCAL_DEBUG("Write data none\n");
						break;
					}

					now = current_kernel_time();
					wait_end = ((unsigned long)now.tv_sec % 100) * 1000 + ((unsigned long)now.tv_nsec / 1000000);
//					LOCAL_DEBUG("Start %lu [%ld.%ld]\n", wait_end, now.tv_sec, now.tv_nsec);
					wait_end += 500;
					for( ; ; ) {
						unsigned long	wait_now;
						up(&pastlog_state_sem);
						msleep(1);
						down(&pastlog_state_sem);
						now = current_kernel_time();
						wait_now = ((unsigned long)now.tv_sec % 100) * 1000 + ((unsigned long)now.tv_nsec / 1000000);
						if(wait_now >= wait_end) {
//							LOCAL_DEBUG("End %lu >= %lu [%ld.%ld]\n", wait_now, wait_end, now.tv_sec, now.tv_nsec);
							break;
						}
						if(pastlog_state != PASTLOG_THREAD_NONE &&
						   pastlog_state != PASTLOG_THREAD_WRITE) {
//							LOCAL_DEBUG("Event outbreak except Write sleep %ums. (%u)\n", wait_end - wait_now, pastlog_state);
							wait_end = 0;
							break;
						}
					}

					if(wait_end != 0) {
						if(pastlog_state == PASTLOG_THREAD_NONE)
							pastlog_state = PASTLOG_THREAD_WRITE;
					}
				} else {
					LOCAL_DEBUG("Event outbreak except Write. (%u)\n", pastlog_state);
				}

			} else if(pastlog_state == PASTLOG_THREAD_FLUSH_IN) {

				pastlog_state = PASTLOG_THREAD_FLUSH;
				up(&pastlog_state_sem);

				klog_flush();
				pastlog_mix2past_flush();

				down(&pastlog_state_sem);
				if(pastlog_state == PASTLOG_THREAD_FLUSH) {
					LOCAL_DEBUG("Flush end\n");
					pastlog_state = PASTLOG_THREAD_NONE;
				} else {
					LOCAL_DEBUG("Event outbreak except Flush. (%u)\n", pastlog_state);
				}
				break;

			} else if(pastlog_state == PASTLOG_THREAD_TERM) {

				break;

			}
		}

		up(&pastlog_state_sem);

		if(pastlog_state == PASTLOG_THREAD_TERM)
			break;

		set_current_state(TASK_INTERRUPTIBLE);
		schedule();
	}

	down(&pastlog_state_sem);
	pastlog_state = PASTLOG_THREAD_FLUSH;
	pastlog_mix2past_flush();
	pastlog_state = PASTLOG_THREAD_NONE;
	up(&pastlog_state_sem);

	LOCAL_DEBUG("End\n");
	return 0;
}

/*****************************************************************************
 * pastlog Timer Functions Prototype
 */
static void pastlog_timer_handler(unsigned long ptr)
{
	pastlog_write_start = false;
	LOCAL_DEBUG("Write start\n");
	past_delay_write();
}

#ifdef CONFIG_EARLYSUSPEND
/*****************************************************************************
 * EarlySuspend Functions Prototype
 */
static void pastlog_early_suspend(struct early_suspend *h)
{
	pastlog_rawinit_check();
	if(!pastlog_emmc ||
	   !pastlog_thread_data)
		return;

	LOCAL_DEBUG("Start %u\n", pastlog_state);

	PASTLOG_KILL_TIMER;

	down(&pastlog_state_sem);
	if(pastlog_state == PASTLOG_THREAD_NOTINIT ||
	   pastlog_state == PASTLOG_THREAD_PM_IN ||
	   pastlog_state == PASTLOG_THREAD_PM_SLEEP ||
	   pastlog_state == PASTLOG_THREAD_PM ||
	   pastlog_state == PASTLOG_THREAD_TERM) {
		LOCAL_DEBUG("None\n");
		up(&pastlog_state_sem);
		return;
	}

	pastlog_state = PASTLOG_THREAD_PM_IN;
	wake_up_process(pastlog_thread_data);
	pastlog_write_start = true;
	LOCAL_DEBUG("End %u\n", pastlog_state);
	up(&pastlog_state_sem);
}

static void pastlog_late_resume(struct early_suspend *h)
{
}
#endif /* CONFIG_EARLYSUSPEND */

/*****************************************************************************
 * Helper Functions Prototype
 */
static void pastlog_rawinit_check(void)
{
	static	bool	bInit = false;
	if(!bInit) {
		if(is_rawinit()) {
			pastlog_emmc = is_rawdatadev("mmcblk2");
			bInit = true;

			past_mmc_initialize();
			past_logger_init();
		}
	}
}

static void pastlog_wait_thread(PASTLOG_THREAD_STATE start, PASTLOG_THREAD_STATE active)
{
	down(&pastlog_state_sem);
	if(pastlog_state != active) {
		pastlog_state = start;
		wake_up_process(pastlog_thread_data);
	}

	do {
		up(&pastlog_state_sem);

		if(unlikely(in_atomic_preempt_off() && !current->exit_state)) {
			mdelay(100);
		} else {
			msleep(100);
			schedule();
		}

		down(&pastlog_state_sem);
	} while(pastlog_state == start || pastlog_state == active);
	up(&pastlog_state_sem);
}

static bool	pastlog_mix2past_write(void)
{
	size_t	total = 0;

	while(1) {
		size_t	remain = PASTLOG_READ_SIZE - total;
		size_t	rsize  = mix2past_read_entry(&pastlog_rbuf[total], remain);
		if((int)rsize <= 0)
			break;

		total += rsize;
	}

	if(total <= 0)
		return	false;

	LOCAL_DEBUG("Write %u\n", total);
	past_mmc_write(pastlog_rbuf, total);

	return	true;
}

static void pastlog_mix2past_flush(void)
{
	PASTLOG_DEBUG("start\n");
	while(pastlog_state == PASTLOG_THREAD_FLUSH &&
	      pastlog_mix2past_write())
		;
	PASTLOG_DEBUG("end\n");
}

#endif /* E_BOOK */
