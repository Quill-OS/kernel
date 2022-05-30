/*
 *  linux/fs/fat/file.c
 *
 *  Written 1992,1993 by Werner Almesberger
 *
 *  regular file handling primitives for fat-based filesystems
 */

#include <linux/capability.h>
#include <linux/module.h>
#include <linux/compat.h>
#include <linux/mount.h>
#include <linux/time.h>
#include <linux/buffer_head.h>
#include <linux/writeback.h>
#include <linux/backing-dev.h>
#include <linux/blkdev.h>
#include <linux/fsnotify.h>
#include <linux/security.h>
#include "fat.h"

static int fat_ioctl_get_attributes(struct inode *inode, u32 __user *user_attr)
{
	u32 attr;

	mutex_lock(&inode->i_mutex);
	attr = fat_make_attrs(inode);
	mutex_unlock(&inode->i_mutex);

	return put_user(attr, user_attr);
}

static int fat_ioctl_set_attributes(struct file *file, u32 __user *user_attr)
{
	struct inode *inode = file->f_path.dentry->d_inode;
	struct msdos_sb_info *sbi = MSDOS_SB(inode->i_sb);
	int is_dir = S_ISDIR(inode->i_mode);
	u32 attr, oldattr;
	struct iattr ia;
	int err;

	err = get_user(attr, user_attr);
	if (err)
		goto out;

	mutex_lock(&inode->i_mutex);
	err = mnt_want_write(file->f_path.mnt);
	if (err)
		goto out_unlock_inode;

	/*
	 * ATTR_VOLUME and ATTR_DIR cannot be changed; this also
	 * prevents the user from turning us into a VFAT
	 * longname entry.  Also, we obviously can't set
	 * any of the NTFS attributes in the high 24 bits.
	 */
	attr &= 0xff & ~(ATTR_VOLUME | ATTR_DIR);
	/* Merge in ATTR_VOLUME and ATTR_DIR */
	attr |= (MSDOS_I(inode)->i_attrs & ATTR_VOLUME) |
		(is_dir ? ATTR_DIR : 0);
	oldattr = fat_make_attrs(inode);

	/* Equivalent to a chmod() */
	ia.ia_valid = ATTR_MODE | ATTR_CTIME;
	ia.ia_ctime = current_fs_time(inode->i_sb);
	if (is_dir)
		ia.ia_mode = fat_make_mode(sbi, attr, S_IRWXUGO);
	else {
		ia.ia_mode = fat_make_mode(sbi, attr,
			S_IRUGO | S_IWUGO | (inode->i_mode & S_IXUGO));
	}

	/* The root directory has no attributes */
	if (inode->i_ino == MSDOS_ROOT_INO && attr != ATTR_DIR) {
		err = -EINVAL;
		goto out_drop_write;
	}

	if (sbi->options.sys_immutable &&
	    ((attr | oldattr) & ATTR_SYS) &&
	    !capable(CAP_LINUX_IMMUTABLE)) {
		err = -EPERM;
		goto out_drop_write;
	}

	/*
	 * The security check is questionable...  We single
	 * out the RO attribute for checking by the security
	 * module, just because it maps to a file mode.
	 */
	err = security_inode_setattr(file->f_path.dentry, &ia);
	if (err)
		goto out_drop_write;

	/* This MUST be done before doing anything irreversible... */
	err = fat_setattr(file->f_path.dentry, &ia);
	if (err)
		goto out_drop_write;

	fsnotify_change(file->f_path.dentry, ia.ia_valid);
	if (sbi->options.sys_immutable) {
		if (attr & ATTR_SYS)
			inode->i_flags |= S_IMMUTABLE;
		else
			inode->i_flags &= S_IMMUTABLE;
	}

	fat_save_attrs(inode, attr);
	mark_inode_dirty(inode);
out_drop_write:
	mnt_drop_write(file->f_path.mnt);
out_unlock_inode:
	mutex_unlock(&inode->i_mutex);
out:
	return err;
}

