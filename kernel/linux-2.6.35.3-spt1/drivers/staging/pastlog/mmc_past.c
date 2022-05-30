#if 1 /* E_BOOK */
/**
 * For Portable Reader System
 * 		past log mmc access
 *
 * file	mmc_past.c
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
#include <linux/slab.h>
#include <linux/mmc/rawdatatable.h>
#include <linux/pastlog.h>
#include "../android/logger.h"

#ifdef CONFIG_HAS_WAKELOCK /* E_BOOK */
#include <linux/wakelock.h>
#endif /* E_BOOK */

#define	PAST_SIZE_SMALL_DEBUG	0
//#define	PAST_SIZE_SMALL_DEBUG	(512 * 1024)
//#define	PAST_EVERYTIME_INIT
/*****************************************************************************
 * Defines
 */
#ifdef CONFIG_PAST_LOG_DEBUG
#define	LOCAL_DEBUG(fmt, arg...)		PASTLOG_DEBUG(fmt, ##arg)
//#define	LOCAL_DEBUG(fmt, arg...)
#else /* CONFIG_PAST_LOG_DEBUG */
#define	LOCAL_DEBUG(fmt, arg...)
//#define	LOCAL_DEBUG(fmt, arg...)		pastlog_degug(KERN_WARNING "[PASTLOG Info:%s(%d)]" fmt, __FUNCTION__, __LINE__, ##arg);
#endif /* CONFIG_PAST_LOG_DEBUG */

#define	PAST_MMC_HEADER_POS1	0
#define	PAST_MMC_HEADER_POS2	4096
#define	PAST_MMC_DATA_START		8192

#define PAST_MAGIC1				0x4C4B5045	/* "EBPL" */
#define PAST_MAGIC2				0x32706265	/* "2pbe" */
#define	PAST_VERSION			0x30313032	/* "0102" */

#define	PAST_CACHE_BUFSIZE		8192
#define	PAST_CACHE_VIEWTOP		512
#define	PAST_CACHE_VIEWSIZE		LOGGER_ENTRY_MAX_LEN
#define	PAST_CACHE_VIEWMAX		(PAST_CACHE_BUFSIZE - PAST_CACHE_VIEWTOP)

typedef	struct
{
	unsigned long	pos;
	unsigned long	sec;
	char*			buf;
}	PAST_CACHE_DATA;

typedef	struct past_header {
	unsigned long	magic1;
	unsigned long	w_pos;
	unsigned long	w_max;
	unsigned long	reader;
	unsigned char	reserved[512 - (sizeof(unsigned long) * 8)];
	unsigned long	version;
	unsigned long	offset;
	unsigned long	size;
	unsigned long	magic2;
} PAST_HEADER;

#define	PAST_MMC_OPEN	((past_mmc_log_index = rawdata_index(RAWLABEL_LOG, &past_mmc_log_size)) == (unsigned long)-1)
#define	PAST_MMC_WRITE(pos, size, getsize, buf)		\
	((unsigned long)(*getsize = rawdata_write(past_mmc_log_index, (unsigned long)pos, (char*)buf, (unsigned long)size)) != (unsigned long)size)
#define	PAST_MMC_READ(pos, size, getsize, buf)		\
	((unsigned long)(*getsize = rawdata_read(past_mmc_log_index, (unsigned long)pos, (char*)buf, (unsigned long)size)) != (unsigned long)size)
#define	PAST_DATA_WRITE(pos, size, getsize, buf)	PAST_MMC_WRITE(past_mmc_header.offset + pos, size, getsize, buf)
#define	PAST_DATA_READ(pos, size, getsize, buf)		PAST_MMC_READ(past_mmc_header.offset + pos, size, getsize, buf)

/*****************************************************************************
 * Static Variables
 */
static	unsigned long		past_mmc_log_index	= (unsigned long)-1;
static	unsigned long		past_mmc_log_size	= 0;
static	PAST_HEADER			past_mmc_header;
static	bool				past_mmc_init		= false;
static	bool				past_mmc_access 	= false;

