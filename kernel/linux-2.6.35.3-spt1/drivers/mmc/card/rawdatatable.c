#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/scatterlist.h>
#include <linux/delay.h>
#include <linux/mmc/core.h>
#include <linux/mmc/card.h>
#include <linux/mmc/host.h>
#include <linux/mmc/mmc.h>
#include <linux/mmc/rawdatatable.h>

//#define	RAWACS_DEBUG

#define	RAWTABLE_DEV		"mmcblk%d"
#define	RAWTABLE_ID			0x44524244
#define	RAWTABLE_SIG		0x72617774
#define	RAWTABLE_RAW		0
#define	RAWTABLE_PART		1
#define	RAWTABLE_SIZE		(128 * 1024)

typedef struct tagRAWTABLEHEADER {
	unsigned long	id;
	unsigned long	count;
	unsigned char	reserved1[24];
	unsigned char	reserved2[28];
	unsigned long	sig;
}	RAWTABLEHEADER;

typedef struct tagRAWDATAENTRY {
	unsigned long	type; // 0 : RAW / 1 : Partition
	unsigned long	start;
	unsigned long	size;
	unsigned char	reserved[20];
    char			label[32];
}	RAWDATAENTRY;

#ifdef RAWACS_DEBUG
static bool debug_flg = false;
#endif /* RAWACS_DEBUG */

static	int					mmc_dev_no = -1;
static	char				mmc_dev_name[16];
static	bool				rawdata_init	= false;
static	unsigned long		rawdata_addr	= 0;
static	char*				rawdata_table	= NULL;
static	char*				rawdata_param	= NULL;
static	char*				partition_param	= NULL;
static	struct mmc_card*	rawdata_card	= NULL;

module_param(mmc_dev_no, int, S_IRUGO);
module_param(rawdata_param, charp, S_IRUGO);
module_param(partition_param, charp, S_IRUGO);

static void prepare_mrq(struct mmc_card *card, struct mmc_request *mrq, struct scatterlist *sg, unsigned sg_len, unsigned dev_addr, unsigned blocks, unsigned blksz, int write)
{
	BUG_ON(!mrq || !mrq->cmd || !mrq->data || !mrq->stop);

	if (blocks > 1) {
		mrq->cmd->opcode = write ?
			MMC_WRITE_MULTIPLE_BLOCK : MMC_READ_MULTIPLE_BLOCK;
	} else {
		mrq->cmd->opcode = write ?
			MMC_WRITE_BLOCK : MMC_READ_SINGLE_BLOCK;
	}

	mrq->cmd->arg = dev_addr;
	if (!mmc_card_blockaddr(card))
		mrq->cmd->arg <<= 9;

	mrq->cmd->flags = MMC_RSP_R1 | MMC_CMD_ADTC;

	if (blocks == 1)
		mrq->stop = NULL;
	else {
		mrq->stop->opcode = MMC_STOP_TRANSMISSION;
		mrq->stop->arg = 0;
		mrq->stop->flags = MMC_RSP_R1B | MMC_CMD_AC;
	}

	mrq->data->blksz = blksz;
	mrq->data->blocks = blocks;
	mrq->data->flags = write ? MMC_DATA_WRITE : MMC_DATA_READ;
	mrq->data->sg = sg;
	mrq->data->sg_len = sg_len;

	mmc_set_data_timeout(mrq->data, card);
}

static int wait_busy(struct mmc_card *card)
{
	int ret, busy;
	struct mmc_command cmd;

	busy = 0;
	do {
		memset(&cmd, 0, sizeof(struct mmc_command));

		cmd.opcode = MMC_SEND_STATUS;
		cmd.arg = card->rca << 16;
		cmd.flags = MMC_RSP_R1 | MMC_CMD_AC;

		ret = mmc_wait_for_cmd(card->host, &cmd, 0);
		if (ret)
			break;

		if (!busy && !(cmd.resp[0] & R1_READY_FOR_DATA)) {
			busy = 1;
			mdelay(1);
//			printk(KERN_INFO "%s: Warning: Host did not "
//				"wait for busy state to end.\n",
//				mmc_hostname(card->host));
		}
	} while (!(cmd.resp[0] & R1_READY_FOR_DATA));

	return ret;
}