long fat_generic_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct inode *inode = filp->f_path.dentry->d_inode;
	u32 __user *user_attr = (u32 __user *)arg;

	switch (cmd) {
#ifdef CONFIG_SNSC_FS_FAT_BATCH_SYNC /* E_BOOK */
	case FAT_IOCTL_CLOSE_NOSYNC:
	{
		filp->f_op = &fat_file_operations_nosync;
		return 0;
	}
#endif /* CONFIG_SNSC_FS_FAT_BATCH_SYNC */
	case FAT_IOCTL_GET_ATTRIBUTES:
		return fat_ioctl_get_attributes(inode, user_attr);
	case FAT_IOCTL_SET_ATTRIBUTES:
		return fat_ioctl_set_attributes(filp, user_attr);
	default:
		return -ENOTTY;	/* Inappropriate ioctl for device */
	}
}

#ifdef CONFIG_COMPAT
static long fat_generic_compat_ioctl(struct file *filp, unsigned int cmd,
				      unsigned long arg)

{
	return fat_generic_ioctl(filp, cmd, (unsigned long)compat_ptr(arg));
}
#endif

#ifdef CONFIG_SNSC_FS_FAT_BATCH_SYNC /* E_BOOK */
/*
 * With the original SS FAT patch, this function is added to fs/fs-writeback.c.
 * However, a build error occurs because ubifs has an equal function.
 * In the first place it adds it here to use this function because there is
 * only this source.
 */
/*
 * earse DIRTY bit of inode
 * and move it from dirty list to inode_inuse
 */
static void mark_inode_clean(struct inode *inode)
{
	if (!(inode->i_state & I_DIRTY))
		return;

	if (unlikely(block_dump)) {
		struct dentry *dentry = NULL;
		const char *name = "?";

		if (!list_empty(&inode->i_dentry)) {
			dentry = list_entry(inode->i_dentry.next,
					    struct dentry, d_alias);
			if (dentry && dentry->d_name.name)
				name = (const char *) dentry->d_name.name;
		}

		if (inode->i_ino || strcmp(inode->i_sb->s_id, "bdev"))
			printk(KERN_DEBUG
			       "%s(%d): clean inode %lu (%s) on %s\n",
			       current->comm, current->pid, inode->i_ino,
			       name, inode->i_sb->s_id);
	}

	spin_lock(&inode_lock);

	if (inode->i_state & I_DIRTY) {
		inode->i_state &= ~I_DIRTY;

		if (inode->i_state & (I_FREEING|I_CLEAR))
			goto out;

		list_move(&inode->i_list, &inode_in_use);
	}
 out:
	spin_unlock(&inode_lock);
}

static int fat_clean_inode(struct inode *inode)
{
	int ret = 0;

	if (!(inode->i_state & I_DIRTY))
		return 0;

	ret = fat_sync_inode(inode);
	if (!ret) {
		pr_debug("Sync inode ok, make it clean!\n");
		mark_inode_clean(inode);
	}

	return ret;
}
#endif /* CONFIG_SNSC_FS_FAT_BATCH_SYNC */

static int fat_file_release(struct inode *inode, struct file *filp)
{
#ifdef CONFIG_SNSC_FS_FAT_BATCH_SYNC /* E_BOOK */
	struct msdos_sb_info *sbi = MSDOS_SB(filp->f_mapping->host->i_sb);
	int ret = 0;
#endif /* CONFIG_SNSC_FS_FAT_BATCH_SYNC */

	if ((filp->f_mode & FMODE_WRITE) &&
	     MSDOS_SB(inode->i_sb)->options.flush) {
		fat_flush_inodes(inode->i_sb, inode, NULL);
		congestion_wait(BLK_RW_ASYNC, HZ/10);
	}
#ifdef CONFIG_SNSC_FS_FAT_BATCH_SYNC /* E_BOOK */
	if (sbi->options.batch_sync) {
		mutex_lock(&inode->i_mutex);
		ret = fat_clean_inode(inode);
		mutex_unlock(&inode->i_mutex);
	}
	return ret;
#else /* CONFIG_SNSC_FS_FAT_BATCH_SYNC */
	return 0;
#endif /* CONFIG_SNSC_FS_FAT_BATCH_SYNC */
}

#ifdef CONFIG_SNSC_FS_FAT_BATCH_SYNC /* E_BOOK */
static int fat_file_release_nosync(struct inode *inode, struct file *filp)
{
	if ((filp->f_mode & FMODE_WRITE) &&
	     MSDOS_SB(inode->i_sb)->options.flush) {
		fat_flush_inodes(inode->i_sb, inode, NULL);
		congestion_wait(WRITE, HZ/10);
	}
	return 0;
}