static	unsigned long		past_size			= 0;
static	PAST_CACHE_DATA		past_acs_cache_r	= { (unsigned long)-1, (unsigned long)-1, NULL };
static	PAST_CACHE_DATA		past_acs_cache_w	= { (unsigned long)-1, (unsigned long)-1, NULL };

static	struct mutex		past_access_mutex;

static	struct logger_log*	log_past			= NULL;

#ifdef CONFIG_HAS_WAKELOCK /* E_BOOK */
static	struct wake_lock	past_wake_lock;
#endif /* E_BOOK */

static	unsigned long	ph_w_pos = 0;
static	unsigned long	ph_w_max = 0;
static	unsigned long	ph_reader = 0;
static	unsigned long	ph_offset = 0;
static	unsigned long	ph_size = 0;

module_param(ph_w_pos, long, S_IRUGO);
module_param(ph_w_max, long, S_IRUGO);
module_param(ph_reader, long, S_IRUGO);
module_param(ph_offset, long, S_IRUGO);
module_param(ph_size, long, S_IRUGO);

/*****************************************************************************
 * Functions
 */
int past_can_write(void)
{
	return (past_mmc_init && !past_mmc_access);
}

size_t get_pastlog_size(void)
{
	return	(size_t)past_size;
}

void set_past_logger(void* log)
{
	mutex_lock(&past_access_mutex);
	log_past = (struct logger_log*)log;
	log_past->head	= (size_t)past_mmc_header.reader;
	log_past->w_off	= (size_t)past_mmc_header.w_pos;
	mutex_unlock(&past_access_mutex);
}

static	void copy_header_info(void)
{
	ph_w_pos  = (long)past_mmc_header.w_pos;
	ph_w_max  = (long)past_mmc_header.w_max;
	ph_reader = (long)past_mmc_header.reader;
	ph_offset = (long)past_mmc_header.offset;
	ph_size   = (long)past_mmc_header.size;
}

static unsigned long get_buf_size(char* buf)
{
	unsigned long	ret = 0;

	if(buf >= past_acs_cache_r.buf &&
	   buf <= &past_acs_cache_r.buf[PAST_CACHE_BUFSIZE]) {
		ret = (unsigned long)(&past_acs_cache_r.buf[PAST_CACHE_BUFSIZE + 1] - buf);
	} else	if(buf >= past_acs_cache_w.buf &&
			   buf <= &past_acs_cache_w.buf[PAST_CACHE_BUFSIZE]) {
		ret = (unsigned long)(&past_acs_cache_w.buf[PAST_CACHE_BUFSIZE + 1] - buf);
	}

	return	ret;
}