extern int mmc_resume_bus(struct mmc_host *host);
static int transfer(struct mmc_card *card, char *buffer, unsigned addr, unsigned num, int write)
{
	int ret;

	struct mmc_request mrq;
	struct mmc_command cmd;
	struct mmc_command stop;
	struct mmc_data data;

	struct scatterlist sg;
#if 1 /* E_BOOK *//* for stop sleeping 2011/06/10 */
	extern int dont_suspend_by_IO;
	int flag;
#endif

#if 1 /* E_BOOK *//* for stop sleeping 2011/06/10 */
	local_irq_save(flag);
	dont_suspend_by_IO++;
	local_irq_restore(flag);
#endif
#ifdef CONFIG_MMC_BLOCK_DEFERRED_RESUME
	mmc_resume_bus(card->host);
#endif

/* 2011/2/7 FY11 : Fixed the bug exclusion control does not work. */
	mmc_claim_host(card->host);

	memset(&mrq, 0, sizeof(struct mmc_request));
	memset(&cmd, 0, sizeof(struct mmc_command));
	memset(&data, 0, sizeof(struct mmc_data));
	memset(&stop, 0, sizeof(struct mmc_command));

	mrq.cmd = &cmd;
	mrq.data = &data;
	mrq.stop = &stop;

#if 1 /* E_BOOK */
	sg_init_one(&sg, buffer, 512*num);
#else
	sg_init_one(&sg, buffer, 512);
#endif

	prepare_mrq(card, &mrq, &sg, 1, addr, num, 512, write);

	mmc_wait_for_req(card->host, &mrq);

	if (cmd.error) {
		mmc_release_host(card->host);
#if 1 /* E_BOOK *//* for stop sleeping 2011/06/10 */
		local_irq_save(flag);
		dont_suspend_by_IO--;
		local_irq_restore(flag);
#endif
		return cmd.error;
	}
	if (data.error) {
		mmc_release_host(card->host);
#if 1 /* E_BOOK *//* for stop sleeping 2011/06/10 */
		local_irq_save(flag);
		dont_suspend_by_IO--;
		local_irq_restore(flag);
#endif
		return data.error;
	}

	ret = wait_busy(card);
	mmc_release_host(card->host);
#if 1 /* E_BOOK *//* for stop sleeping 2011/06/10 */
	local_irq_save(flag);
	dont_suspend_by_IO--;
	local_irq_restore(flag);
#endif
	if (ret)
		return ret;

	return 0;
}

static void cmdline_parse_rawdata(void)
{
	char*	p = NULL;
	char	buf[32];

	if(mmc_dev_no != -1 &&
	   rawdata_addr != 0)
		return;

	p = strstr(saved_command_line, "bootdev=");
	if(p) {
		int	i = 0;
		int	no = -1;
		p += 8;
		memset(buf, 0, 32);
		for(; i < 31 && *p && *p != 0x20; i++, p++)
			buf[i] = *p;

		if(sscanf(buf, "%d", &no) == 1 &&
		   (no == 0 || no ==2))
			mmc_dev_no = no;
	}

	p = strstr(saved_command_line, "rawtable=");
	if(p) {
		int	i = 0;
		unsigned long	addr = (unsigned long)-1;
		p += 9;
		if(strncmp(p, "0x", 2) == 0)
			p += 2;

		memset(buf, 0, 32);
		for(; i < 31 && *p && *p != 0x20; i++, p++)
			buf[i] = *p;

		if(sscanf(buf, "%lx", &addr) == 1 &&
		   addr >= 0x100000 &&
		   addr < 0x01800000 &&
		   (addr % 512) == 0)
			rawdata_addr = addr;
	}

	if(mmc_dev_no == -1 &&
	   rawdata_addr == 0) {
		p = strstr(saved_command_line, "nfsroot=");
		if(p) {
			mmc_dev_no = 2;
			rawdata_addr = 0x00F40000;
		}
	}
}

