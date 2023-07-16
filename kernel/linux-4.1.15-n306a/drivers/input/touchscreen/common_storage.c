#include <linux/fs.h>
#include <asm/uaccess.h>
#include "common_define.h"

int init_storage(void)
{
    return 0;
}

int write_to_storage(char *name, char *w_buf, int buf_size)
{
    struct file  *cali_file = NULL;
    mm_segment_t fs;
    loff_t pos;
    int err;
    cali_file = filp_open(name, O_CREAT | O_RDWR, 0666);

    if (IS_ERR(cali_file))
    {
        err = PTR_ERR(cali_file);
        printk(KERN_ERR "%s: filp_open error!err=%d,path=%s\n", __func__, err, name);
        return -1;
    }
    else
    {
        fs = get_fs();
        set_fs(KERNEL_DS);
        pos = 0;
        vfs_write(cali_file, w_buf, sizeof(buf_size), &pos);
        set_fs(fs);
    }

    filp_close(cali_file, NULL);
    return 0;
}

int read_from_storage(char *name, char *r_buf, int buf_size)
{
    struct file  *cali_file = NULL;
    mm_segment_t fs;
    loff_t pos;
    int err;
    cali_file = filp_open(name, O_CREAT | O_RDWR, 0644);

    if (IS_ERR(cali_file))
    {
        err = PTR_ERR(cali_file);
        printk(KERN_ERR "%s: filp_open error!err=%d,path=%s\n", __func__, err, name);
        return -1;
    }
    else
    {
        fs = get_fs();
        set_fs(KERNEL_DS);
        pos = 0;
        vfs_read(cali_file, r_buf, sizeof(buf_size), &pos);
        set_fs(fs);
    }

    filp_close(cali_file, NULL);
    return 0;
}

int remove_storage(void)
{
    return 0;
}

const struct stk_storage_ops stk_s_ops =
{
    .init_storage           = init_storage,
    .write_to_storage       = write_to_storage,
    .read_from_storage      = read_from_storage,
    .remove                 = remove_storage,
};
