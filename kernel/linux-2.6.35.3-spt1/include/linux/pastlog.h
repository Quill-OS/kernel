#ifndef	__PASTLOG_H__
#define	__PASTLOG_H__

#define PASTLOG_DEVNAME	"pastlog"			/* device name */

#define PASTLOGFLUSH	0x1					/* ioctl Log Flush */
#define	PASTLOGWRITE	0x2					/* write Log */
#define	PASTLOGH1		0x3
#define	PASTLOGH2		0x4
#define	PASTLOGH3		0x5
//#define	PASTLOGTESTDIE	0x10

typedef	unsigned long	PASTLOGH_DATA;


#ifdef CONFIG_PAST_LOG /* E_BOOK */

int pastlog_degug(const char *fmt, ...);

#ifdef CONFIG_MIX_LOGGER /* E_BOOK */
size_t mix2past_read_entry(char* buffer, size_t size);
#endif /* CONFIG_MIX_LOGGER */

int past_delay_write(void);

/* mmc_past */
void past_logger_init(void);
bool past_mmc_initialize(void);
bool past_can_read(void);
int past_can_write(void);
size_t get_pastlog_size(void);
void set_past_logger(void* log);
size_t get_past_entry_size(size_t offset);
int past_suspend(const char* devname);
int past_resume(const char* devname);
void past_flush(void);
void past_wait_flush(void);

/*
 * Write in one log entry.
 *		log_buf			:	A buffer address
 *		log_entry_size	:	Entry size
 */
size_t past_mmc_write(const char *log_buf, size_t log_entry_size);

/*
 * Read one log entry.
 *		buf			:	A buffer to receive a log entry.
 *		buf_size	:	The size of the buffer.
 *		*r_off		:	The pointer to a variable to point a current offset.
 *						-1 points the first entry.
 */
size_t past_mmc_read(char __user *buf, size_t buf_size, size_t* r_off);

#endif /* CONFIG_PAST_LOG */

#define	PASTLOG_WARN(fmt, arg...)	pastlog_degug(KERN_WARNING "[PASTLOG Info:%s(%d)]" fmt, __FUNCTION__, __LINE__, ##arg);
#define	PASTLOG_ERR(fmt, arg...)	pastlog_degug(KERN_ERR "[PASTLOG Err:%s(%d)]" fmt, __FUNCTION__, __LINE__, ##arg);

#ifdef CONFIG_PAST_LOG_DEBUG
#define	PASTLOG_DEBUG(fmt, arg...)	pastlog_degug(KERN_ERR "[PASTLOG:%s(%d)]" fmt, __FUNCTION__, __LINE__, ##arg);
#define PASTLOG_DEBUG2(fmt, arg...)	pastlog_degug(fmt, ##arg);
#else /* CONFIG_PAST_LOG_DEBUG */
#define	PASTLOG_DEBUG(fmt, arg...)
#define	PASTLOG_DEBUG2(fmt, arg...)
#endif /* CONFIG_PAST_LOG_DEBUG */

#endif	/* __PASTLOG_H__ */