static bool past_read_cache(PAST_CACHE_DATA* cache, unsigned long pos, unsigned long size)
{
	unsigned long	sec  = 0;
	unsigned long	nsec = 0;

	LOCAL_DEBUG("pos=%lu size=%lu\n", pos, size);

	if(size > PAST_CACHE_VIEWMAX)
		return	false;

	sec  = pos / 512;
	nsec = ((pos + size + 511) / 512) - sec;

	if(cache->sec == (unsigned long)-1 ||
	   sec < cache->sec ||
	   (sec + nsec - 1) > (cache->sec + (PAST_CACHE_BUFSIZE / 512) - 1)) {
		unsigned long	read_top  = sec * 512;
		unsigned long	read_size = PAST_CACHE_BUFSIZE;
		int				rsize     = 0;
		char*			read_buf  = cache->buf;
		if(pos >= PAST_CACHE_VIEWTOP) {
			read_top -= PAST_CACHE_VIEWTOP;
		}

		if((read_top + read_size) >= past_size) {
			unsigned long	div_top1  = read_top;
			unsigned long	div_size1 = past_size - read_top;
			unsigned long	div_top2  = 0;
			unsigned long	div_size2 = read_size - div_size1;
			LOCAL_DEBUG("read1 pos=%lu size=%lu\n", div_top1, div_size1);
			LOCAL_DEBUG("read2 pos=%lu size=%lu\n", div_top2, div_size2);
			if(PAST_DATA_READ(div_top1, div_size1, &rsize, read_buf) ||
			   (div_size2 > 0 &&
			    PAST_DATA_READ(div_top2, div_size2, &rsize, &read_buf[div_size1]))) {
				PASTLOG_ERR("read error : pos=%lu size=%lu offset=%lu\n", pos, size, read_top);
				cache->pos = (unsigned long)-1;
				cache->sec = (unsigned long)-1;
				return	NULL;
			}
		} else {
			LOCAL_DEBUG("read pos=%lu size=%lu\n", read_top, read_size);
			if(PAST_DATA_READ(read_top, read_size, &rsize, read_buf)) {
				PASTLOG_ERR("read error : pos=%lu size=%lu offset=%lu\n", pos, size, read_top);
				cache->pos = (unsigned long)-1;
				cache->sec = (unsigned long)-1;
				return	NULL;
			}
		}

		cache->pos = read_top;
		cache->sec = read_top / 512;
	}

	return	true;
}

static char* past_read(unsigned long pos, unsigned long size)
{
	if(!past_read_cache(&past_acs_cache_r, pos, size))
		return	NULL;

	LOCAL_DEBUG("cache pos=%lu\n", pos - past_acs_cache_r.pos);
	return	&past_acs_cache_r.buf[pos - past_acs_cache_r.pos];
}

static bool past_write(unsigned long pos, unsigned long size, const char* buf)
{
	unsigned long	sec  = pos / 512;
	unsigned long	nsec = ((pos + size + 511) / 512) - sec;
	unsigned long	write_top  = sec * 512;
	unsigned long	write_size = nsec * 512;
	int				wsize      = 0;
	char*			write_buf  = NULL;

	if(!past_read_cache(&past_acs_cache_w, pos, size))
		return	false;

	LOCAL_DEBUG("cache copy pos=%lu\n", pos - past_acs_cache_w.pos);
	memcpy(&past_acs_cache_w.buf[pos - past_acs_cache_w.pos], buf, size);
	LOCAL_DEBUG("cache write pos=%lu\n", write_top - past_acs_cache_w.pos);
	write_buf = &past_acs_cache_w.buf[write_top - past_acs_cache_w.pos];

	if((write_top + write_size) >= past_size) {
		unsigned long	div_top1  = write_top;
		unsigned long	div_size1 = past_size - write_top;
		unsigned long	div_top2  = 0;
		unsigned long	div_size2 = write_size - div_size1;
		LOCAL_DEBUG("write1 pos=%lu size=%lu\n", div_top1, div_size1);
		LOCAL_DEBUG("write2 pos=%lu size=%lu\n", div_top2, div_size2);
		if(PAST_DATA_WRITE(div_top1, div_size1, &wsize, write_buf) ||
		   (div_size2 > 0 &&
		    PAST_DATA_WRITE(div_top2, div_size2, &wsize, &write_buf[div_size1]))) {
			PASTLOG_ERR("write error : pos=%lu size=%lu offset=%lu\n", pos, size, write_top);
			past_acs_cache_w.pos = (unsigned long)-1;
			past_acs_cache_w.sec = (unsigned long)-1;
			return	false;
		}
	} else {
		LOCAL_DEBUG("write pos=%lu size=%lu\n", write_top, write_size);
		if(PAST_DATA_WRITE(write_top, write_size, &wsize, write_buf)) {
			PASTLOG_ERR("write error : pos=%lu size=%lu offset=%lu\n", pos, size, write_top);
			past_acs_cache_w.pos = (unsigned long)-1;
			past_acs_cache_w.sec = (unsigned long)-1;
			return	false;
		}
	}

	return	true;
}

