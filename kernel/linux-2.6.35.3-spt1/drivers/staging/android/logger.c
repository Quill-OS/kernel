/*
 * drivers/misc/logger.c
 *
 * A Logging Subsystem
 *
 * Copyright (C) 2007-2008 Google, Inc.
 *
 * Robert Love <rlove@google.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/sched.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/time.h>
#include "logger.h"

#include <asm/ioctls.h>

#ifdef	CONFIG_MIX_LOGGER_STATIC /* E_BOOK */
#include <linux/io.h>
#include <linux/ioport.h>
#endif /* E_BOOK */
#ifdef	CONFIG_KERNEL_LOGGER /* E_BOOK */
#include <linux/kthread.h>
#endif /* E_BOOK */
#if 1 /* E_BOOK */
#include <linux/mix_logger.h>
#include <linux/pastlog.h>
#ifdef CONFIG_PAST_LOG_DEBUG
//#define	LOCAL_DEBUG(fmt, arg...)		PASTLOG_DEBUG(fmt, ##arg)
#define	LOCAL_DEBUG(fmt, arg...)
#else /* CONFIG_PAST_LOG_DEBUG */
#define	LOCAL_DEBUG(fmt, arg...)
#endif /* CONFIG_PAST_LOG_DEBUG */
#endif /* E_BOOK */

#if 0 /* E_BOOK */
/*
 * struct logger_log - represents a specific log, such as 'main' or 'radio'
 *
 * This structure lives from module insertion until module removal, so it does
 * not need additional reference counting. The structure is protected by the
 * mutex 'mutex'.
 */
struct logger_log {
	unsigned char 		*buffer;/* the ring buffer itself */
	struct miscdevice	misc;	/* misc device representing the log */
	wait_queue_head_t	wq;	/* wait queue for readers */
	struct list_head	readers; /* this log's readers */
	struct mutex		mutex;	/* mutex protecting buffer */
	size_t			w_off;	/* current write head offset */
	size_t			head;	/* new readers start here */
	size_t			size;	/* size of the log */
};

/*
 * struct logger_reader - a logging device open for reading
 *
 * This object lives from open to release, so we don't need additional
 * reference counting. The structure is protected by log->mutex.
 */
struct logger_reader {
	struct logger_log	*log;	/* associated log */
	struct list_head	list;	/* entry in logger_log's list */
	size_t			r_off;	/* current read head offset */
};
#endif /* E_BOOK */

/* logger_offset - returns index 'n' into the log via (optimized) modulus */
#if 1 /* E_BOOK */
#define logger_offset(n)	((n) % log->size)
#else /* E_BOOK */
/*
 * !!! BUG !!!
 *
 *   If total size is not 2 power, this calculating formula does not hold good.
 *
 *     For example, when be size of 63kB (63 * 1024 = 64512 = 0xFC00).
 *       The value that subtracted 1 becomes 0xFBFF.
 *       In the case of the binary, become 1111101111111111.
 *     When, in the case of 1036(0x40C), n executes this calculating formula;
 *     of 12(0xC) it follows.
 *
 *     A calculation does not make ends meet as a calculating formula in search
 *     of a rest in order that the value that bits do not continue as for this
 *     is spent.
 */
#define logger_offset(n)	((n) & (log->size - 1))
#endif /* E_BOOK */

#ifdef	CONFIG_MIX_LOGGER /* E_BOOK */
#define	MIXLOG_MAGIC				0x45424D4C	/* "EBML" */

typedef struct {
	unsigned long		magic;
	struct logger_log	log;
#ifdef CONFIG_PAST_LOG /* E_BOOK */
	struct logger_reader	reader;
#endif /* CONFIG_PAST_LOG */
} MIX_LOG_HEAD;

#define	MIXLOG_EVENT_LEVEL			4 /* ANDROID_LOG_INFO */

#ifdef CONFIG_MIX_LOGGER_STATIC
static	struct resource*	mix_log_res			= NULL;
static	char*				mix_log_addr		= NULL;
#else /* CONFIG_MIX_LOGGER_STATIC */
//#define	MIX_LOGGER_OLD

#ifndef MIX_LOGGER_OLD
static	char				mix_log_addr[MIX_LOG_RESERVE_SIZE];
#endif /* MIX_LOGGER_OLD */
#endif /* CONFIG_MIX_LOGGER_STATIC */

#ifndef MIX_LOGGER_OLD
static	MIX_LOG_HEAD*		mix_log_header		= NULL;
#endif /* MIX_LOGGER_OLD */

static inline struct logger_log *get_log_mix(void);

static int	mix_log_level_android = CONFIG_MIX_LOGGER_LEVEL_ANDROID;
module_param(mix_log_level_android, int, S_IRUGO | S_IWUSR);

#endif /* E_BOOK */

#ifdef	CONFIG_KERNEL_LOGGER /* E_BOOK */
#define	KERNEL_LOG_SIZE		(64*1024)

//#define	KERNEL_LOGGER_OLD

#ifndef KERNEL_LOGGER_OLD
static	unsigned char			_buf_log_kernel[KERNEL_LOG_SIZE];
static	struct logger_log		log_kernel;
#endif /* KERNEL_LOGGER_OLD */

static bool		klog_init = false;
static bool		klogger_init = false;
static bool		klogger_thread_init = false;
static inline struct logger_log *get_log_kernel(void);
static int klog_thread_init(void);

#ifdef	CONFIG_MIX_LOGGER
static int	mix_log_level_kernel  = CONFIG_MIX_LOGGER_LEVEL_KERNEL;
module_param(mix_log_level_kernel, int, S_IRUGO | S_IWUSR);
#endif /* CONFIG_MIX_LOGGER */
#endif /* E_BOOK */

#ifdef	CONFIG_PAST_LOG /* E_BOOK */
static void past_logger_write(void);
static inline struct logger_log *get_log_past(void);
#endif /* E_BOOK */

/*
 * file_get_log - Given a file structure, return the associated log
 *
 * This isn't aesthetic. We have several goals:
 *
 * 	1) Need to quickly obtain the associated log during an I/O operation
 * 	2) Readers need to maintain state (logger_reader)
 * 	3) Writers need to be very fast (open() should be a near no-op)
 *
 * In the reader case, we can trivially go file->logger_reader->logger_log.
 * For a writer, we don't want to maintain a logger_reader, so we just go
 * file->logger_log. Thus what file->private_data points at depends on whether
 * or not the file was opened for reading. This function hides that dirtiness.
 */