void rawdatatable_load(struct mmc_card *card, const char* devname)
{
	char	name[16];
	int		len;

	if(rawdata_table)
		return;

	cmdline_parse_rawdata();

	if(mmc_dev_no == -1 ||
	   rawdata_addr == 0)
		return;

	snprintf(name, sizeof(name), RAWTABLE_DEV, mmc_dev_no);
	len = strlen(name);

	if(strncmp(devname, name, len) == 0 &&
	   card->host &&
	   card->host->ops &&
	   card->host->ops->request) {
		RAWTABLEHEADER* header;
		RAWDATAENTRY*	entry;
		char			buf[128];
		char*			raw_p;
		char*			part_p;
		unsigned long	i;
		unsigned long	raw_size, part_size;
		unsigned long	raw_count, part_count;

		printk(KERN_INFO "Raw data table load\n");

		rawdata_table = (char*)kmalloc(RAWTABLE_SIZE, GFP_KERNEL);
		if(!rawdata_table) {
			printk(KERN_ERR "  out of memory(%dkB)\n", RAWTABLE_SIZE / 1024);
			return;
		}

		if(transfer(card, rawdata_table, rawdata_addr / 512, RAWTABLE_SIZE / 512, 0) != 0) {
			printk(KERN_ERR "  read error\n");
			kfree(rawdata_table);
			rawdata_table = NULL;
			return;
		}

		header = (RAWTABLEHEADER*)rawdata_table;
		if(header->id != RAWTABLE_ID ||
		   header->count >= ((RAWTABLE_SIZE - sizeof(RAWTABLEHEADER)) / sizeof(RAWDATAENTRY)) ||
		   header->sig != RAWTABLE_SIG) {
			printk(KERN_ERR "  header error\n");
			kfree(rawdata_table);
			rawdata_table = NULL;
			return;
		}

		raw_count  = 
		part_count = 0;
		entry      = (RAWDATAENTRY*)&rawdata_table[sizeof(RAWTABLEHEADER)];
		for(i = 0; i < header->count; i++, entry++) {
			if(entry->type == 0)
				raw_count++;
			else if(entry->type == 1)
				part_count++;
		}

		raw_size  = raw_count  * 64;
		part_size = part_count * 64;
		rawdata_param   = (char*)kmalloc(raw_size, GFP_KERNEL);
		if(!rawdata_param) {
			printk(KERN_ERR "  out of memory (%lu)\n", raw_size);
			kfree(rawdata_table);
			rawdata_table = NULL;
			return;
		}
		if(part_count > 0) {
			partition_param = (char*)kmalloc(part_size, GFP_KERNEL);
			if(!partition_param) {
				printk(KERN_ERR "  out of memory (%lu)\n", part_size);
				kfree(rawdata_param);
				kfree(rawdata_table);
				rawdata_param = NULL;
				rawdata_table = NULL;
				return;
			}
		}

		entry  = (RAWDATAENTRY*)&rawdata_table[sizeof(RAWTABLEHEADER)];
		raw_p  = rawdata_param;
		part_p = partition_param;
		for(i = 0; i < header->count; i++, entry++) {
			int				len;
			unsigned long	size;

			if(entry->type == 0) {
				snprintf(buf, sizeof(buf), "%-32s:0x%08lx:0x%08lx\n", entry->label, entry->start, entry->size);
				size = raw_size;
			} else if(entry->type == 1) {
				snprintf(buf, sizeof(buf), "%-32s:%lu:0x%08lx\n", entry->label, entry->start, entry->size);
				size = part_size;
			} else
				continue;

			len = strlen(buf);
			if(size < len + 1) {
				printk(KERN_ERR "  code error\n");
				kfree(partition_param);
				kfree(rawdata_param);
				kfree(rawdata_table);
				partition_param = NULL;
				rawdata_param   = NULL;
				rawdata_table   = NULL;
				return;
			}

			if(entry->type == 0) {
				strncpy(raw_p, buf, size);
				raw_p    += len;
				raw_size -= len;
			} else {
				strncpy(part_p, buf, size);
				part_p    += len;
				part_size -= len;
			}
		}

		rawdata_card = card;
		strncpy(mmc_dev_name, devname, sizeof(mmc_dev_name));
		rawdata_init = true;
	}

#ifdef RAWACS_DEBUG
	{
		char*	buffer = (char*)kmalloc(8192, GFP_KERNEL);
		if(!buffer) {
			printk("#### TEST : out of memory\n");
			return;
		}
		memset(buffer, 0, 8192);

		while(1) {
			unsigned long	next_start = 0;
			unsigned long	count, *p;
			unsigned long	size  = 0;
			unsigned long	index = rawdata_index(RAWLABEL_LOG, &size);
			if(index == (unsigned long)-1) {
				printk("#### TEST : open error\n");
				break;
			}

			debug_flg = true;

			printk("#### TEST : short... %08lx\n", next_start);
			p = (unsigned long*)buffer;
			*p = (unsigned long)buffer;
			for(count = 0; count < 512; count += sizeof(long)) {
				unsigned long offset	= next_start + count * 512 + count;
				unsigned long ret		= (unsigned long)rawdata_write(index, offset, buffer, sizeof(long));
				if(ret != sizeof(long)) {
					printk("  write error (%lu) : offset=%lx 4 : ret=%lu\n", count, offset, ret);
				}
			}
			p = (unsigned long*)&buffer[512];
			memset(p, 0, 512);
			for(count = 0; count < 512; count += sizeof(long)) {
				unsigned long offset	= next_start + count * 512 + count;
				unsigned long ret		= (unsigned long)rawdata_read(index, offset, (char*)p, sizeof(long));
				if(ret != sizeof(long)) {
					printk("  read error (%lu) : offset=%lx 4 : ret=%lu\n", count, offset, ret);
				} else if(*p != (unsigned long)buffer) {
					printk("  verify error (%lu) : offset=%lx 4 : %lx != %lx\n", count, offset, *p, (unsigned long)buffer);
				}
				memset(p, 0, 512);
			}
			next_start += (512 * 512 + 1024);

			printk("#### TEST : between sector 512byte... %08lx\n", next_start);
			memcpy(buffer, rawdata_table, 512);
			for(count = 0; count < 512; count++) {
				unsigned long offset	= next_start + count * 1024 + count;
				unsigned long ret		= (unsigned long)rawdata_write(index, offset, buffer, 512);
				if(ret != 512) {
					printk("  write error (%lu) : offset=%lx 512 : ret=%lu\n", count, offset, ret);
				}
			}
			p = (unsigned long*)&buffer[512];
			memset(p, 0, 512);
			for(count = 0; count < 512; count++) {
				unsigned long offset	= next_start + count * 1024 + count;
				unsigned long ret		= (unsigned long)rawdata_read(index, offset, (char*)p, 512);
				if(ret != 512) {
					printk("  read error (%lu) : offset=%lx 512 : ret=%lu\n", count, offset, ret);
				} else if(memcmp(p, rawdata_table, 512) != 0) {
					printk("  verify error (%lu) : offset=%lx 512\n", count, offset);
				}
				memset(p, 0, 512);
			}
			next_start += (512 * 1024 + 1024);

			printk("#### TEST : between sector 1024byte... %08lx\n", next_start);
			memcpy(buffer, rawdata_table, 1024);
			for(count = 0; count < 512; count++) {
				char	c = buffer[count];
				buffer[count] = buffer[1024 - count - 1];
				buffer[1024 - count - 1] = c;
			}
			memcpy(&buffer[1024], buffer, 1024);
			for(count = 0; count < 512; count++) {
				unsigned long offset	= next_start + count * 2048 + count;
				unsigned long ret;

				printk("Write (%lu) : offset=%lx 1024\n", count, offset);
				ret = (unsigned long)rawdata_write(index, offset, buffer, 1024);
				if(ret != 1024) {
					printk("  error : ret=%lu\n", ret);
				}

				ret = (unsigned long)rawdata_read(index, (offset / 512) * 512, &buffer[4096], 2048);
				if(memcmp(&buffer[4096 + (offset % 512)], &buffer[1024], 1024) != 0) {
					printk("  verify error\n");
				}
			}
			p = (unsigned long*)buffer;
			memset(p, 0, 1024);
			for(count = 0; count < 512; count++) {
				unsigned long offset	= next_start + count * 2048 + count;
				unsigned long ret		= (unsigned long)rawdata_read(index, offset, (char*)p, 1024);
				if(ret != 1024) {
					printk("  read error (%lu) : offset=%lx 1024 : ret=%lu\n", count, offset, ret);
				} else if(memcmp(p, &buffer[1024], 1024) != 0) {
					printk("  verify error (%lu) : offset=%lx 1024\n", count, offset);
				}
				memset(p, 0, 1024);
			}

			break;
		}
		debug_flg = false;
		kfree(buffer);
	}
#endif /* RAWACS_DEBUG */
}

