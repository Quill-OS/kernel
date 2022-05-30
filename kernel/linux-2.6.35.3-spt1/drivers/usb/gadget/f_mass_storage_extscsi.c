#include "f_mass_storage_extscsi.h"




#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/poll.h>
#include <linux/miscdevice.h>




#define	SC_SONY_EXTENDED	(0x20)




#define DEFAULT_READ_TIMEOUT		(3)			/* sec */
#define READ_TIMEOUT_COUNT(x)		(x * 10)




#define EL_CBW 				(0x1 << 0x01)
#define EL_CBW_D2A	 		(0x1 << 0x02)
#define EL_DATA_A2D			(0x1 << 0x03)
#define EL_DATA_D2A			(0x1 << 0x04)
#define EL_CSW				(0x1 << 0x05)

#define EL_CBW_E			(0x1 << 0x11)
#define EL_DATA_A2D_E		(0x1 << 0x12)
#define EL_DATA_D2A_E		(0x1 << 0x13)

typedef struct tag_ebook_extscsi_que {
	unsigned long	cbw_time;
	unsigned long	data_time;
	unsigned long	csw_time;
	unsigned int	status;
	char			cbw[31];
	char			data[8];
	unsigned int	data_size;
	char			csw[13];
} ebook_extscsi_que;


#define MAX_QUEUE_EXTSCSI		(10)
static	ebook_extscsi_que	queue_ebook_tbl[MAX_QUEUE_EXTSCSI];
static	unsigned int		queue_ebook_counter = 0;
static	unsigned int		queue_ebook_carry = 0;


static	void
ebook_extscsi_queue_log(char *buf, unsigned int size, int flag)
{
	switch(flag) {
	case EL_CBW:
		queue_ebook_tbl[queue_ebook_counter].status = EL_CBW;
		queue_ebook_tbl[queue_ebook_counter].cbw_time = msecs_to_jiffies(jiffies);
		memcpy(queue_ebook_tbl[queue_ebook_counter].cbw, buf, size);
		break;

	case EL_DATA_D2A:
	case EL_DATA_A2D:
		queue_ebook_tbl[queue_ebook_counter].status |= flag;
		queue_ebook_tbl[queue_ebook_counter].data_time = msecs_to_jiffies(jiffies);
		queue_ebook_tbl[queue_ebook_counter].data_size = size;
		memcpy(queue_ebook_tbl[queue_ebook_counter].data, buf, 8);
		break;

	case EL_CSW:
		queue_ebook_tbl[queue_ebook_counter].status |= EL_CSW;
		queue_ebook_tbl[queue_ebook_counter].csw_time = msecs_to_jiffies(jiffies);
		memcpy(queue_ebook_tbl[queue_ebook_counter].csw, buf, size);
		queue_ebook_counter++;
		break;

	case EL_CBW_D2A:
	case EL_CBW_E:
	case EL_DATA_A2D_E:
	case EL_DATA_D2A_E:
		queue_ebook_tbl[queue_ebook_counter].status |= flag;
		break;
	}

	if (queue_ebook_counter > MAX_QUEUE_EXTSCSI - 1) {
		queue_ebook_counter = 0;
		queue_ebook_carry = 1;
	}
}