static inline struct logger_log *file_get_log(struct file *file)
{
	if (file->f_mode & FMODE_READ) {
		struct logger_reader *reader = file->private_data;
		return reader->log;
	} else
		return file->private_data;
}

/*
 * get_entry_len - Grabs the length of the payload of the next entry starting
 * from 'off'.
 *
 * Caller needs to hold log->mutex.
 */
static __u32 get_entry_len(struct logger_log *log, size_t off)
{
	__u16 val;

	switch (log->size - off) {
	case 1:
		memcpy(&val, log->buffer + off, 1);
		memcpy(((char *) &val) + 1, log->buffer, 1);
		break;
	default:
		memcpy(&val, log->buffer + off, 2);
	}

	return sizeof(struct logger_entry) + val;
}

/*
 * do_read_log_to_user - reads exactly 'count' bytes from 'log' into the
 * user-space buffer 'buf'. Returns 'count' on success.
 *
 * Caller must hold log->mutex.
 */
static ssize_t do_read_log_to_user(struct logger_log *log,
				   struct logger_reader *reader,
				   char __user *buf,
				   size_t count)
{
	size_t len;

	/*
	 * We read from the log in two disjoint operations. First, we read from
	 * the current read head offset up to 'count' bytes or to the end of
	 * the log, whichever comes first.
	 */
	len = min(count, log->size - reader->r_off);
	if (copy_to_user(buf, log->buffer + reader->r_off, len))
		return -EFAULT;

	/*
	 * Second, we read any remaining bytes, starting back at the head of
	 * the log.
	 */
	if (count != len)
		if (copy_to_user(buf + len, log->buffer, count - len))
			return -EFAULT;

	reader->r_off = logger_offset(reader->r_off + count);

	return count;
}

/*
 * logger_read - our log's read() method
 *
 * Behavior:
 *
 * 	- O_NONBLOCK works
 * 	- If there are no log entries to read, blocks until log is written to
 * 	- Atomically reads exactly one log entry
 *
 * Optimal read size is LOGGER_ENTRY_MAX_LEN. Will set errno to EINVAL if read
 * buffer is insufficient to hold next entry.
 */
static ssize_t logger_read(struct file *file, char __user *buf,
			   size_t count, loff_t *pos)
{
	struct logger_reader *reader = file->private_data;
	struct logger_log *log = reader->log;
	ssize_t ret;
#ifdef CONFIG_PAST_LOG /* E_BOOK */
	bool past = (log == get_log_past());
#endif /* E_BOOK */
	DEFINE_WAIT(wait);

#ifdef CONFIG_PAST_LOG /* E_BOOK */
	if(past) {
		goto past_read;
	}
#endif /* E_BOOK */

start:
	while (1) {
		prepare_to_wait(&log->wq, &wait, TASK_INTERRUPTIBLE);

		mutex_lock(&log->mutex);
		ret = (log->w_off == reader->r_off);
		mutex_unlock(&log->mutex);
		if (!ret)
			break;

		if (file->f_flags & O_NONBLOCK) {
			ret = -EAGAIN;
			break;
		}

		if (signal_pending(current)) {
			ret = -EINTR;
			break;
		}

		schedule();
	}

	finish_wait(&log->wq, &wait);
	if (ret)
		return ret;

#ifdef CONFIG_PAST_LOG /* E_BOOK */
past_read:
#endif /* E_BOOK */

	mutex_lock(&log->mutex);

	/* is there still something to read or did we race? */
	if (unlikely(log->w_off == reader->r_off)) {
#ifdef CONFIG_PAST_LOG /* E_BOOK */
		if(past) {
			PASTLOG_DEBUG("w_off(%x) == r_off(%x)\n", log->w_off, reader->r_off);
			ret = 0;
			goto out;
		}
#endif /* E_BOOK */
		mutex_unlock(&log->mutex);
		goto start;
	}

#ifdef CONFIG_PAST_LOG /* E_BOOK */
	if(past) {
		LOCAL_DEBUG("log w_off=%u r_off=%u size=%u\n", log->w_off, reader->r_off, log->size);
		ret = past_mmc_read(buf, count, &reader->r_off);
		LOCAL_DEBUG("log w_off=%u r_off=%u size=%u\n", log->w_off, reader->r_off, log->size);
		LOCAL_DEBUG("read %u size = %u\n", count, ret);
		goto out;
	}
#endif /* E_BOOK */

	/* get the size of the next entry */
	ret = get_entry_len(log, reader->r_off);
	if (count < ret) {
		ret = -EINVAL;
		goto out;
	}

	/* get exactly one entry from the log */
	ret = do_read_log_to_user(log, reader, buf, ret);

out:
	mutex_unlock(&log->mutex);

	return ret;
}

/*
 * get_next_entry - return the offset of the first valid entry at least 'len'
 * bytes after 'off'.
 *
 * Caller must hold log->mutex.
 */
static size_t get_next_entry(struct logger_log *log, size_t off, size_t len)
{
	size_t count = 0;

	do {
		size_t nr = get_entry_len(log, off);
		off = logger_offset(off + nr);
		count += nr;
	} while (count < len);

	return off;
}

/*
 * clock_interval - is a < c < b in mod-space? Put another way, does the line
 * from a to b cross c?
 */
static inline int clock_interval(size_t a, size_t b, size_t c)
{
	if (b < a) {
		if (a < c || b >= c)
			return 1;
	} else {
		if (a < c && b >= c)
			return 1;
	}

	return 0;
}

/*
 * fix_up_readers - walk the list of all readers and "fix up" any who were
 * lapped by the writer; also do the same for the default "start head".
 * We do this by "pulling forward" the readers and start head to the first
 * entry after the new write head.
 *
 * The caller needs to hold log->mutex.
 */
static void fix_up_readers(struct logger_log *log, size_t len)
{
	size_t old = log->w_off;
	size_t new = logger_offset(old + len);
	struct logger_reader *reader;

	if (clock_interval(old, new, log->head))
		log->head = get_next_entry(log, log->head, len);

	list_for_each_entry(reader, &log->readers, list)
		if (clock_interval(old, new, reader->r_off))
			reader->r_off = get_next_entry(log, reader->r_off, len);
}

/*
 * do_write_log - writes 'len' bytes from 'buf' to 'log'
 *
 * The caller needs to hold log->mutex.
 */