static bool past_mmc_payload_check(char* buf, unsigned long size)
{
	unsigned long	ent_lvl_pos = sizeof(struct logger_entry);
	unsigned long	ent_pay_len = (unsigned long)*((__u16*)buf);
	unsigned long	ent_nxt_pos = ent_lvl_pos + ent_pay_len;

	if(ent_lvl_pos < size &&
	   buf[ent_lvl_pos] >= 0 &&
	   buf[ent_lvl_pos] <= 8 &&
	   ent_pay_len >= 2 &&
	   ent_pay_len <= LOGGER_ENTRY_MAX_PAYLOAD &&
	   *((__u16*)(buf + sizeof(__u16))) == 0 &&
	   (ent_nxt_pos - 1) < size &&
	   buf[ent_nxt_pos - 1] == 0) {
		return true;
	}

	return	false;
}

static char* past_mmc_search_payload(unsigned long r_top, unsigned long* off)
{
	unsigned long	r_cnt = 0;
	char*			r_buf = NULL;

	for(r_cnt = 0; r_cnt < (16 * 1024); r_cnt++) {
		*off = r_top + r_cnt;
		*off %= past_size;

		r_buf = past_read(*off, LOGGER_ENTRY_MAX_LEN);
		if(!r_buf) {
			PASTLOG_ERR("Read error : 0x%lx (%u)\n", off, LOGGER_ENTRY_MAX_LEN);
			return r_buf;
		}

		if(past_mmc_payload_check(r_buf, get_buf_size(r_buf)))
			return r_buf;
	}
	r_buf = NULL;

	PASTLOG_ERR("Reader check over\n");
	return	r_buf;
}

static bool past_mmc_reader_check(bool* bChange)
{
	unsigned long	off = 0;

	*bChange = false;
	if(past_mmc_header.w_max < past_size) {
		*bChange = (past_mmc_header.reader != 0);
		if(*bChange)
			PASTLOG_WARN("Reader set 0x0 <- (0x%lx)\n", past_mmc_header.reader);
		past_mmc_header.reader = 0;
		return true;
	}

	if(past_mmc_search_payload(past_mmc_header.reader, &off)) {
		*bChange = (past_mmc_header.reader != off);
		if(*bChange)
			PASTLOG_WARN("Reader set 0x%lx <- (0x%lx)\n", off, past_mmc_header.reader);
		past_mmc_header.reader = off;
		return true;
	}

	return false;
}

size_t get_past_entry_size(size_t offset)
{
	size_t			ret		= 0;
	char*			r_buf	= NULL;

	if(!log_past)
		mutex_lock(&past_access_mutex);

	r_buf = past_read(offset, sizeof(__u16));
	if(r_buf)
		ret = *((__u16*)r_buf) + sizeof(struct logger_entry);

	if(!log_past)
		mutex_unlock(&past_access_mutex);

	return	ret;
}

static inline struct mutex* get_mutex(void)
{
	return log_past? &log_past->mutex: &past_access_mutex;
}

#ifndef PAST_EVERYTIME_INIT
static int past_mmc_header_read(void)
{
	int		rsize = 0;

	if(!PAST_MMC_READ(PAST_MMC_HEADER_POS1, sizeof(PAST_HEADER), &rsize, &past_mmc_header) &&
	   past_mmc_header.magic1 == PAST_MAGIC1 &&
	   past_mmc_header.magic2 == PAST_MAGIC2 &&
	   past_mmc_header.version == PAST_VERSION &&
	   past_mmc_header.offset == PAST_MMC_DATA_START &&
	   past_mmc_header.size   == past_mmc_log_size)
		return 0;

	PASTLOG_ERR("pos1 header error\n");
	if(PAST_MMC_READ(PAST_MMC_HEADER_POS2, sizeof(PAST_HEADER), &rsize, &past_mmc_header)) {
		PASTLOG_ERR("pos2 header read error\n");
		return -1;	/* cannot read */
	}

	if(past_mmc_header.magic1 != PAST_MAGIC1 ||
	   past_mmc_header.magic2 != PAST_MAGIC2 ||
	   past_mmc_header.version != PAST_VERSION ||
	   past_mmc_header.offset != PAST_MMC_DATA_START ||
	   past_mmc_header.size   != past_mmc_log_size) {
		PASTLOG_WARN("not initialized\n");
		return -2;	/* not initialized */
	}

	return 0;
}
#endif /* PAST_EVERYTIME_INIT */