static	int
do_extended_h2d(struct fsg_common* common)
{
	struct	extscsi_t*		extscsi = common->private_data;
	struct	fsg_lun*		curlun = common->curlun;
	struct	fsg_buffhd*		bh;
	int						get_some_more;
	u32						amount_left_to_req, amount_left_to_write;
	loff_t					usb_offset = 0, file_offset = 0;
	u32						amount;
	int						rc;

	if (!extscsi->ext_buf) {
		curlun->sense_data = SS_COMMUNICATION_FAILURE;
		return -EINVAL;
	}

	memset(extscsi->ext_buf, 0x0, MAX_EXTENDED_DATA_SIZE);

	/* Carry out the file writes */
	get_some_more = 1;
	amount_left_to_req = amount_left_to_write = common->data_size;

	while (amount_left_to_write > 0) {

		/* Queue a request for more data from the host */
		bh = common->next_buffhd_to_fill;
		if (bh->state == BUF_STATE_EMPTY && get_some_more) {

			amount = min(amount_left_to_req, (u32)MAX_EXTENDED_DATA_SIZE);

			/* Get the next buffer */
			usb_offset += amount;
			amount_left_to_req -= amount;
			if (amount_left_to_req == 0)
				get_some_more = 0;

			/* amount is always divisible by 512, hence by
			 * the bulk-out maxpacket size */
			bh->outreq->length = bh->bulk_out_intended_length = amount;
			START_TRANSFER_OR(common, bulk_out, bh->outreq, &bh->outreq_busy, &bh->state)
				return -EIO;
			common->next_buffhd_to_fill = bh->next;
			continue;
		}

		/* Write the received data to the backing file */
		bh = common->next_buffhd_to_drain;
		if (bh->state == BUF_STATE_EMPTY && !get_some_more)
			break;			// We stopped early

		if (bh->state == BUF_STATE_FULL) {
			common->next_buffhd_to_drain = bh->next;
			bh->state = BUF_STATE_EMPTY;

			/* Did something go wrong with the transfer? */
			if (bh->outreq->status != 0) {
				curlun->sense_data = SS_COMMUNICATION_FAILURE;
				break;
			}

			amount = bh->outreq->actual;

			memcpy((char *)(extscsi->ext_buf + file_offset), (char *)bh->buf, amount);

			if (signal_pending(current))
				return -EINTR;		// Interrupted!

			file_offset += amount;
			amount_left_to_write -= amount;
			common->residue -= amount;

			/* Did the host decide to stop early? */
			if (bh->outreq->actual != bh->outreq->length) {
				common->short_packet_received = 1;
				break;
			}
			continue;
		}

		/* Wait for something to happen */
		if ((rc = sleep_thread(common)) != 0)
			return rc;
	}

	ebook_extscsi_queue_log(extscsi->ext_buf, file_offset, EL_DATA_D2A);
	extscsi->next_stage = EXT_DATA_R_FULL;
	wake_up(&extscsi->poller);

	extscsi->error = 0;

	while (extscsi->next_stage != EXT_DATA_R_GOT) {
		mod_timer(&extscsi->timeout_timer, EXT_SCSI_WAKEUP_TIME);
		if ((rc = sleep_thread(common)) != 0) {
			//phase_error = 1;
			curlun->sense_data = SS_COMMUNICATION_FAILURE;
			usb_offset = -EINVAL;
			ebook_extscsi_queue_log(NULL, 0, EL_DATA_D2A_E);
			break;
		} else if (extscsi->error == 0) {
			del_timer(&extscsi->timeout_timer);
		} else  {
			curlun->sense_data = SS_COMMUNICATION_FAILURE;
			usb_offset = -EINVAL;
			ebook_extscsi_queue_log(NULL, 0, EL_DATA_D2A_E);
			break;
		}
	}

	return usb_offset;
}



static	int
do_extended_d2h(struct fsg_common* common)
{
	struct	extscsi_t*		extscsi = common->private_data;
	struct	fsg_lun*		curlun = common->curlun;
	struct	fsg_buffhd*		bh;
	int						rc;
	u32						amount_left;
	loff_t					file_offset = 0;
	unsigned int			amount;

	extscsi->error = 0;

	while (extscsi->next_stage != EXT_DATA_W_FULL) {
		mod_timer(&extscsi->timeout_timer, EXT_SCSI_WAKEUP_TIME);
		if ((rc = sleep_thread(common)) != 0) {
			ebook_extscsi_queue_log(NULL, 0, EL_DATA_A2D_E);
			curlun->sense_data = SS_COMMUNICATION_FAILURE;
			return -EINVAL;
		} else if (extscsi->error == 0) {
			del_timer(&extscsi->timeout_timer);
		} else  {
			ebook_extscsi_queue_log(NULL, 0, EL_DATA_A2D_E);
			curlun->sense_data = SS_COMMUNICATION_FAILURE;
			return -EINVAL;
		}
	}

	/* Carry out the file reads */
	amount_left = extscsi->ext_buf_size;
	ebook_extscsi_queue_log(extscsi->ext_buf, amount_left, EL_DATA_A2D);
	if (unlikely(amount_left == 0))
		return -EIO;		// No default reply

	for (;;) {

		amount = min((unsigned int) amount_left, (u32)MAX_EXTENDED_DATA_SIZE);

		/* Wait for the next buffer to become available */
		bh = common->next_buffhd_to_fill;
		while (bh->state != BUF_STATE_EMPTY) {
			if ((rc = sleep_thread(common)) != 0)
				return rc;
		}

		memcpy(bh->buf, (char *)(extscsi->ext_buf + file_offset), amount);

		if (signal_pending(current))
			return -EINTR;

		file_offset  += amount;
		amount_left  -= amount;
		bh->inreq->length = amount;
		bh->state = BUF_STATE_FULL;

		if (amount_left == 0)
			break;		// No more left to read

		/* Send this buffer and go read some more */
		bh->inreq->zero = 0;
		START_TRANSFER_OR(common, bulk_in, bh->inreq, &bh->inreq_busy, &bh->state)
			return -EIO;
		common->next_buffhd_to_fill = bh->next;

	}
	return file_offset;		// No default reply
}