static ssize_t fat_sync_write(struct file *filp, const char __user *buf, size_t len, loff_t *ppos)
{
	ssize_t ret;

	ret = do_sync_write(filp, buf, len, ppos);

	return ret;
}

static int fat_fsync(struct file *filp, int datasync)
{
	struct inode *inode = filp->f_mapping->host;
	struct msdos_sb_info *sbi = MSDOS_SB(inode->i_sb);
	int ret;

	if (sbi->options.batch_sync)
		ret = fat_clean_inode(inode);
	else
		ret = file_fsync(filp, datasync);

	return ret;
}
#endif /* CONFIG_SNSC_FS_FAT_BATCH_SYNC */

int fat_file_fsync(struct file *filp, int datasync)
{
	struct inode *inode = filp->f_mapping->host;
	int res, err;

	res = generic_file_fsync(filp, datasync);
	err = sync_mapping_buffers(MSDOS_SB(inode->i_sb)->fat_inode->i_mapping);

	return res ? res : err;
}


const struct file_operations fat_file_operations = {
	.llseek		= generic_file_llseek,
	.read		= do_sync_read,
#ifdef CONFIG_SNSC_FS_FAT_BATCH_SYNC /* E_BOOK */
	.write		= fat_sync_write,
#else /* CONFIG_SNSC_FS_FAT_BATCH_SYNC */
	.write		= do_sync_write,
#endif /* CONFIG_SNSC_FS_FAT_BATCH_SYNC */
	.aio_read	= generic_file_aio_read,
	.aio_write	= generic_file_aio_write,
	.mmap		= generic_file_mmap,
#ifdef CONFIG_SNSC_FS_FAT_BATCH_SYNC /* E_BOOK */
	.open		= generic_file_open,
#endif /* CONFIG_SNSC_FS_FAT_BATCH_SYNC */
	.release	= fat_file_release,
	.unlocked_ioctl	= fat_generic_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= fat_generic_compat_ioctl,
#endif
#ifdef CONFIG_SNSC_FS_FAT_BATCH_SYNC /* E_BOOK */
	.fsync		= fat_fsync,
#else /* CONFIG_SNSC_FS_FAT_BATCH_SYNC */
	.fsync		= fat_file_fsync,
#endif /* CONFIG_SNSC_FS_FAT_BATCH_SYNC */
	.splice_read	= generic_file_splice_read,
};

#ifdef CONFIG_SNSC_FS_FAT_BATCH_SYNC /* E_BOOK */
const struct file_operations fat_file_operations_nosync = {
	.llseek		= generic_file_llseek,
	.read		= do_sync_read,
	.write		= fat_sync_write,
	.aio_read	= generic_file_aio_read,
  	.aio_write	= generic_file_aio_write,
	.mmap		= generic_file_mmap,
	.open           = generic_file_open,
	.release	= fat_file_release_nosync,
	.unlocked_ioctl	= fat_generic_ioctl,
	.fsync		= fat_fsync,
};
#endif /* CONFIG_SNSC_FS_FAT_BATCH_SYNC */

static int fat_cont_expand(struct inode *inode, loff_t size)
{
	struct address_space *mapping = inode->i_mapping;
	loff_t start = inode->i_size, count = size - inode->i_size;
	int err;

	err = generic_cont_expand_simple(inode, size);
	if (err)
		goto out;

	inode->i_ctime = inode->i_mtime = CURRENT_TIME_SEC;
	mark_inode_dirty(inode);
	if (IS_SYNC(inode)) {
		int err2;

		/*
		 * Opencode syncing since we don't have a file open to use
		 * standard fsync path.
		 */
		err = filemap_fdatawrite_range(mapping, start,
					       start + count - 1);
		err2 = sync_mapping_buffers(mapping);
		if (!err)
			err = err2;
		err2 = write_inode_now(inode, 1);
		if (!err)
			err = err2;
		if (!err) {
			err =  filemap_fdatawait_range(mapping, start,
						       start + count - 1);
		}
	}
out:
	return err;
}