static int past_mmc_header_write(void)
{
	int		wsize = 0;

	copy_header_info();

	if(PAST_MMC_WRITE(PAST_MMC_HEADER_POS1, sizeof(PAST_HEADER), &wsize, &past_mmc_header)) {
		PASTLOG_ERR("pos1 header write error\n");
		return -1;
	}
	if(PAST_MMC_WRITE(PAST_MMC_HEADER_POS2, sizeof(PAST_HEADER), &wsize, &past_mmc_header)) {
		PASTLOG_ERR("pos2 header write error\n");
		return -1;
	}

	LOCAL_DEBUG("write header : w_pos=%lu w_max=%lu reader=%lu offset=%lu size=%lu\n",
					past_mmc_header.w_pos,
					past_mmc_header.w_max,
					past_mmc_header.reader,
					past_mmc_header.offset,
					past_mmc_header.size);
	return 0;
}

static bool past_search_reader(unsigned long new_pos, unsigned long* reader)
{
	unsigned long	r_org	= *reader;
	bool			bNext	= true;

	if(past_mmc_header.w_max < past_size &&
	   past_mmc_header.w_max < new_pos) {
		LOCAL_DEBUG("w_max(%lu) < past_size(%lu) && wmax(%lu) < new_pos(%lu)\n", past_mmc_header.w_max, past_size, past_mmc_header.w_max, new_pos);
		return	true;
	}

	LOCAL_DEBUG("start=%lu new_pos=%lu\n", *reader, new_pos);
	do {
		unsigned long	pay_len;
		char*			r_buf;

		LOCAL_DEBUG("read pos=%lu\n", *reader);
		r_buf = past_read(*reader, sizeof(__u16));
		if(!r_buf) {
			PASTLOG_WARN("cannot read -> log buffer reset\n");
			return false;
		}

		pay_len = (unsigned long)*((__u16*)r_buf);
		if(pay_len < 2 ||
		   pay_len > LOGGER_ENTRY_MAX_PAYLOAD) {
			PASTLOG_ERR("Payload size error : offset=%lu %u\n", *reader, pay_len);
		}

		r_buf = past_read(*reader, LOGGER_ENTRY_MAX_LEN);
		if(!r_buf) {
			PASTLOG_WARN("cannot read -> log buffer reset\n");
			return false;
		}

		if(!past_mmc_payload_check(r_buf, get_buf_size(r_buf))) {
			if(!past_mmc_search_payload(*reader + 1, reader)) {
				PASTLOG_WARN("cannot read -> log buffer reset\n");
				return false;
			}

			r_buf = past_read(*reader, sizeof(__u16));
			if(!r_buf) {
				PASTLOG_WARN("cannot read -> log buffer reset\n");
				return false;
			}
			pay_len = (unsigned long)*((__u16*)r_buf);
		}

		*reader += pay_len + sizeof(struct logger_entry);
		*reader %= past_size;
		LOCAL_DEBUG("next = %lu\n", *reader);
		if(new_pos < r_org) {
			bNext = (*reader > r_org || new_pos > *reader);
		} else {
			bNext = (*reader < new_pos && *reader >= r_org);
		}
	} while(bNext);

	LOCAL_DEBUG("attach reader : %lu\n", *reader);
	return	true;
}