static	int
do_extended(struct fsg_common* common)
{
	struct	extscsi_t*	extscsi = common->private_data;

	extscsi->error = 0;
	extscsi->next_stage = EXT_CBW_FULL;

	switch(common->data_dir) {
	case DATA_DIR_TO_HOST: /* write */
		mod_timer(&extscsi->timeout_timer, EXT_SCSI_WAKEUP_TIME);
		wake_up(&extscsi->poller);
		if ((sleep_thread(common) != 0) || (extscsi->error == 1)) {
			common->curlun->sense_data = SS_COMMUNICATION_FAILURE;
			ebook_extscsi_queue_log(NULL, 0, EL_CBW_E);
			return -EINVAL;
		} 
		del_timer(&extscsi->timeout_timer);
		ebook_extscsi_queue_log(NULL, 0, EL_CBW_D2A);
		return do_extended_d2h(common); 

	case DATA_DIR_FROM_HOST: /* read */
	case DATA_DIR_NONE:
		while (extscsi->next_stage != EXT_CBW_GOT) {
			mod_timer(&extscsi->timeout_timer, EXT_SCSI_WAKEUP_TIME);
			wake_up(&extscsi->poller);
			if (sleep_thread(common) != 0) {
				common->curlun->sense_data = SS_COMMUNICATION_FAILURE;
				ebook_extscsi_queue_log(NULL, 0, EL_DATA_D2A_E);
				return -EINVAL;
			} else if (extscsi->error == 0) {
				del_timer(&extscsi->timeout_timer);
			} else  {
				common->curlun->sense_data = SS_COMMUNICATION_FAILURE;
				ebook_extscsi_queue_log(NULL, 0, EL_DATA_D2A_E);
				return -EINVAL;
			}
		}
		ebook_extscsi_queue_log(NULL, 0, EL_CBW_D2A);
		if (common->data_size) {
			extscsi->next_stage = EXT_DATA_R_EMPTY;
			return do_extended_h2d(common);
		} else {
			return 0;
		}
		break;

	default:
		common->phase_error = 1;
		break;
	}

	return -EINVAL;
}



static	void
extscsi_timer(unsigned long _dev)
{
	struct	fsg_dev*	fsg = (void*)_dev;
	struct	extscsi_t*	extscsi = fsg->common->private_data;

	spin_lock(&fsg->common->lock);
	extscsi->error = 1;
	spin_unlock(&fsg->common->lock);
	wakeup_thread(fsg->common);
}




static	unsigned int
extscsi_data_poll(struct file* file, poll_table* wait)
{
	struct fsg_dev*		fsg = file->private_data;
	struct	extscsi_t*	extscsi = fsg->common->private_data;
	int					mask = 0;

	poll_wait(file, &extscsi->poller, wait);

	if ((extscsi->next_stage == EXT_CBW_FULL) || (extscsi->next_stage == EXT_DATA_R_FULL)) {
		if (extscsi->next_stage != extscsi->prev_stage) {
			mask = POLLIN | POLLRDNORM;
		}

		extscsi->prev_stage = extscsi->next_stage;
	}

	return mask;
}



static	ssize_t
extscsi_data_read(struct file* file, char* buf, size_t count, loff_t* ppos)
{
	struct fsg_dev*		fsg = file->private_data;
	struct	extscsi_t*	extscsi = fsg->common->private_data;
	ssize_t 			read = 0;
	ssize_t 			r_size = 0;
	int					sleep_time;

	if ((!fsg) || (!extscsi->ext_buf) || (!extscsi->connected)) {
		return -ENODEV;
	}
	sleep_time = extscsi->timeout;

	while ((extscsi->next_stage != EXT_CBW_FULL) && 
			(extscsi->next_stage != EXT_DATA_R_FULL)) {
		if (!sleep_time)
			return -ETIMEDOUT;

		set_current_state(TASK_INTERRUPTIBLE);
		if (schedule_timeout(HZ / 10) != 0) {
			set_current_state(TASK_RUNNING);
			return -ERESTARTSYS;
		}
		set_current_state(TASK_RUNNING);
		sleep_time--;
	}


	if (extscsi->next_stage == EXT_CBW_FULL) {
		r_size = 31;
	} else if (extscsi->next_stage == EXT_DATA_R_FULL) {
		r_size = fsg->common->data_size;
	}

	if (copy_to_user(buf, extscsi->ext_buf, r_size)) {
		return -EFAULT;
	}

	read += r_size;
	count -= r_size;

	spin_lock(&fsg->common->lock);
	if (extscsi->next_stage == EXT_CBW_FULL) {
		extscsi->prev_stage = extscsi->next_stage = EXT_CBW_GOT;
	} else if (extscsi->next_stage == EXT_DATA_R_FULL) {
		extscsi->prev_stage = extscsi->next_stage = EXT_DATA_R_GOT;
	}
	spin_unlock(&fsg->common->lock);
	wakeup_thread(fsg->common);

	return read ? read : -EIO;
}