bool is_rawinit(void)
{
	return rawdata_init;
}
EXPORT_SYMBOL(is_rawinit);

bool is_rawdatadev(const char* devname)
{
	return	(strncmp(mmc_dev_name, devname, strlen(devname)) == 0);
}
EXPORT_SYMBOL(is_rawdatadev);

unsigned long	rawdata_index(const char* label, unsigned long* size)
{
	unsigned long	index = -1;

	if(rawdata_table) {
		RAWTABLEHEADER* header = (RAWTABLEHEADER*)rawdata_table;
		RAWDATAENTRY*	entry  = (RAWDATAENTRY*)&rawdata_table[sizeof(RAWTABLEHEADER)];
		int				len    = strlen(label);
		unsigned long	i;

		for(i = 0; i < header->count; i++, entry++) {
			if(strncmp(label, entry->label, len) == 0) {
				index = i;
				*size = entry->size;
				break;
			}
		}
	}

	return	index;
}
EXPORT_SYMBOL(rawdata_index);

int	rawdata_read(unsigned long index, unsigned long offset, char* buffer, unsigned long size)
{
	RAWTABLEHEADER* header = (RAWTABLEHEADER*)rawdata_table;
	RAWDATAENTRY*	entry;
	unsigned 		off;
	unsigned 		num;
	unsigned		block;
	unsigned		max_block;
	int				ret = -1;
	char			buf[512];
	char*			p = buffer;

	if(!rawdata_table ||
	   index >= header->count) {
		return ret;
	}

	entry  = (RAWDATAENTRY*)&rawdata_table[sizeof(RAWTABLEHEADER) + sizeof(RAWDATAENTRY) * index];
	offset += entry->start;
	if(offset < entry->start ||
	   offset >= (entry->start + entry->size)) {
		return ret;
	}

	if((offset + size) > (entry->start + entry->size)) {
		size = (entry->start + entry->size) - offset;
	}

	if(offset % 512) {
		unsigned in = (unsigned)(offset % 512);
		unsigned re = (unsigned)(512 - in);
		if(re > size)
			re = size;

		off = (unsigned)(offset / 512);
		num = 1;
		if(transfer(rawdata_card, buf, off, num, 0) != 0) {
			return ret;
		}

		if(ret < 0)
			ret = 0;
		memcpy(p, &buf[in], re);
		p      += re;
		offset += (unsigned long)re;
		size   -= (unsigned long)re;
		ret    += (int)re;
		if(size <= 0)
			return ret;
	}

	max_block = rawdata_card->host->max_req_size / 512;
	if(max_block > rawdata_card->host->max_blk_count)
		max_block = rawdata_card->host->max_blk_count;

	num = (unsigned)(size / 512);
	for(block = 0; block < num; block += max_block) {
		unsigned	acs_block;
		acs_block = num - block;
		if(acs_block > max_block)
			acs_block = max_block;

		off = (unsigned)(offset / 512);
		if(transfer(rawdata_card, p, off, acs_block, 0) != 0) {
			return ret;
		}

		if(ret < 0)
			ret = 0;
		acs_block *= 512;
		p         += acs_block;
		offset    += (unsigned long)acs_block;
		size      -= (unsigned long)acs_block;
		ret       += (int)acs_block;
		if(size <= 0)
			return ret;
	}

	if(size > 0) {
		off = (unsigned)(offset / 512);
		num = 1;
		if(transfer(rawdata_card, buf, off, num, 0) != 0) {
			return ret;
		}

		memcpy(p, buf, size);

		if(ret < 0)
			ret = 0;
		ret += (int)size;
	}

	return ret;
}
EXPORT_SYMBOL(rawdata_read);