bool past_mmc_initialize(void)
{
	if(!past_mmc_init) {
		int	ret;

		if(PAST_MMC_OPEN) {
			PASTLOG_ERR("Open error\n");
			return false;
		}

#if (PAST_SIZE_SMALL_DEBUG > (PAST_CACHE_BUFSIZE * 2))
		past_mmc_log_size = PAST_SIZE_SMALL_DEBUG + PAST_MMC_DATA_START;
#endif /* PAST_SIZE_SMALL_DEBUG > 512*/
		past_mmc_log_size = (past_mmc_log_size / 512) * 512;
		LOCAL_DEBUG("Log size = %lu\n", past_mmc_log_size);

#ifdef PAST_EVERYTIME_INIT
		ret = -2;
#else /* PAST_EVERYTIME_INIT */
		ret = past_mmc_header_read();
#endif /* PAST_EVERYTIME_INIT */

		if(ret) {
			memset(&past_mmc_header, 0, sizeof(PAST_HEADER));
			past_mmc_header.magic1 = PAST_MAGIC1;
			past_mmc_header.magic2 = PAST_MAGIC2;
			past_mmc_header.version = PAST_VERSION;
			past_mmc_header.offset = PAST_MMC_DATA_START;
			past_mmc_header.size   = past_mmc_log_size;

			LOCAL_DEBUG("header init write\n");
			if(past_mmc_header_write()) {
				PASTLOG_ERR("header write error 1\n");
				return false;
			}

			if(log_past)
				log_past->head = (size_t)past_mmc_header.reader;
		}

		PASTLOG_DEBUG("header : magic1=%08lx magic2=%08lx version=%08lx w_pos=%lu w_max=%lu reader=%lu offset=%lu size=%lu\n",
						past_mmc_header.magic1,
						past_mmc_header.magic2,
						past_mmc_header.version,
						past_mmc_header.w_pos,
						past_mmc_header.w_max,
						past_mmc_header.reader,
						past_mmc_header.offset,
						past_mmc_header.size);

		if(!past_acs_cache_r.buf) {
			past_acs_cache_r.pos = (unsigned long)-1;
			past_acs_cache_r.sec = (unsigned long)-1;
			past_acs_cache_r.buf = (char*)kmalloc(PAST_CACHE_BUFSIZE, GFP_KERNEL);
			if(!past_acs_cache_r.buf) {
				PASTLOG_ERR("out of memory : %u\n", PAST_CACHE_BUFSIZE);
				return false;
			}
			memset(past_acs_cache_r.buf, 0, PAST_CACHE_BUFSIZE);
		}
		if(!past_acs_cache_w.buf) {
			past_acs_cache_w.pos = (unsigned long)-1;
			past_acs_cache_w.sec = (unsigned long)-1;
			past_acs_cache_w.buf = (char*)kmalloc(PAST_CACHE_BUFSIZE, GFP_KERNEL);
			if(!past_acs_cache_w.buf) {
				PASTLOG_ERR("out of memory : %u\n", PAST_CACHE_BUFSIZE);
				return false;
			}
			memset(past_acs_cache_w.buf, 0, PAST_CACHE_BUFSIZE);
		}

		past_size = past_mmc_log_size - past_mmc_header.offset;

		if(!ret) {
			bool	bChange = false;
			if(!past_mmc_reader_check(&bChange)) {
				past_mmc_header.w_pos	=
				past_mmc_header.w_max	=
				past_mmc_header.reader	= 0;
				bChange = true;
			}
			if(bChange) {
				if(past_mmc_header_write()) {
					PASTLOG_ERR("header write error 3\n");
					return false;
				}
			}
		}

		if(log_past) {
			log_past->head  = (size_t)past_mmc_header.reader;
			log_past->w_off = (size_t)past_mmc_header.w_pos;
		}

		LOCAL_DEBUG("Past log start : w_pos=%lu w_max=%lu r_pos=%lu size=%lu\n",
						past_mmc_header.w_pos,
						past_mmc_header.w_max,
						past_mmc_header.reader,
						past_size);

#ifdef CONFIG_HAS_WAKELOCK /* E_BOOK */
		wake_lock_init(&past_wake_lock, WAKE_LOCK_SUSPEND, "past_wake_lock");
#endif /* E_BOOK */

		mutex_init(&past_access_mutex);
		past_mmc_init = true;
	}

	return true;
}

