#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>

extern void ntx_bt_power_ctrl (int iIsBTEnable);
static int __init sdio_bt_pwr_init(void)
{
	ntx_bt_power_ctrl(1);
	return 0;
}
module_init(sdio_bt_pwr_init);

static void __exit sdio_bt_pwr_exit(void)
{
	ntx_bt_power_ctrl(0);
}
module_exit(sdio_bt_pwr_exit);

MODULE_DESCRIPTION("sdio bluetooth power control driver");
MODULE_AUTHOR("Netronix");
MODULE_LICENSE("GPL");