int	rawdata_write(unsigned long index, unsigned long offset, char* buffer, unsigned long size)
{
	RAWTABLEHEADER* header = (RAWTABLEHEADER*)rawdata_table;
	RAWDATAENTRY*	entry;
	unsigned 		off;
	unsigned 		num;
	unsigned 		block;
	unsigned		max_block;
	int				ret = -1;
	char			buf[512];
	char*			p = buffer;

	if(!rawdata_table ||
	   index >= header->count) {
		return ret;
	}

	entry  = (RAWDATAENTRY*)&rawdata_table[sizeof(RAWTABLEHEADER) + sizeof(RAWDATAENTRY) * index];
	offset += entry->start;
	if(offset < entry->start ||
	   offset >= (entry->start + entry->size)) {
		return ret;
	}

	if((offset + size) > (entry->start + entry->size)) {
		size = (entry->start + entry->size) - offset;
	}

	if(offset % 512) {
		unsigned in = (unsigned)(offset % 512);
		unsigned re = (unsigned)(512 - in);
		if(re > size)
			re = size;

		off = (unsigned)(offset / 512);
		num = 1;
#ifdef	RAWACS_DEBUG
		if(debug_flg) {
			printk("    %s #1 : offset=%x pos=%u size=%u p=%p\n", __FUNCTION__, off, in, re, p);
		}
#endif /* RAWACS_DEBUG */
		if(transfer(rawdata_card, buf, off, num, 0) != 0) {
			return ret;
		}

		memcpy(&buf[in], p, re);
		if(transfer(rawdata_card, buf, off, num, 1) != 0) {
			return ret;
		}

		if(ret < 0)
			ret = 0;
		p      += re;
		offset += (unsigned long)re;
		size   -= (unsigned long)re;
		ret    += (int)re;
		if(size <= 0)
			return ret;
	}

	max_block = rawdata_card->host->max_req_size / 512;
	if(max_block > rawdata_card->host->max_blk_count)
		max_block = rawdata_card->host->max_blk_count;

	num = (unsigned)(size / 512);
	for(block = 0; block < num; block += max_block) {
		unsigned	acs_block;
		acs_block = num - block;
		if(acs_block > max_block)
			acs_block = max_block;

		off = (unsigned)(offset / 512);
#ifdef	RAWACS_DEBUG
		if(debug_flg) {
			printk("    %s #2 : offset=%x size=%u p=%p\n", __FUNCTION__, off, acs_block * 512, p);
		}
#endif /* RAWACS_DEBUG */
		if(transfer(rawdata_card, p, off, acs_block, 1) != 0) {
			return ret;
		}

		if(ret < 0)
			ret = 0;
		acs_block *= 512;
		p         += acs_block;
		offset    += (unsigned long)acs_block;
		size      -= (unsigned long)acs_block;
		ret       += (int)acs_block;
		if(size <= 0)
			return ret;
	}

	if(size > 0) {
		off = (unsigned)(offset / 512);
		num = 1;
#ifdef	RAWACS_DEBUG
		if(debug_flg) {
			printk("    %s #3 : offset=%x size=%u p=%p\n", __FUNCTION__, off, size, p);
		}
#endif /* RAWACS_DEBUG */
		if(transfer(rawdata_card, buf, off, num, 0) != 0) {
			return ret;
		}

		memcpy(buf, p, size);

		if(transfer(rawdata_card, buf, off, num, 1) != 0) {
			return ret;
		}

		if(ret < 0)
			ret = 0;
		ret += (int)size;
	}

	return ret;
}