static inline int clock_interval(unsigned long a, unsigned long b, unsigned long c)
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

static void fix_up_readers(size_t len)
{
	unsigned long			old = past_mmc_header.w_pos;
	unsigned long			new = (((old + (unsigned long)len + (PAST_CACHE_BUFSIZE * 2) - 1) / 512) * 512) % past_size;
	unsigned long			new_reader = 0;
	struct logger_reader*	reader;

	new_reader = log_past? (unsigned long)log_past->head: past_mmc_header.reader;
	LOCAL_DEBUG("old=%lu new=%lu now=%lu\n", old, new, new_reader);
	if(clock_interval(old, new, new_reader)) {
		if(past_search_reader(new, &new_reader)) {
			LOCAL_DEBUG("new now=%lu\n", new_reader);
			past_mmc_header.reader = new_reader;
			if(log_past)
				log_past->head = (size_t)past_mmc_header.reader;
		} else {
			past_mmc_header.w_pos	=
			past_mmc_header.w_max	=
			past_mmc_header.reader	= 0;

			old = past_mmc_header.w_pos;
			new = (((old + (unsigned long)len + (PAST_CACHE_BUFSIZE * 2) - 1) / 512) * 512) % past_size;
			new_reader = 0;
		}
		past_mmc_header_write();
	}

	if(!log_past)
		return;

	list_for_each_entry(reader, &log_past->readers, list)
		LOCAL_DEBUG("old=%lu new=%lu now=%u\n", old, new, reader->r_off);
		if(past_mmc_header.w_pos == 0) {
			reader->r_off = 0;
		} else	if(past_mmc_header.w_max >= past_size) {
			if(new_reader < past_size &&
			   clock_interval(old, new, new_reader)) {
				past_search_reader(new, &new_reader);
				LOCAL_DEBUG("new now=%lu\n", new_reader);
				reader->r_off = (size_t)new_reader;
			}
		}
}

/*
 * Write in one log entry.
 *		buf		:	A buffer address
 *		size	:	Entry size
 */
size_t past_mmc_write(const char *buf, size_t size)
{
	size_t	ret = 0;
	char*	log_buf = (char*)buf;
	size_t	log_entry_size = size;
	size_t	pos = 0;
	size_t	last = (size_t)-1;

	if(!past_mmc_initialize())
		return	ret;

	mutex_lock(get_mutex());

#ifdef CONFIG_HAS_WAKELOCK /* E_BOOK */
	wake_lock(&past_wake_lock);
#endif /* E_BOOK */
	past_mmc_access = true;

	while(pos < log_entry_size) {
		if(past_mmc_payload_check(&log_buf[pos], log_entry_size - pos)) {
			last = pos;
			pos += (size_t)*((__u16*)&log_buf[pos]) + sizeof(struct logger_entry);
		} else {
			bool	bAttach = false;
			size_t	start = pos;

			LOCAL_DEBUG("pos = %x\n", pos);
			LOCAL_DEBUG("write data payload error : write skip\n");

			while(pos < (log_entry_size - sizeof(struct logger_entry))) {
				LOCAL_DEBUG("  %02x\n", *((const unsigned char*)&log_buf[pos]));
				pos++;
				if(past_mmc_payload_check(&log_buf[pos], log_entry_size - pos)) {
					bAttach = true;
					break;
				}
			}

			if(bAttach) {
				LOCAL_DEBUG("move %x <- %x (%x)\n", start, pos, log_entry_size - pos)
				memcpy(&log_buf[start], &log_buf[pos], log_entry_size - pos);
				log_entry_size -= (pos - start);
				pos = start;
			} else {
				log_entry_size = pos;
			}
		}
	}

	if(last == (size_t)-1) {
		LOCAL_DEBUG("no write\n");
		goto write_out;
	}
	log_entry_size = last + (size_t)*((__u16*)&log_buf[last]) + sizeof(struct logger_entry);
	LOCAL_DEBUG("after write size %x\n", log_entry_size);

	if(past_mmc_header.w_max < past_size) {
		unsigned long new_w = log_past? (unsigned long)log_past->w_off: past_mmc_header.w_pos;
		new_w += (unsigned long)log_entry_size;
		if(new_w > past_size) {
			past_mmc_header.w_max = past_size;
		} else {
			past_mmc_header.w_max = new_w;
		}
		copy_header_info();
	}
	fix_up_readers(log_entry_size);

	if(past_write(past_mmc_header.w_pos, (unsigned long)log_entry_size, log_buf)) {
		past_mmc_header.w_pos += (unsigned long)log_entry_size;
		past_mmc_header.w_pos %= past_size;
		if(past_mmc_header_write() == 0) {
			if(log_past)
				log_past->w_off = (size_t)past_mmc_header.w_pos;
			ret = log_entry_size;
		}
	}

write_out:
	past_mmc_access = false;
#ifdef CONFIG_HAS_WAKELOCK /* E_BOOK */
	wake_unlock(&past_wake_lock);
#endif /* E_BOOK */

	mutex_unlock(get_mutex());

	if(log_past)
		wake_up_interruptible(&log_past->wq);

	return	ret;
}

