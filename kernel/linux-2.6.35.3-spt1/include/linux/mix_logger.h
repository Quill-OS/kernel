#ifndef	__MIX_LOGGER_H__
#define	__MIX_LOGGER_H__

#ifdef CONFIG_MIX_LOGGER /* E_BOOK */
#define	MIX_LOG_RESERVE_SIZE	(2*1024*1024)	/* mix logger area size. */

#ifdef CONFIG_MIX_LOGGER_STATIC
void	mix_log_set_mem_addr(resource_size_t addr, struct resource* res);
#endif /* CONFIG_MIX_LOGGER_STATIC */

void	mix_log_mem_init(void);
#endif /* CONFIG_MIX_LOGGER */

#endif	/* __MIX_LOGGER_H__ */