static void do_write_log(struct logger_log *log, const void *buf, size_t count)
{
	size_t len;

	len = min(count, log->size - log->w_off);
	memcpy(log->buffer + log->w_off, buf, len);

	if (count != len)
		memcpy(log->buffer, buf + len, count - len);

	log->w_off = logger_offset(log->w_off + count);

}

/*
 * do_write_log_user - writes 'len' bytes from the user-space buffer 'buf' to
 * the log 'log'
 *
 * The caller needs to hold log->mutex.
 *
 * Returns 'count' on success, negative error code on failure.
 */
static ssize_t do_write_log_from_user(struct logger_log *log,
				      const void __user *buf, size_t count)
{
	size_t len;

	len = min(count, log->size - log->w_off);
	if (len && copy_from_user(log->buffer + log->w_off, buf, len))
		return -EFAULT;

	if (count != len)
		if (copy_from_user(log->buffer, buf + len, count - len))
			return -EFAULT;

	log->w_off = logger_offset(log->w_off + count);

	return count;
}

/*
 * logger_aio_write - our write method, implementing support for write(),
 * writev(), and aio_write(). Writes are our fast path, and we try to optimize
 * them above all else.
 */
ssize_t logger_aio_write(struct kiocb *iocb, const struct iovec *iov,
			 unsigned long nr_segs, loff_t ppos)
{
	struct logger_log *log = file_get_log(iocb->ki_filp);
	size_t orig = log->w_off;
	struct logger_entry header;
	struct timespec now;
	ssize_t ret = 0;
#ifdef	CONFIG_MIX_LOGGER /* E_BOOK */
	struct logger_log*	mix_log = get_log_mix();
	bool	log_level_ok = true;
#endif /* E_BOOK */

	now = current_kernel_time();

#if 1 /* E_BOOK */
	header.__pad = 0;
#endif /* E_BOOK */
	header.pid = current->tgid;
	header.tid = current->pid;
	header.sec = now.tv_sec;
	header.nsec = now.tv_nsec;
	header.len = min_t(size_t, iocb->ki_left, LOGGER_ENTRY_MAX_PAYLOAD);

	/* null writes succeed, return zero */
	if (unlikely(!header.len))
		return 0;

#ifdef	CONFIG_MIX_LOGGER /* E_BOOK */
	while(mix_log) {
		const struct iovec*	mix_iov		= iov;
		unsigned long		mix_nr_segs	= nr_segs;
		size_t				mix_orig	= 0;
		struct logger_entry	mix_header;
		ssize_t				mix_ret		= 0;
		bool				mix_event	= (strncmp(log->misc.name, "log_events", 10) == 0);
		int					mix_next	= 0;

		mutex_lock(&mix_log->mutex);
		mix_orig = mix_log->w_off;

		memcpy(&mix_header, &header, sizeof(struct logger_entry));
		if(mix_event) {
			if(mix_log_level_android > MIXLOG_EVENT_LEVEL) {
				log_level_ok = false;
				mutex_unlock(&mix_log->mutex);
				break;
			}

			/*
			 * log_events (binary) -> Text Events
			 *   ---					-> level (1Bytes)        : MIXLOG_EVENT_LEVEL(4)
			 *   Event index (4Bytes)	-> Event name (14Bytes)  : "Event%08lx\0"
			 *   ---					-> logger name (15Bytes) : "[log_events  ] "
			 *   Event data (n Bytes)   -> Event data (n * 2 + 1 Bytes) (max : LOGGER_ENTRY_MAX_PAYLOAD * 2 + 1)
			 *
			 * no event data length : 31
			 */
			mix_header.len = (iocb->ki_left - sizeof(long)) * 2;	/* Event data */
			if(mix_header.len > (LOGGER_ENTRY_MAX_PAYLOAD - 32))
				mix_header.len = LOGGER_ENTRY_MAX_PAYLOAD - 32;
			mix_header.len += 31;
		} else {
			mix_header.len += 16;	/* add logger name length (16) and add last NULL length */
			if(mix_header.len >= LOGGER_ENTRY_MAX_PAYLOAD)
				mix_header.len = LOGGER_ENTRY_MAX_PAYLOAD;
		}

		fix_up_readers(mix_log, sizeof(struct logger_entry) + mix_header.len);
		do_write_log(mix_log, &mix_header, sizeof(struct logger_entry));

		while (mix_nr_segs-- > 0) {
			size_t len;
			ssize_t nr = -EFAULT;

			len = min_t(size_t, mix_iov->iov_len, mix_header.len - mix_ret);

			if(mix_event) {
				if(mix_next == 0) {
					char			buf[32];
					char*			p = &buf[1];
					size_t			index_len = min_t(size_t, len, sizeof(buf));
					unsigned long*	index = (unsigned long*)buf;
					unsigned long	index_data;

					if(copy_from_user(buf, mix_iov->iov_base, index_len) == 0) {
						index_data=*index;

						snprintf(p, sizeof(buf) - 1, "Event%08lx", index_data);
						buf[0] = MIXLOG_EVENT_LEVEL;
						do_write_log(mix_log, buf, strlen(p) + 2);
						nr = strlen(p) + 2;
						snprintf(buf, sizeof(buf), "[%-12s] ", log->misc.name);
						do_write_log(mix_log, buf, strlen(buf));
						nr += strlen(buf);
					} else {
						nr = -EFAULT;
					}
				} else {
					char			buf[4];
					size_t			i;
					size_t			remain = mix_header.len - 31;

					nr = 0;
					for(i = 0; i < len; i++, remain-=2) {
						unsigned char	c;
						if(remain < 3)
							break;
						if(copy_from_user(&c, mix_iov->iov_base + i, 1) == 0) {
							snprintf(buf, sizeof(buf), "%02x", c);
							do_write_log(mix_log, buf, strlen(buf));
							nr += strlen(buf);
						} else {
							nr = -EFAULT;
							break;
						}
					}
				}
			} else {
				if(mix_next == 0) {
					unsigned char	c = 0;
					if(copy_from_user(&c, mix_iov->iov_base, 1)) {
						mix_log->w_off = mix_orig;
						break;
					}
					if(mix_log_level_android > (int)c) {
						mix_log->w_off = mix_orig;
						log_level_ok = false;
						break;
					}
				}

				if(mix_next == 2) {
					len--;

					if((mix_ret + len) >= mix_header.len)
						len = mix_header.len - mix_ret - 1;
				}

				nr = do_write_log_from_user(mix_log, mix_iov->iov_base, len);
			}

			if (unlikely(nr < 0)) {
				mix_log->w_off = mix_orig;
				mix_ret = 0;
				break;
			}

			if(!mix_event && mix_next == 1) {
				char	buf[32];
				snprintf(buf, sizeof(buf), "[%-12s] ", log->misc.name);
				do_write_log(mix_log, buf, strlen(buf));
				nr += strlen(buf);
			}

			mix_next++;
			mix_iov++;
			mix_ret += nr;
		}

		if(mix_log->w_off != mix_orig) {
			for(; mix_ret < (size_t)mix_header.len; mix_ret++) {
				char	buf[4];
				buf[0]=0;
				do_write_log(mix_log, buf, 1);
			}
		}

		mutex_unlock(&mix_log->mutex);
		if(mix_ret > 0) {
			wake_up_interruptible(&mix_log->wq);
#ifdef CONFIG_PAST_LOG /* E_BOOK */
			past_logger_write();
#endif /* CONFIG_PAST_LOG */
		}
		break;
	}

	if(!log_level_ok) {
		// When log is controlled by a log level set by mix_log_level_android.
		// Control the log to begin to write to logger.
		ret = iov->iov_len;
		return ret;
	}
#endif /* E_BOOK */

	mutex_lock(&log->mutex);

	/*
	 * Fix up any readers, pulling them forward to the first readable
	 * entry after (what will be) the new write offset. We do this now
	 * because if we partially fail, we can end up with clobbered log
	 * entries that encroach on readable buffer.
	 */
	fix_up_readers(log, sizeof(struct logger_entry) + header.len);

	do_write_log(log, &header, sizeof(struct logger_entry));

	while (nr_segs-- > 0) {
		size_t len;
		ssize_t nr;

		/* figure out how much of this vector we can keep */
		len = min_t(size_t, iov->iov_len, header.len - ret);

		/* write out this segment's payload */
		nr = do_write_log_from_user(log, iov->iov_base, len);
		if (unlikely(nr < 0)) {
			log->w_off = orig;
			mutex_unlock(&log->mutex);
			return nr;
		}

		iov++;
		ret += nr;
	}

	mutex_unlock(&log->mutex);

	/* wake up any blocked readers */
	wake_up_interruptible(&log->wq);

	return ret;
}

