#include <linux/spi/spi.h>
#include <linux/cyttsp.h>

#define CY_USE_MT		/* define if using Multi-Touch */
#define CY_USE_SPI		/* define if using SPI interface */
#define CY_USE_I2C		/* define if using I2C interface  */

#ifdef CY_USE_SPI
#define CY_SPI_IRQ_GPIO	139	/* xxx board GPIO */

/* optional init function; set up IRQ GPIO;
 * call reference in platform data structure
 */
static int cyttsp_spi_init(int on)
{
	int ret;


	/* add any special code to initialize any required system hw
	 * such as regulators or gpio pins
	 */


	if (on) {
		gpio_direction_input(CY_SPI_IRQ_GPIO);
		/* for MSM systems the call to gpio_direction_input can be
		 * replaced with the more explicit call:
		 gpio_tlmm_config(GPIO_CFG(CY_SPI_IRQ_GPIO, 0, GPIO_INPUT,
			GPIO_PULL_UP, GPIO_6MA), GPIO_ENABLE);
		 */
		ret = gpio_request(CY_SPI_IRQ_GPIO, "CYTTSP IRQ GPIO");
		if (ret) {
			printk(KERN_ERR "%s: Failed to request GPIO %d\n",
			       __func__, CY_SPI_IRQ_GPIO);
			return ret;
		}
	} else {
		gpio_free(CY_SPI_IRQ_GPIO);
	}
	return 0;
}

static int cyttsp_spi_wakeup(void)
{
	return 0;
}

static struct cyttsp_platform_data cypress_spi_ttsp_platform_data = {
	.wakeup = cyttsp_spi_wakeup,
	.init = cyttsp_spi_init,
#ifdef CY_USE_MT
	.mt_sync = input_mt_sync,
#endif
	.maxx = 240,
	.maxy = 320,
	.flags = 0,
	.gen = CY_GEN3,
	.use_st = 0,
	.use_mt = 1,
	.use_trk_id = 0,
	.use_hndshk = 1,
	.use_timer = 0,
	.use_sleep = 1,
	.use_gestures = 0,
	.use_load_file = 1,
	.use_force_fw_update = 1,
	.use_virtual_keys = 0,
	/* activate up to 4 groups
	 * and set active distance
	 */
	.gest_set = CY_GEST_GRP_NONE | CY_ACT_DIST,
	/* change act_intrvl to customize the Active power state
	 * scanning/processing refresh interval for Operating mode
	 */
	.act_intrvl = CY_ACT_INTRVL_DFLT,
	/* change tch_tmout to customize the touch timeout for the
	 * Active power state for Operating mode
	 */
	.tch_tmout = CY_TCH_TMOUT_DFLT,
	/* change lp_intrvl to customize the Low Power power state
	 * scanning/processing refresh interval for Operating mode
	 */
	.lp_intrvl = CY_LP_INTRVL_DFLT,
	.name = CY_SPI_NAME,
	.irq_gpio = CY_SPI_IRQ_GPIO,
};

static struct spi_board_info xxx_cyttsp_spi_board_info[] __initdata = {
	{
		.modalias = CY_SPI_NAME,
		.platform_data  = &cypress_spi_ttsp_platform_data,
		.irq = INT_34XX_SPI4_IRQ,
		.max_speed_hz   = 1000000,
		.bus_num = 4,
		.chip_select = 0,
		.mode = SPI_MODE_0,
	},
};

static void __init xxx_spi_cyttsp_init(void)
{
	printk(KERN_INFO "irq = %d\n",
		xxx_cyttsp_spi_board_info[0].irq);
	spi_register_board_info(xxx_cyttsp_spi_board_info,
		ARRAY_SIZE(xxx_cyttsp_spi_board_info));
}
#endif /* CY_USE_SPI */

#ifdef CY_USE_I2C
#define CY_I2C_IRQ_GPIO	133	/* xxx board GPIO */

/* optional init function; set up IRQ GPIO;
 * call reference in platform data structure
 */
static int cyttsp_i2c_init(int on)
{
	int ret;


	/* add any special code to initialize any required system hw
	 * such as regulators or gpio pins
	 */


	if (on) {
		gpio_direction_input(CY_I2C_IRQ_GPIO);
		/* for MSM systems the call to gpio_direction_input can be
		 * replaced with the more explicit call:
		 gpio_tlmm_config(GPIO_CFG(CY_I2C_IRQ_GPIO, 0, GPIO_INPUT,
			GPIO_PULL_UP, GPIO_6MA), GPIO_ENABLE);
		 */
		ret = gpio_request(CY_I2C_IRQ_GPIO, "CYTTSP IRQ GPIO");
		if (ret) {
			printk(KERN_ERR "%s: Failed to request GPIO %d\n",
			       __func__, CY_I2C_IRQ_GPIO);
			return ret;
		}
	} else {
		gpio_free(CY_I2C_IRQ_GPIO);
	}
	return 0;
}

static int cyttsp_i2c_wakeup(void)
{
	return 0;
}

static struct cyttsp_platform_data cypress_i2c_ttsp_platform_data = {
	.wakeup = cyttsp_i2c_wakeup,
	.init = cyttsp_i2c_init,
#ifdef CY_USE_MT
	.mt_sync = input_mt_sync,
#endif
	.maxx = 240,
	.maxy = 320,
	.flags = 0,
	.gen = CY_GEN3,
	.use_st = 0,
	.use_mt = 1,
	.use_trk_id = 0,
	.use_hndshk = 1,
	.use_timer = 0,
	.use_sleep = 1,
	.use_gestures = 0,
	.use_load_file = 1,
	.use_force_fw_update = 1,
	.use_virtual_keys = 0,
	/* activate up to 4 groups
	 * and set active distance
	 */
	.gest_set = CY_GEST_GRP_NONE | CY_ACT_DIST,
	/* change act_intrvl to customize the Active power state
	 * scanning/processing refresh interval for Operating mode
	 */
	.act_intrvl = CY_ACT_INTRVL_DFLT,
	/* change tch_tmout to customize the touch timeout for the
	 * Active power state for Operating mode
	 */
	.tch_tmout = CY_TCH_TMOUT_DFLT,
	/* change lp_intrvl to customize the Low Power power state
	 * scanning/processing refresh interval for Operating mode
	 */
	.lp_intrvl = CY_LP_INTRVL_DFLT,
	.name = CY_I2C_NAME,
	.irq_gpio = CY_I2C_IRQ_GPIO,
};

static struct i2c_board_info __initdata xxx_i2c4_boardinfo[] = {
	{
		I2C_BOARD_INFO(CY_I2C_NAME, 0x24),
		/* helper method for initializing IRQ GPIO
		.irq = OMAP_GPIO_IRQ(CY_I2C_IRQ_GPIO), // OMAP METHOD
		.irq = MSM_GPIO_TO_INT(CY_I2C_IRQ_GPIO, // MSM METHOD
		*/
		.platform_data = &cypress_i2c_ttsp_platform_data,
	},
#endif /* CY_USE_I2C */