/* Free all clusters after the skip'th cluster. */
static int fat_free(struct inode *inode, int skip)
{
	struct super_block *sb = inode->i_sb;
	int err, wait, free_start, i_start, i_logstart;

	if (MSDOS_I(inode)->i_start == 0)
		return 0;

	fat_cache_inval_inode(inode);

#ifdef CONFIG_SNSC_FS_FAT_BATCH_SYNC /* E_BOOK */
	wait = IS_DIRSYNC(inode) || MSDOS_SB(sb)->options.batch_sync;
#else /* CONFIG_SNSC_FS_FAT_BATCH_SYNC */
	wait = IS_DIRSYNC(inode);
#endif /* CONFIG_SNSC_FS_FAT_BATCH_SYNC */
	i_start = free_start = MSDOS_I(inode)->i_start;
	i_logstart = MSDOS_I(inode)->i_logstart;

	/* First, we write the new file size. */
	if (!skip) {
		MSDOS_I(inode)->i_start = 0;
		MSDOS_I(inode)->i_logstart = 0;
	}
	MSDOS_I(inode)->i_attrs |= ATTR_ARCH;
	inode->i_ctime = inode->i_mtime = CURRENT_TIME_SEC;
	if (wait) {
		err = fat_sync_inode(inode);
		if (err) {
			MSDOS_I(inode)->i_start = i_start;
			MSDOS_I(inode)->i_logstart = i_logstart;
			return err;
		}
	} else
		mark_inode_dirty(inode);

	/* Write a new EOF, and get the remaining cluster chain for freeing. */
	if (skip) {
		struct fat_entry fatent;
		int ret, fclus, dclus;

		ret = fat_get_cluster(inode, skip - 1, &fclus, &dclus);
		if (ret < 0)
			return ret;
		else if (ret == FAT_ENT_EOF)
			return 0;

		fatent_init(&fatent);
		ret = fat_ent_read(inode, &fatent, dclus);
		if (ret == FAT_ENT_EOF) {
			fatent_brelse(&fatent);
			return 0;
		} else if (ret == FAT_ENT_FREE) {
			fat_fs_error(sb,
				     "%s: invalid cluster chain (i_pos %lld)",
				     __func__, MSDOS_I(inode)->i_pos);
			ret = -EIO;
		} else if (ret > 0) {
			err = fat_ent_write(inode, &fatent, FAT_ENT_EOF, wait);
			if (err)
				ret = err;
		}
		fatent_brelse(&fatent);
		if (ret < 0)
			return ret;

		free_start = ret;
	}
	inode->i_blocks = skip << (MSDOS_SB(sb)->cluster_bits - 9);

	/* Freeing the remained cluster chain */
	return fat_free_clusters(inode, free_start);
}

void fat_truncate_blocks(struct inode *inode, loff_t offset)
{
	struct msdos_sb_info *sbi = MSDOS_SB(inode->i_sb);
	const unsigned int cluster_size = sbi->cluster_size;
	int nr_clusters;

	/*
	 * This protects against truncating a file bigger than it was then
	 * trying to write into the hole.
	 */
	if (MSDOS_I(inode)->mmu_private > offset)
		MSDOS_I(inode)->mmu_private = offset;

	nr_clusters = (offset + (cluster_size - 1)) >> sbi->cluster_bits;

	fat_free(inode, nr_clusters);
	fat_flush_inodes(inode->i_sb, inode, NULL);
}

int fat_getattr(struct vfsmount *mnt, struct dentry *dentry, struct kstat *stat)
{
	struct inode *inode = dentry->d_inode;
	generic_fillattr(inode, stat);
	stat->blksize = MSDOS_SB(inode->i_sb)->cluster_size;
	return 0;
}
EXPORT_SYMBOL_GPL(fat_getattr);

static int fat_sanitize_mode(const struct msdos_sb_info *sbi,
			     struct inode *inode, umode_t *mode_ptr)
{
	mode_t mask, perm;

	/*
	 * Note, the basic check is already done by a caller of
	 * (attr->ia_mode & ~FAT_VALID_MODE)
	 */

	if (S_ISREG(inode->i_mode))
		mask = sbi->options.fs_fmask;
	else
		mask = sbi->options.fs_dmask;

	perm = *mode_ptr & ~(S_IFMT | mask);