static struct logger_log *get_log_from_minor(int);

/*
 * logger_open - the log's open() file operation
 *
 * Note how near a no-op this is in the write-only case. Keep it that way!
 */
static int logger_open(struct inode *inode, struct file *file)
{
	struct logger_log *log;
	int ret;

	ret = nonseekable_open(inode, file);
	if (ret)
		return ret;

	log = get_log_from_minor(MINOR(inode->i_rdev));
	if (!log)
		return -ENODEV;

	if (file->f_mode & FMODE_READ) {
		struct logger_reader *reader;

#ifdef CONFIG_PAST_LOG /* E_BOOK */
		bool bCanRead = past_can_read();
		if(log == get_log_past() &&
		   (log->size == 0 ||
		    !bCanRead)) {
			PASTLOG_DEBUG("log size=%u can_read=%s\n", log->size, bCanRead? "ok": "ng");
			return -EFAULT;
		}
#endif /* E_BOOK */

		reader = kmalloc(sizeof(struct logger_reader), GFP_KERNEL);
		if (!reader)
			return -ENOMEM;

		reader->log = log;
		INIT_LIST_HEAD(&reader->list);

		mutex_lock(&log->mutex);
		reader->r_off = log->head;
		list_add_tail(&reader->list, &log->readers);
		mutex_unlock(&log->mutex);

		file->private_data = reader;
#ifdef CONFIG_PAST_LOG_DEBUG /* E_BOOK */
		if(log == get_log_past()) {
			PASTLOG_DEBUG("past reader : %p\n", reader);
			LOCAL_DEBUG("log w_off=%u r_off=%u size=%u\n", log->w_off, reader->r_off, log->size);
		}
#endif /* E_BOOK */
#if 1 /* E_BOOK */
	} else {
#ifdef CONFIG_KERNEL_LOGGER
		if(log == get_log_kernel())
			return -EFAULT;
#endif /* CONFIG_KERNEL_LOGGER */
#ifdef CONFIG_MIX_LOGGER
		if(log == get_log_mix())
			return -EFAULT;
#endif /* CONFIG_MIX_LOGGER */
#ifdef CONFIG_PAST_LOG
		if(log == get_log_past()) {
			PASTLOG_DEBUG("write not support\n");
			return -EFAULT;
		}
#endif /* CONFIG_PAST_LOG */
		file->private_data = log;
	}
#else /* E_BOOK */
	} else
		file->private_data = log;
#endif /* E_BOOK */

	return 0;
}

/*
 * logger_release - the log's release file operation
 *
 * Note this is a total no-op in the write-only case. Keep it that way!
 */
static int logger_release(struct inode *ignored, struct file *file)
{
	if (file->f_mode & FMODE_READ) {
		struct logger_reader *reader = file->private_data;
		list_del(&reader->list);
		kfree(reader);
	}

	return 0;
}

/*
 * logger_poll - the log's poll file operation, for poll/select/epoll
 *
 * Note we always return POLLOUT, because you can always write() to the log.
 * Note also that, strictly speaking, a return value of POLLIN does not
 * guarantee that the log is readable without blocking, as there is a small
 * chance that the writer can lap the reader in the interim between poll()
 * returning and the read() request.
 */
static unsigned int logger_poll(struct file *file, poll_table *wait)
{
	struct logger_reader *reader;
	struct logger_log *log;
	unsigned int ret = POLLOUT | POLLWRNORM;

	if (!(file->f_mode & FMODE_READ))
		return ret;

	reader = file->private_data;
	log = reader->log;

	poll_wait(file, &log->wq, wait);

	mutex_lock(&log->mutex);
	if (log->w_off != reader->r_off)
		ret |= POLLIN | POLLRDNORM;
	mutex_unlock(&log->mutex);

	return ret;
}

