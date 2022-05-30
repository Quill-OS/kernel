#ifndef	__F_MASS_STORAGE_EXTSCSI_H__
#define	__F_MASS_STORAGE_EXTSCSI_H__




#include <linux/timer.h>
#include <linux/wait.h>
#include <linux/jiffies.h>




enum extscsi_stage
{
	EXT_CBW_EMPTY,
	EXT_CBW_FULL,
	EXT_CBW_GOT,
	EXT_DATA_R_EMPTY,
	EXT_DATA_R_FULL,
	EXT_DATA_R_GOT,
	EXT_DATA_W_EMPTY,
	EXT_DATA_W_FULL,
	EXT_ERROR,
};




#define EXT_SCSI_WAKEUP_TIME	(jiffies + msecs_to_jiffies(180000))



#define MAX_EXTENDED_DATA_SIZE		(4 * 1024)

struct	extscsi_t {
	int						connected;
	int						opened;
	int						error;
	int						timeout;
	enum	extscsi_stage	next_stage;
	enum	extscsi_stage	prev_stage;
	struct	timer_list		timeout_timer;
	wait_queue_head_t		poller;
	char					ext_buf[MAX_EXTENDED_DATA_SIZE];
	unsigned int			ext_buf_size;
	int						(*do_extscsi)(struct fsg_common*);
};




#endif	/* #ifndef __F_MASS_STORAGE_EXTSCSI_H__ */