	/*
	 * Of the r and x bits, all (subject to umask) must be present. Of the
	 * w bits, either all (subject to umask) or none must be present.
	 *
	 * If fat_mode_can_hold_ro(inode) is false, can't change w bits.
	 */
	if ((perm & (S_IRUGO | S_IXUGO)) != (inode->i_mode & (S_IRUGO|S_IXUGO)))
		return -EPERM;
	if (fat_mode_can_hold_ro(inode)) {
		if ((perm & S_IWUGO) && ((perm & S_IWUGO) != (S_IWUGO & ~mask)))
			return -EPERM;
	} else {
		if ((perm & S_IWUGO) != (S_IWUGO & ~mask))
			return -EPERM;
	}

	*mode_ptr &= S_IFMT | perm;

	return 0;
}

static int fat_allow_set_time(struct msdos_sb_info *sbi, struct inode *inode)
{
	mode_t allow_utime = sbi->options.allow_utime;

	if (current_fsuid() != inode->i_uid) {
		if (in_group_p(inode->i_gid))
			allow_utime >>= 3;
		if (allow_utime & MAY_WRITE)
			return 1;
	}

	/* use a default check */
	return 0;
}

int fat_setsize(struct inode *inode, loff_t offset)
{
	int error;

	error = simple_setsize(inode, offset);
	if (error)
		return error;
	fat_truncate_blocks(inode, offset);

	return error;
}

#define TIMES_SET_FLAGS	(ATTR_MTIME_SET | ATTR_ATIME_SET | ATTR_TIMES_SET)
/* valid file mode bits */
#define FAT_VALID_MODE	(S_IFREG | S_IFDIR | S_IRWXUGO)

int fat_setattr(struct dentry *dentry, struct iattr *attr)
{
	struct msdos_sb_info *sbi = MSDOS_SB(dentry->d_sb);
	struct inode *inode = dentry->d_inode;
	unsigned int ia_valid;
	int error;

	/*
	 * Expand the file. Since inode_setattr() updates ->i_size
	 * before calling the ->truncate(), but FAT needs to fill the
	 * hole before it. XXX: this is no longer true with new truncate
	 * sequence.
	 */
	if (attr->ia_valid & ATTR_SIZE) {
		if (attr->ia_size > inode->i_size) {
			error = fat_cont_expand(inode, attr->ia_size);
			if (error || attr->ia_valid == ATTR_SIZE)
				goto out;
			attr->ia_valid &= ~ATTR_SIZE;
		}
	}

	/* Check for setting the inode time. */
	ia_valid = attr->ia_valid;
	if (ia_valid & TIMES_SET_FLAGS) {
		if (fat_allow_set_time(sbi, inode))
			attr->ia_valid &= ~TIMES_SET_FLAGS;
	}

	error = inode_change_ok(inode, attr);
	attr->ia_valid = ia_valid;
	if (error) {
		if (sbi->options.quiet)
			error = 0;
		goto out;
	}

	if (((attr->ia_valid & ATTR_UID) &&
	     (attr->ia_uid != sbi->options.fs_uid)) ||
	    ((attr->ia_valid & ATTR_GID) &&
	     (attr->ia_gid != sbi->options.fs_gid)) ||
	    ((attr->ia_valid & ATTR_MODE) &&
	     (attr->ia_mode & ~FAT_VALID_MODE)))
		error = -EPERM;

	if (error) {
		if (sbi->options.quiet)
			error = 0;
		goto out;
	}

	/*
	 * We don't return -EPERM here. Yes, strange, but this is too
	 * old behavior.
	 */
	if (attr->ia_valid & ATTR_MODE) {
		if (fat_sanitize_mode(sbi, inode, &attr->ia_mode) < 0)
			attr->ia_valid &= ~ATTR_MODE;
	}

	if (attr->ia_valid & ATTR_SIZE) {
		error = fat_setsize(inode, attr->ia_size);
		if (error)
			goto out;
	}

	generic_setattr(inode, attr);
	mark_inode_dirty(inode);
out:
	return error;
}
EXPORT_SYMBOL_GPL(fat_setattr);

const struct inode_operations fat_file_inode_operations = {
	.setattr	= fat_setattr,
	.getattr	= fat_getattr,
};