static long logger_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct logger_log *log = file_get_log(file);
	struct logger_reader *reader;
	long ret = -ENOTTY;

	mutex_lock(&log->mutex);

	switch (cmd) {
	case LOGGER_GET_LOG_BUF_SIZE:
		ret = log->size;
#ifdef CONFIG_PAST_LOG_DEBUG /* E_BOOK */
		if(log == get_log_past()) {
			LOCAL_DEBUG("log buf size = %u\n", ret);
		}
#endif /* E_BOOK */
		break;
	case LOGGER_GET_LOG_LEN:
		if (!(file->f_mode & FMODE_READ)) {
			ret = -EBADF;
			break;
		}
		reader = file->private_data;
		if (log->w_off >= reader->r_off)
			ret = log->w_off - reader->r_off;
		else
			ret = (log->size - reader->r_off) + log->w_off;
#ifdef CONFIG_PAST_LOG_DEBUG /* E_BOOK */
		if(log == get_log_past()) {
			LOCAL_DEBUG("log len = %u\n", ret);
		}
#endif /* E_BOOK */
		break;
	case LOGGER_GET_NEXT_ENTRY_LEN:
		if (!(file->f_mode & FMODE_READ)) {
			ret = -EBADF;
			break;
		}
		reader = file->private_data;
#ifdef CONFIG_PAST_LOG /* E_BOOK */
		if(log == get_log_past()) {
			if (log->w_off != reader->r_off) {
				ret = get_past_entry_size(reader->r_off);
				LOCAL_DEBUG("next entry size = %u\n", ret);
			} else {
				ret = 0;
				LOCAL_DEBUG("next entry none\n");
			}
		} else
#endif /* E_BOOK */
		if (log->w_off != reader->r_off)
			ret = get_entry_len(log, reader->r_off);
		else
			ret = 0;
		break;
	case LOGGER_FLUSH_LOG:
		if (!(file->f_mode & FMODE_WRITE)) {
			ret = -EBADF;
			break;
		}
		list_for_each_entry(reader, &log->readers, list)
			reader->r_off = log->w_off;
		log->head = log->w_off;
		ret = 0;
		break;
	}

	mutex_unlock(&log->mutex);

	return ret;
}

static const struct file_operations logger_fops = {
	.owner = THIS_MODULE,
	.read = logger_read,
	.aio_write = logger_aio_write,
	.poll = logger_poll,
	.unlocked_ioctl = logger_ioctl,
	.compat_ioctl = logger_ioctl,
	.open = logger_open,
	.release = logger_release,
};

/*
 * Defines a log structure with name 'NAME' and a size of 'SIZE' bytes, which
 * must be a power of two, greater than LOGGER_ENTRY_MAX_LEN, and less than
 * LONG_MAX minus LOGGER_ENTRY_MAX_LEN.
 */
#define DEFINE_LOGGER_DEVICE(VAR, NAME, SIZE) \
static unsigned char _buf_ ## VAR[SIZE]; \
static struct logger_log VAR = { \
	.buffer = _buf_ ## VAR, \
	.misc = { \
		.minor = MISC_DYNAMIC_MINOR, \
		.name = NAME, \
		.fops = &logger_fops, \
		.parent = NULL, \
	}, \
	.wq = __WAIT_QUEUE_HEAD_INITIALIZER(VAR .wq), \
	.readers = LIST_HEAD_INIT(VAR .readers), \
	.mutex = __MUTEX_INITIALIZER(VAR .mutex), \
	.w_off = 0, \
	.head = 0, \
	.size = SIZE, \
};

#if 1 /* E_BOOK */
#define DEFINE_LOGGER_DEVICE_SIZE0(VAR, NAME) \
static struct logger_log VAR = { \
	.buffer = NULL, \
	.misc = { \
		.minor = MISC_DYNAMIC_MINOR, \
		.name = NAME, \
		.fops = &logger_fops, \
		.parent = NULL, \
	}, \
	.wq = __WAIT_QUEUE_HEAD_INITIALIZER(VAR .wq), \
	.readers = LIST_HEAD_INIT(VAR .readers), \
	.mutex = __MUTEX_INITIALIZER(VAR .mutex), \
	.w_off = 0, \
	.head = 0, \
	.size = 0, \
};
#endif /* E_BOOK */

DEFINE_LOGGER_DEVICE(log_main, LOGGER_LOG_MAIN, 64*1024)
DEFINE_LOGGER_DEVICE(log_events, LOGGER_LOG_EVENTS, 64*1024)
DEFINE_LOGGER_DEVICE(log_radio, LOGGER_LOG_RADIO, 64*1024)
DEFINE_LOGGER_DEVICE(log_system, LOGGER_LOG_SYSTEM, 64*1024)
#ifdef	CONFIG_MIX_LOGGER /* E_BOOK */
#ifdef MIX_LOGGER_OLD
DEFINE_LOGGER_DEVICE(log_mix, LOGGER_LOG_MIX, MIX_LOG_RESERVE_SIZE)
#endif /* MIX_LOGGER_OLD */
#endif /* E_BOOK */
#ifdef	CONFIG_KERNEL_LOGGER /* E_BOOK */
#ifdef KERNEL_LOGGER_OLD
DEFINE_LOGGER_DEVICE(log_kernel, LOGGER_LOG_KERNEL, KERNEL_LOG_SIZE)
#endif /* KERNEL_LOGGER_OLD */
#endif /* E_BOOK */
#ifdef CONFIG_PAST_LOG /* E_BOOK */
DEFINE_LOGGER_DEVICE_SIZE0(log_past, LOGGER_LOG_PAST)
#endif /* E_BOOK */

static struct logger_log *get_log_from_minor(int minor)
{
	if (log_main.misc.minor == minor)
		return &log_main;
	if (log_events.misc.minor == minor)
		return &log_events;
	if (log_radio.misc.minor == minor)
		return &log_radio;
	if (log_system.misc.minor == minor)
		return &log_system;
#ifdef	CONFIG_MIX_LOGGER /* E_BOOK */
#ifdef MIX_LOGGER_OLD
	if (log_mix.misc.minor == minor)
		return &log_mix;
#else /* MIX_LOGGER_OLD */
	if (mix_log_header &&
		mix_log_header->log.misc.minor == minor)
		return &mix_log_header->log;
#endif /* MIX_LOGGER_OLD */
#endif /* E_BOOK */
#ifdef	CONFIG_KERNEL_LOGGER /* E_BOOK */
	if (log_kernel.misc.minor == minor)
		return &log_kernel;
#endif /* E_BOOK */
#ifdef CONFIG_PAST_LOG /* E_BOOK */
	if (log_past.size != 0 &&
	    log_past.misc.minor == minor)
		return &log_past;
#endif /* E_BOOK */
	return NULL;
}

