#ifndef	__KERNEL_LOGGER_H__
#define	__KERNEL_LOGGER_H__

#ifdef	CONFIG_KERNEL_LOGGER /* E_BOOK */
void	kernel_log_mem_init(void);
void	printk2logger(int level, const char* klog);
void	pastdev2logger(const char* klog);
void	klog_flush(void);
#endif /* E_BOOK */

#endif /* __KERNEL_LOGGER_H__ */