/*
 * Read one log entry.
 *		buf			:	A buffer to receive a log entry.
 *		buf_size	:	The size of the buffer.
 *		*r_off		:	The pointer to a variable to point a current offset.
 *						-1 points the first entry.
 */
size_t past_mmc_read(char __user *buf, size_t buf_size, size_t* r_off)
{
	size_t	ret   = 0;

	if(!past_mmc_initialize())
		return	ret;

	if(!log_past)
		mutex_lock(&past_access_mutex);
#ifdef CONFIG_HAS_WAKELOCK /* E_BOOK */
	wake_lock(&past_wake_lock);
#endif /* E_BOOK */
	past_mmc_access = true;

	while(true) {
		char*	r_buf = NULL;

		if(past_mmc_header.w_max >= past_size) {
			if(past_mmc_header.w_pos < past_mmc_header.reader) {
				if((unsigned long)*r_off >= past_mmc_header.w_pos &&
				   (unsigned long)*r_off <  past_mmc_header.reader) {
					break;
				}
			} else {
				if((unsigned long)*r_off >= past_mmc_header.w_pos ||
				   (unsigned long)*r_off <  past_mmc_header.reader) {
					break;
				}
			}
		} else	if((unsigned long)*r_off >= past_mmc_header.w_pos) {
			break;
		}

		r_buf = past_read(*r_off, LOGGER_ENTRY_MAX_LEN);
		if(!r_buf)
			break;

		if(past_mmc_payload_check(r_buf, get_buf_size(r_buf))) {
			size_t	ent_size = (size_t)*((__u16*)r_buf) + sizeof(struct logger_entry);
			if(buf_size < ent_size) {
				PASTLOG_ERR("Entry size is bigger than buffer size. offset=%u %u > %u\n", *r_off, ent_size, buf_size);
				break;
			}

			if(copy_to_user(buf, r_buf, ent_size))
				break;
			ret = ent_size;
			*r_off += ent_size;
			*r_off %= past_size;
			break;
		}

		*r_off = (*r_off + 1) % past_size;
	}

#ifdef CONFIG_HAS_WAKELOCK /* E_BOOK */
	wake_unlock(&past_wake_lock);
#endif /* E_BOOK */
	past_mmc_access = false;
	if(!log_past)
		mutex_unlock(&past_access_mutex);

	return	ret;
}

#endif /* E_BOOK */