static int __init init_log(struct logger_log *log)
{
	int ret;

	ret = misc_register(&log->misc);
	if (unlikely(ret)) {
		printk(KERN_ERR "logger: failed to register misc "
		       "device for log '%s'!\n", log->misc.name);
		return ret;
	}

	printk(KERN_INFO "logger: created %luK log '%s'\n",
	       (unsigned long) log->size >> 10, log->misc.name);

	return 0;
}

static int __init logger_init(void)
{
	int ret;

	ret = init_log(&log_main);
	if (unlikely(ret))
		goto out;

	ret = init_log(&log_events);
	if (unlikely(ret))
		goto out;

	ret = init_log(&log_radio);
	if (unlikely(ret))
		goto out;

	ret = init_log(&log_system);
	if (unlikely(ret))
		goto out;

#ifdef	CONFIG_MIX_LOGGER /* E_BOOK */
#ifdef MIX_LOGGER_OLD
	ret = init_log(&log_mix);
#else /* MIX_LOGGER_OLD */
	ret = init_log(&mix_log_header->log);
#endif /* MIX_LOGGER_OLD */
	if (unlikely(ret))
		goto out;
#endif /* E_BOOK */

#ifdef	CONFIG_KERNEL_LOGGER /* E_BOOK */
	ret = init_log(&log_kernel);
	if (unlikely(ret))
		goto out;
	klogger_init = true;
	ret = klog_thread_init();
	if (unlikely(ret))
		goto out;
#ifdef KERNEL_LOGGER_OLD
	klog_init = true;
	printk(KERN_INFO "##### LOG START #####\n");
#endif /* KERNEL_LOGGER_OLD */
#endif /* E_BOOK */

out:
	return ret;
}
device_initcall(logger_init);

/*****************************************************************************/
#ifdef	CONFIG_MIX_LOGGER /* E_BOOK */
#ifdef	CONFIG_MIX_LOGGER_STATIC
void mix_log_set_mem_addr(resource_size_t addr, struct resource* res)
{
	if (!mix_log_res) {
		res->start	= addr;
		res->end	= addr + MIX_LOG_RESERVE_SIZE - 1;
		request_resource(&iomem_resource, res);

		mix_log_res = res;
	}
}
#endif /* CONFIG_MIX_LOGGER_STATIC */

void mix_log_mem_init(void)
{
#ifndef MIX_LOGGER_OLD
	unsigned char*		mix_buffer	= NULL;
	size_t				mix_size	= 0;
	size_t				mix_h_len	= sizeof(MIX_LOG_HEAD);
	MIX_LOG_HEAD*		mix_header	= NULL;

#ifdef CONFIG_MIX_LOGGER_STATIC
	bool				bStaticLogOK = true;
	if (!mix_log_res) {
		printk(KERN_ERR "Cannot start static memory mix log.\n");
		return;
	} else if (mix_log_addr) {
		printk(KERN_WARNING "Finished with static memory mix log initialization.");
		return;
	}

	mix_log_addr	= (char*)ioremap((phys_addr_t)mix_log_res->start, MIX_LOG_RESERVE_SIZE);

	printk(KERN_INFO "Mix log static memory : %08lx - %08lx\n", (unsigned long)mix_log_res->start, (unsigned long)mix_log_res->end);
	printk(KERN_INFO "              address : %p\n", mix_log_addr);
#else /* CONFIG_MIX_LOGGER_STATIC */
	printk(KERN_INFO "Mix log address : %p\n", mix_log_addr);
#endif /* CONFIG_MIX_LOGGER_STATIC */

	mix_header		= (MIX_LOG_HEAD*)mix_log_addr;
	mix_buffer		= (unsigned char*)(mix_log_addr + mix_h_len);
	mix_size		= MIX_LOG_RESERVE_SIZE - mix_h_len - 1;

	if(mix_header->magic != MIXLOG_MAGIC ||
	   mix_header->log.buffer != mix_buffer ||
	   mix_header->log.size != mix_size) {
		printk(KERN_INFO "  There is not the last log.\n");
		bStaticLogOK = false;
	} else {
		printk(KERN_INFO "  The last log is left.\n");
	}

	if(bStaticLogOK &&
	   mix_header->log.w_off >= mix_size) {
		printk(KERN_INFO "    A writing starting position is abnormal.\n");
		bStaticLogOK = false;
	}

	if(bStaticLogOK &&
	   mix_header->log.head >= mix_size) {
		printk(KERN_INFO "    A reading starting position is abnormal.\n");
		bStaticLogOK = false;
	}

#ifdef CONFIG_PAST_LOG
	if(bStaticLogOK) {
		int	pattarn = 0;
		if(mix_header->reader.r_off >= mix_size) {
			/*	|-----------------|---r	*/
			pattarn = 1;
		} else	if(mix_header->log.w_off > mix_header->log.head) {
			if(mix_header->reader.r_off >= mix_header->log.w_off ||
			   mix_header->reader.r_off < mix_header->log.head) {
				/*	|-r--h-------w--r-|	*/
				pattarn = 2;
			}
		} else	if(mix_header->reader.r_off >= mix_header->log.w_off &&
				   mix_header->reader.r_off < mix_header->log.head) {
			/*	|----w---r---h----|	*/
			pattarn = 3;
		}

		if(pattarn) {
			printk(KERN_INFO "    The reading starting position that does not store eMMC is abnormal.(%d)\n", pattarn);
			bStaticLogOK = false;
		}
	}
#endif /* CONFIG_PAST_LOG */

	if(bStaticLogOK) {
		memset(&mix_header->log.misc, 0, sizeof(struct miscdevice));
	} else {
		printk(KERN_INFO "Initialize a static memory for mix_logger.\n");
		memset(mix_log_addr, 0, MIX_LOG_RESERVE_SIZE);

		mix_header->magic			= MIXLOG_MAGIC;
		mix_header->log.buffer		= mix_buffer;
		mix_header->log.w_off		= 0;
		mix_header->log.head		= 0;
		mix_header->log.size		= mix_size;
#ifdef CONFIG_PAST_LOG
		mix_header->reader.r_off	= mix_header->log.head;
#endif /* CONFIG_PAST_LOG */
	}

	printk(KERN_DEBUG	"  w_off  = %u\n"
						"  head   = %u\n"
#ifdef CONFIG_PAST_LOG
						"  reader = %u\n"
#endif /* CONFIG_PAST_LOG */
						"  size   = %u\n"
						"  header = %p\n"
						"  buffer = %p\n",
			mix_header->log.w_off,
			mix_header->log.head,
#ifdef CONFIG_PAST_LOG
			mix_header->reader.r_off,
#endif /* CONFIG_PAST_LOG */
			mix_header->log.size,
			mix_header,
			mix_header->log.buffer);

	mix_header->log.misc.name	= LOGGER_LOG_MIX;
	mix_header->log.misc.minor	= MISC_DYNAMIC_MINOR;
	mix_header->log.misc.fops	= &logger_fops;
	mix_header->log.misc.parent	= NULL;
	init_waitqueue_head(&mix_header->log.wq);
	INIT_LIST_HEAD(&mix_header->log.readers);
	mutex_init(&mix_header->log.mutex);

	mix_log_addr[MIX_LOG_RESERVE_SIZE - 1] = 0;

#ifdef CONFIG_PAST_LOG
	mix_header->reader.log = &mix_header->log;
	INIT_LIST_HEAD(&mix_header->reader.list);
	list_add_tail(&mix_header->reader.list, &mix_header->log.readers);
#endif /* CONFIG_PAST_LOG */

	mix_log_header = mix_header;
#endif /* MIX_LOGGER_OLD */
}