static	ssize_t
extscsi_data_write(struct file* file, const char* buf, size_t count, loff_t* ppos)
{
	struct fsg_dev*		fsg = file->private_data;
	struct	extscsi_t*	extscsi = fsg->common->private_data;
	ssize_t				written = 0;

	if (*ppos > 0) {
		return 0;
	}

	if ((!fsg) || (!extscsi->ext_buf) || (!extscsi->connected)) {
		return -ENODEV;
	}

	if (count > MAX_EXTENDED_DATA_SIZE) {
		return -ENOMEM;
	}

	if (count > fsg->common->data_size)
		count = fsg->common->data_size;


	if (copy_from_user(extscsi->ext_buf, buf, count)) {
		return -EFAULT;
	}

	written += count;
	extscsi->ext_buf_size = count;

	spin_lock(&fsg->common->lock);
	extscsi->prev_stage = extscsi->next_stage = EXT_DATA_W_FULL;
	spin_unlock(&fsg->common->lock);

	wakeup_thread(fsg->common);

	return written;
}



static	int
extscsi_data_release(struct inode* inode, struct file* file)
{
	struct	fsg_dev*	fsg = file->private_data;
	struct	extscsi_t*	extscsi = fsg->common->private_data;

	if (fsg == NULL) {
		return -ENODEV;
	}

	extscsi->opened--;

	if (extscsi->opened <= 0) {
		extscsi->opened = 0;
	}

	return 0;
}



struct	fsg_dev*	the_fsg = NULL;
static	int
extscsi_data_open(struct inode* inode, struct file* file)
{
	struct	fsg_dev*	fsg = the_fsg;
	struct	extscsi_t*	extscsi = fsg->common->private_data;

	if ((fsg == NULL) || (!extscsi->ext_buf)) {
		return -ENODEV;
	}

	if (extscsi->opened) {		/* can't multiple open */
		return -EBUSY;
	}

	extscsi->opened++;
	file->private_data = fsg;

	return 0;
}



static	struct	file_operations		extscsi_fops = {
	.owner		= THIS_MODULE,
	.poll		= extscsi_data_poll,
	.read		= extscsi_data_read,
	.write		= extscsi_data_write,
	.release	= extscsi_data_release,
	.open		= extscsi_data_open,
};

static	struct	miscdevice	extscsi_misc = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= "extscsi",
	.fops	= &extscsi_fops,
};




int
extscsi_initialize(struct fsg_dev* fsg)
{
	struct extscsi_t*	extscsi;
	int					result;

	extscsi = kmalloc(sizeof(struct extscsi_t), GFP_KERNEL);
	if (extscsi == NULL) {
		return -ENOMEM;
	}

	extscsi->do_extscsi	= do_extended;
	extscsi->connected	= 0;
	extscsi->opened		= 0;
	extscsi->prev_stage	= extscsi->next_stage = EXT_CBW_EMPTY;
	extscsi->timeout	= READ_TIMEOUT_COUNT(DEFAULT_READ_TIMEOUT);

	init_waitqueue_head(&extscsi->poller);

	memset(extscsi->ext_buf, 0x0, MAX_EXTENDED_DATA_SIZE);

	init_timer(&extscsi->timeout_timer);

	extscsi->timeout_timer.function = extscsi_timer;
	extscsi->timeout_timer.data = (unsigned long)fsg;
	fsg->common->private_data = extscsi;
	the_fsg = fsg;

	result = misc_register(&extscsi_misc);
	if (result < 0) { 
		printk("%s: misc_register: %d\n", extscsi_misc.name, result);
		goto Error_extscsi_initialize;
	}

	return 0;


Error_extscsi_initialize:
	if (extscsi != NULL) {
		kfree(extscsi);
	}

	return result;
}



void
extscsi_finalize(struct fsg_dev* fsg)
{
	struct	extscsi_t*	extscsi = fsg->common->private_data;

	if (extscsi != NULL) {
		del_timer_sync(&extscsi->timeout_timer);
		kfree(extscsi);
		fsg->common->private_data = NULL;
	}

	misc_deregister(&extscsi_misc);
}




/* end of f_mass_storage_extscsi.c */