#ifdef CONFIG_PAST_LOG
size_t	mix2past_read_entry(char* buffer, size_t size)
{
	size_t					ret		= 0;
#ifndef MIX_LOGGER_OLD
	struct logger_reader*	reader	= NULL;
	struct logger_log*		log		= NULL;
	size_t					len		= 0;

	if(!mix_log_header)
		return	ret;

	reader	= &mix_log_header->reader;
	log		= reader->log;

	mutex_lock(&log->mutex);

	/* is there still something to read or did we race? */
	if (unlikely(log->w_off == reader->r_off)) {
		goto out;
	}

	/* get the size of the next entry */
	len = get_entry_len(log, reader->r_off);
	if(len > LOGGER_ENTRY_MAX_LEN) {
		ret = -EINVAL;
		PASTLOG_DEBUG("This entry skips. offset=%u size=%u\n", reader->r_off, len);
	} else if (size < len) {
		ret = -EFAULT;
		goto out;
	} else {
		/* get exactly one entry from the log */
		ret = min(len, log->size - reader->r_off);
		memcpy(buffer, log->buffer + reader->r_off, ret);

		if (len != ret) {
			memcpy(buffer + len, log->buffer, len - ret);
			ret += (len - ret);
		}
	}

	reader->r_off = logger_offset(reader->r_off + len);

out:
	mutex_unlock(&log->mutex);

#endif /* MIX_LOGGER_OLD */
	return ret;
}
#endif /* CONFIG_PAST_LOG */

static inline struct logger_log *get_log_mix(void)
{
#ifdef MIX_LOGGER_OLD
	return &log_mix;
#else /* MIX_LOGGER_OLD */
	if(!mix_log_header)
		return NULL;
	return &mix_log_header->log;
#endif /* MIX_LOGGER_OLD */
}
#endif /* E_BOOK */

/*****************************************************************************/
#ifdef	CONFIG_KERNEL_LOGGER /* E_BOOK */
static char*				kernel_log_next = NULL;
static struct semaphore		klog_next_sem;
static struct task_struct*	klog_thread_data = NULL;
static bool					bStackLog = false;
static char					szStackLog[LOGGER_ENTRY_MAX_LEN];

static int kernel_level(int level, char* p)
{
	if(level < 0 || level > 7)
		level = default_message_loglevel;

	switch(level)
	{
	case 0:		/* KERN_EMERG	:	system is unusable */
	case 1:		/* KERN_ALERT	:	action must be taken immediately */
	case 2:		/* KERN_CRIT	:	critical conditions */
		*p = 7;	/* Fatal */
		break;
	case 3:		/* KERN_ERR		:	error conditions */
		*p = 6;	/* Error */
		break;
	case 4:		/* KERN_WARNING	:	warning conditions */
		*p = 5;	/* Werning */
		break;
	case 5:		/* KERN_NOTICE	:	normal but significant condition */
	case 6:		/* KERN_INFO	:	informational */
		*p = 4;	/* Info */
		break;
	case 7:		/* KERN_DEBUG	:	debug-level messages */
		*p = 3;	/* Debug */
		break;
	}

	return level;
}

static void kernel2logger(struct logger_entry* ent, int level, char lvl, const char* klog, const char* tag)
{
	struct logger_entry*	header	= (struct logger_entry*)szStackLog;
	struct logger_log*		log		= get_log_kernel();
	size_t					left	= strlen(klog);
	size_t					remain	= LOGGER_ENTRY_MAX_PAYLOAD - 1;
	char*					p		= &szStackLog[sizeof(struct logger_entry)];
	size_t					taglen	= strlen(tag) + 1;
	size_t					headlen	= taglen + 1;
	size_t					i;

	if(!klog_init)
		return;

	if(!bStackLog) {
		memcpy(header, ent, sizeof(struct logger_entry));
		*p = lvl;
		memcpy(p + 1, tag, taglen);
		header->len = headlen;

		bStackLog = true;
	}
	p		+= header->len;
	remain	-= header->len;

	/* emit */
	for(i = 0; i < left; i++, klog++) {
		if(!bStackLog) {
			header->len = headlen;
			p		= &szStackLog[sizeof(struct logger_entry) + header->len];
			remain	= LOGGER_ENTRY_MAX_PAYLOAD - header->len - 1;
			bStackLog = true;
		}

		*p = (*klog == '\n')? 0: *klog;
		header->len++;

		remain--;
		if(remain <= 0) {
			p++;
			*p = 0;
			header->len++;
		}

		if(*p == 0) {
#ifdef	CONFIG_MIX_LOGGER
			struct logger_log*	mix_log = get_log_mix();
			if(mix_log &&
			   level <= mix_log_level_kernel) {
				mutex_lock(&mix_log->mutex);
				fix_up_readers(mix_log, sizeof(struct logger_entry) + header->len);
				do_write_log(mix_log, header, sizeof(struct logger_entry) + header->len);
#ifdef CONFIG_PAST_LOG
				past_logger_write();
#endif /* CONFIG_PAST_LOG */
				mutex_unlock(&mix_log->mutex);
				wake_up_interruptible(&mix_log->wq);
			}
#endif /* CONFIG_MIX_LOGGER */

			mutex_lock(&log->mutex);

			fix_up_readers(log, sizeof(struct logger_entry) + header->len);
			do_write_log(log, header, sizeof(struct logger_entry) + header->len);

			mutex_unlock(&log->mutex);
			wake_up_interruptible(&log->wq);

			bStackLog = false;
		} else {
			p++;
		}
	}
}

static bool klog_put(void)
{
	char*			next_top = NULL;
	unsigned int*	nxt = NULL;
	bool			ret = false;
	unsigned long	flag;

	local_irq_save(flag);
	down(&klog_next_sem);
	if(kernel_log_next) {
		nxt = (unsigned int*)kernel_log_next;
		next_top = kernel_log_next;
		kernel_log_next = (char*)*nxt;
	}
	up(&klog_next_sem);
	local_irq_restore(flag);

	if(next_top) {
		struct logger_entry*	ent		= (struct logger_entry*)&next_top[sizeof(unsigned int)];
		int*					klv		= (int*)&next_top[sizeof(unsigned int) + sizeof(struct logger_entry)];
		char*					lvl		= &next_top[sizeof(unsigned int) + sizeof(struct logger_entry) + sizeof(int)];
		char*					tag		= lvl + 1;
		size_t					taglen	= strlen(tag) + 1;
		char*					klog	= &tag[taglen];

		kernel2logger(ent, *klv, *lvl, klog, tag);

		ret = (*nxt != 0);
		kfree(next_top);
		next_top = NULL;
	} else {
		ret = false;
	}

	return ret;
}

static int klog_thread(void* data)
{
	while(!kthread_should_stop()) {
		bool			next_proc = false;

		do {
			next_proc = klog_put();
			schedule();
		} while(next_proc);

		set_current_state(TASK_INTERRUPTIBLE);
	}

	return 0;
}

void	kernel_log_mem_init(void)
{
#ifndef KERNEL_LOGGER_OLD
	log_kernel.buffer		= _buf_log_kernel;
	log_kernel.misc.name	= LOGGER_LOG_KERNEL;
	log_kernel.misc.minor	= MISC_DYNAMIC_MINOR;
	log_kernel.misc.fops	= &logger_fops;
	log_kernel.misc.parent	= NULL;
	log_kernel.w_off		= 0;
	log_kernel.head			= 0;
	log_kernel.size			= sizeof(_buf_log_kernel);
	init_waitqueue_head(&log_kernel.wq);
	INIT_LIST_HEAD(&log_kernel.readers);
	mutex_init(&log_kernel.mutex);
	sema_init(&klog_next_sem, 1);

	klog_init = true;
	printk(KERN_INFO "##### LOG START #####\n");
#endif /* KERNEL_LOGGER_OLD */
}

void addtag2logger(int level, const char* klog, const char* tag)
{
	if(!klogger_thread_init) {
		if(klogger_init) {
			klog_thread_init();
		}
	}

	if(klogger_thread_init) {
		size_t					kln = strlen(klog);
		size_t					len = kln + sizeof(struct logger_entry) + 32;
		char*					buf = kmalloc(len, GFP_KERNEL);
		struct timespec			now = current_kernel_time();
		unsigned int*			nxt;
		struct logger_entry*	ent;
		char*					p;
		int*					klv;
		size_t					taglen	= strlen(tag) + 1;

		if(!buf)
			return;

		nxt = (unsigned int*)buf;
		ent = (struct logger_entry*)&buf[sizeof(unsigned int)];
		klv = (int*)&buf[sizeof(unsigned int) + sizeof(struct logger_entry)];
		p	= &buf[sizeof(unsigned int) + sizeof(struct logger_entry) + sizeof(int)];

		*nxt       = 0;
		ent->__pad = 0;
		ent->pid   = current->tgid;
		ent->tid   = current->pid;
		ent->sec   = now.tv_sec;
		ent->nsec  = now.tv_nsec;
		ent->len   = 0;

		*klv = kernel_level(level, p);
		memcpy(p + 1, tag, taglen);
		strncpy(p + 1 + taglen, klog, len - (sizeof(unsigned int) + sizeof(struct logger_entry) + sizeof(int) + 1 + taglen));

		down(&klog_next_sem);
		if(kernel_log_next) {
			char*	last = kernel_log_next;
			while(last) {
				unsigned int*	last_nxt = (unsigned int*)last;
				if(!*last_nxt) {
					*last_nxt = (unsigned int)buf;
					break;
				}
				last = (char*)*last_nxt;
			}
			if(!last) {
				kernel_log_next = buf;
			}
		} else {
			kernel_log_next = buf;
		}
		up(&klog_next_sem);

		wake_up_process(klog_thread_data);
	} else {
		struct logger_entry	ent;
		char				lvl = 0;
		struct timespec		now = current_kernel_time();

		ent.__pad = 0;
		ent.pid   = current->tgid;
		ent.tid   = current->pid;
		ent.sec   = now.tv_sec;
		ent.nsec  = now.tv_nsec;
		ent.len   = 0;

		level = kernel_level(level, &lvl);

		kernel2logger(&ent, level, lvl, klog, tag);
	}
}

void printk2logger(int level, const char* klog)
{
	addtag2logger(level, klog, "Kernel ");
}

void pastdev2logger(const char* klog)
{
	addtag2logger(default_message_loglevel, klog, "PastDev");
}

void klog_flush(void)
{
	bool			next_proc = false;

	do {
		next_proc = klog_put();
	} while(next_proc);
}

static inline struct logger_log *get_log_kernel(void)
{
	return &log_kernel;
}

static int klog_thread_init(void)
{
	int	ret = 0;

	if(!klog_thread_data) {
		klog_thread_data = kthread_run(klog_thread, NULL, "klog_thread");
		if (IS_ERR(klog_thread_data)) {
			ret = PTR_ERR(klog_thread_data);
			printk(KERN_ERR "  cannot run kernel log thread : %d\n", ret);
			klog_thread_data = NULL;
		} else {
			klogger_thread_init = true;
		}
	}

	return ret;
}
#endif /* E_BOOK */

/*****************************************************************************/
#ifdef CONFIG_PAST_LOG /* E_BOOK */
void past_logger_init(void)
{
	struct logger_log*	log = get_log_past();

	if(log->size == 0) {
		int	ret;

		log->size = get_pastlog_size();
		if(log->size == 0)
			return;

		ret = init_log(&log_past);
		if(unlikely(ret)) {
			log->size = 0;
			return;
		}

		set_past_logger((void*)log);
	}
}

static void past_logger_write(void)
{
	if(past_delay_write() < 0)
		return;
	past_logger_init();
}

static inline struct logger_log *get_log_past(void)
{
	return &log_past;
}
#endif /* E_BOOK */
