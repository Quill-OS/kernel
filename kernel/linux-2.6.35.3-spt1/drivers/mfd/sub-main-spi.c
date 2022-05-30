/*
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/spi/spi.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/wakelock.h>
#include <linux/kthread.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/mfd/sub-main/core.h>
#include <linux/mfd/core.h>
#include <linux/poll.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/machine.h>
#include <linux/earlysuspend.h>
#include <mach/gpio.h>
#include <mach/iomux-mx50.h>
#include <mach/gpio.h>


#define	SPI_FIFOSIZE		1	/* Bust size in bytes */
#define SPI_WAKE_INT	(2*32 + 25)	/*GPIO_3_25*/
#define SPI_RCV_RDY		(3*32 + 23)	/*GPIO_4_23 */
#define SPI_SUB_INT		(2*32 + 26)	/* GPIO_3_26 */
#define CSPI_CS1		(3*32 + 13)	/*GPIO_4_13 CS for NOR */
#define LOG_INT			(4*32 + 24)	/* GPIO_5_24 */
#define BOARD_ID	(5*32+14)	/* GPIO_6_14 */

#define WDT2_ENABLE		1	// enable
#define WDT2_DISABLE	0	// disable
#define WDT2_TIME		10	// 10 seconds

static int set_watch_dog_timer_2(u8 enable, u8 wdt_time);
static void sub_wdt_start_queue(struct work_struct *ignored);
static void sub_wdt_stop_queue(struct work_struct *ignored);
static void sub_wdt_clear_queue(struct work_struct *ignored);
static DECLARE_WORK(sub_wdt_start_work, sub_wdt_start_queue);
static DECLARE_WORK(sub_wdt_stop_work, sub_wdt_stop_queue);
static DECLARE_WORK(sub_wdt_clear_work, sub_wdt_clear_queue);
struct workqueue_struct *sub_wdt_queue;
static struct regulator *ldo8 = NULL;	//USBPHY 2.5V
static int ldo8_enabled;
static unsigned int sub_mcu_fw_ver = 0;

extern void sub_power_state_change_detect(void);
extern void sub_power_key_state_change_detect(void);
extern int get_ac_online_info(void);
extern void turn_off_v29_vcc_at_standby(void);
extern void turn_on_v29_vcc_at_standby(void);

struct sub_cpu_bci_device_info *confirm_the_di(void);

int read_all_status_reg(void);

static struct sub_main *the_sub;

static int v29_vcc_off_flag = 0;

/*****************************************************************************
	spi_usleep
*****************************************************************************/
static void spi_usleep(unsigned int usecs)
{
	struct hrtimer_sleeper *sleeper, _sleeper;
	enum hrtimer_mode mode = HRTIMER_MODE_REL;
	int state = TASK_INTERRUPTIBLE;

	sleeper = &_sleeper;
	/*
	 * This is really just a reworked and simplified version
	 * of do_nanosleep().
	 */
	hrtimer_init(&sleeper->timer, CLOCK_MONOTONIC, mode);
	hrtimer_set_expires(&sleeper->timer, ktime_set(0, usecs*NSEC_PER_USEC));
	hrtimer_init_sleeper(sleeper, current);

	do {
		set_current_state(state);
		hrtimer_start_expires(&sleeper->timer, mode);
		if (sleeper->task)
			schedule();
		hrtimer_cancel(&sleeper->timer);
		mode = HRTIMER_MODE_ABS;
	} while (sleeper->task && !signal_pending(current));
}

/*****************************************************************************
	spi_read_write
*****************************************************************************/
static int spi_read_write(struct spi_device *spi, u8 * buf, u32 len)
{
	struct spi_message m;
	struct spi_transfer t;
	int status;

	if (len > SPI_FIFOSIZE || len <= 0)
		return -1;
	
	spi_message_init(&m);
	memset(&t, 0, sizeof t);

	t.tx_buf = buf;
	t.rx_buf = buf;
	t.len = ((len - 1) >> 2) + 1;

	spi_message_add_tail(&t, &m);
	status = spi_sync(spi, &m);

	if ( status != 0 || m.status != 0) {
		printk("%s:%u\n", __func__, __LINE__);
		printk(KERN_ERR "%s: error\n", __func__);
	}

	//DEBUG(MTD_DEBUG_LEVEL2, "%s: len: 0x%x success\n", __func__, len);
	//printk("%s: len: 0x%x success\n", __func__, len);
	
	return status;

}

/*****************************************************************************
	sub_send_status_hi_check
*****************************************************************************/
static int sub_send_status_hi_check(void)
{
	int i = 0;
	while( !gpio_get_value(SPI_RCV_RDY) )
	{
		if( i > 500){
			if( i > 1000 ){
				return -ECOMM;
			}
			spi_usleep(1000);
		}else{
			udelay(1);
		}
		i++;
	}
		
	return 0;
	
}

/*****************************************************************************
	sub_send_status_low_check
*****************************************************************************/
static int sub_send_status_low_check(void)
{
	int i = 0;
	while( gpio_get_value(SPI_RCV_RDY) )
	{
		if( the_sub->control_mode == SUB_NORMAL_MODE){
			if( i > 700)
				return -ECOMM;
			i++;
			udelay(1);
		}else{
			if( i > 1000000) //1sec
				return -ECOMM;
			i++;
			udelay(1);
		}
	}
	
	return 0;
	
}

/*****************************************************************************
	sub_main_spi_byte_read
*****************************************************************************/
static int sub_main_spi_byte_read(u8 *data)
{
	int ret = 0;
	
	/* SPI_RCV_RDY status check */
	ret = sub_send_status_low_check();
	if( ret ){
		printk("%s:%u\n", __func__, __LINE__);
		printk("[Error] SPI_RCV_RDY didn't become low.\n");
		goto rcv_err;
	}
	
	/* SPI_WAKE_INT Hi */
	gpio_set_value(SPI_WAKE_INT, 1);
	
	udelay(10);
	
	/* SPI_RCV_RDY status check */
	ret = sub_send_status_hi_check();
	if(ret){
		printk("%s:%u\n", __func__, __LINE__);
		printk("[Error] SPI_RCV_RDY didn't become high.\n");
		goto rcv_err;
	}
	
	
	/* Read Data */
	ret = spi_read_write(the_sub->control_data, data, 1);
	
	//printk("read date = %02X\n",*data);
	
	if(ret){
		printk("%s:%u\n", __func__, __LINE__);
		printk("[Error] sub_main_spi_byte_read.\n");
		goto rcv_err;
	}
	
	//Debug
	//printk("Read data = %02X\n", *data);
	
rcv_err:
	
	/* SPI_WAKE_INT Low */
	gpio_set_value(SPI_WAKE_INT, 0);
	
	return ret;
}

/*****************************************************************************
	sub_main_spi_byte_write
*****************************************************************************/
static int sub_main_spi_byte_write(u8 *data)
{
	int ret;
	
	/* SPI_RCV_RDY status check */
	ret = sub_send_status_low_check();
	if(ret){
		printk("%s:%u\n", __func__, __LINE__);
		printk("SPI_RCV_RDY didn't become low. Error!!!\n");
		goto send_err;
	}
	
	/* SPI_WAKE_INT Hi */
	gpio_set_value(SPI_WAKE_INT, 1);
	
	udelay(20);
	
	/* SPI_RCV_RDY status check */
	ret = sub_send_status_hi_check();
	if(ret){
		printk("%s:%u\n", __func__, __LINE__);
		printk("SPI_RCV_RDY didn't become high. Error!!!\n");
		goto send_err;
	}
	
	/* Writ Data */
	ret = spi_read_write(the_sub->control_data, data, 1);
	if(ret){
		printk("%s:%u\n", __func__, __LINE__);
		printk("sub_main_spi_bute_write Error!!!\n");
	}
	
send_err:
	
	/* SPI_WAKE_INT Low */
	gpio_set_value(SPI_WAKE_INT, 0);
	
	return ret;
}

/*****************************************************************************
	sub_main_send_cmd
*****************************************************************************/
static int sub_main_send_cmd(u8 cmd, u8 data_size, u8 *send_buf)
{
	u8 data;
	u8 send_cnt = 0;
	int ret;
	unsigned short chk_sum = 0;
	
	if( the_sub->control_mode != SUB_NORMAL_MODE){
		return -ECANCELED;
	}
	
	/* Send Frame Start */
	data = 0xEE;
	ret = sub_main_spi_byte_write(&data);
	if(ret)
		goto err_spi;
	
	/* Send data size */
	data = CMD_LENGTH + data_size + CHECK_SUM_LENGTH;
	chk_sum = data;
	ret = sub_main_spi_byte_write(&data);
	if(ret)
		goto err_spi;
	
	/* Send Command */
	data = cmd;
	chk_sum += cmd;
	ret = sub_main_spi_byte_write(&data);
	if(ret)
		goto err_spi;
	send_cnt++;
	
	/* Send_Data */
	while(send_cnt < (data_size + CMD_LENGTH))
	{
		data = *send_buf;
		chk_sum += data;
		ret = sub_main_spi_byte_write(&data);
		if(ret)
			goto err_spi;
		send_buf++;
		send_cnt++;
	}
	
	chk_sum &= 0x00FF;
	ret = sub_main_spi_byte_write((u8 *)&chk_sum);
		if(ret)
			goto err_spi;
	
	return 0;
	
err_spi:
	printk("sub_main_spi_read error!!!\n");
	spi_usleep(1000 * 1000);
	return ret;
}

/*****************************************************************************
	sub_main_read_response
*****************************************************************************/
#define RCV_DEBUG	1
static int sub_main_read_response( u8 *buf, u8 cmd, u8 len)
{
	u8 data_size;
	u8 temp, read_sum_check;
	u8 rcv_cnt = 0;
	unsigned short sum_chk;
	int ret;

#ifdef RCV_DEBUG
	int debug_rcv_cnt = 0, i = 0;
	u8 debug_buff[100];
#endif
	
	if( the_sub->control_mode != SUB_NORMAL_MODE){
		return -ECANCELED;
	}
	
	/* Read Frame Start */
	ret = sub_main_spi_byte_read((u8 *)&temp);
#ifdef RCV_DEBUG
	debug_rcv_cnt++;
	debug_buff[i] = temp;
	i++;
#endif	
	if( ret || (temp != 0xEE) ){
		printk("%s:%u\n", __func__, __LINE__);
		printk("Error!!! Read Frame Start.\n");
		printk("Frame Start = 0x%02X\n",temp);
		goto err_spi;
	}
	
	/* Receive DataSize */
	ret = sub_main_spi_byte_read((u8 *)&temp);
#ifdef RCV_DEBUG
	debug_rcv_cnt++;
	debug_buff[i] = temp;
	i++;
#endif	
	if(ret){
		printk("%s:%u\n", __func__, __LINE__);
		printk("Error!!! Read Frame Start.\n");
		goto err_spi;
	}
	data_size = temp;
	sum_chk = data_size;
	
	/* Receive Command Type */
	ret = sub_main_spi_byte_read((u8 *)&temp);
#ifdef RCV_DEBUG
	debug_rcv_cnt++;
	debug_buff[i] = temp;
	i++;
#endif	
	sum_chk += temp;
	if(ret){
		printk("%s:%u\n", __func__, __LINE__);
		printk("Error!!! Receive Command Type.\n");
		goto err_spi;
	}
	if( temp != cmd ){
		printk("%s:%u\n", __func__, __LINE__);
		printk("Error!!! Didn't match command type.\n");
		printk("Receive command = %02X\n",temp);
		goto err_spi;
	}
	
	/* Receive_Data */
	while( rcv_cnt < (data_size - CMD_LENGTH - CHECK_SUM_LENGTH) )
	{
		if( rcv_cnt >= len ){
			printk("%s:%u\n", __func__, __LINE__);
			printk("Sub-Main Read buffer overflow!!\n");
			goto err_spi;
		}
		
		ret = sub_main_spi_byte_read(buf);
#ifdef RCV_DEBUG
		debug_rcv_cnt++;
		debug_buff[i] = *buf;
		i++;
#endif	
		if(ret)
			goto err_spi;
		sum_chk += *buf;
		buf++;
		rcv_cnt++;
	}
	
	//SUM Check
	ret = sub_main_spi_byte_read(&read_sum_check);
#ifdef RCV_DEBUG
	debug_rcv_cnt++;
	debug_buff[i] = read_sum_check;
	i++;
#endif	
	if(ret)
			goto err_spi;
	
	sum_chk &= 0xFF;
	
	if( read_sum_check != sum_chk ){
		printk("%s:%u\n", __func__, __LINE__);
		printk("read_sum_check = %02X\n",read_sum_check);
		printk("sum_chk = %02X\n",sum_chk);
		printk("Error!!! SUM Check.\n");
		goto err_spi;
	}

	return 0;
	
err_spi:
	printk("sub_main_read_response error!!!");
	for(i=0; i != debug_rcv_cnt; i++){
		printk("Receive data[%d] = %02X\n",i,debug_buff[i]);
	}
	spi_usleep(1000 * 1000);
	return -ECOMM;;
}

/*****************************************************************************
	send_standby_req_cmd();
*****************************************************************************/
void send_standby_req_cmd(void)
{
	int ret;
	
	mutex_lock(&the_sub->io_lock);
	
	if( the_sub->control_mode != SUB_NORMAL_MODE){
		return;
	}

retry:
	
	ret = sub_main_send_cmd(STANDBY_REQ_CMD, 0, NULL);
	if(ret)
		goto retry;
	
	ret = sub_main_read_response( NULL, STANDBY_REQ_CMD, 0 );
	
	if(ret)
		goto retry;

	mutex_unlock(&the_sub->io_lock);
	
}

/*****************************************************************************
	send_resume_req_cmd();
*****************************************************************************/
void send_resume_req_cmd(void)
{
	int ret;
	
	mutex_lock(&the_sub->io_lock);
	
	if( the_sub->control_mode != SUB_NORMAL_MODE){
		return;
	}

retry:
	
	ret = sub_main_send_cmd(RESUME_REQ_CMD, 0, NULL);
	if(ret)
		goto retry;
	
	ret = sub_main_read_response( NULL, RESUME_REQ_CMD, 0 );
	
	if(ret)
		goto retry;

	mutex_unlock(&the_sub->io_lock);
	
}

/*****************************************************************************
	get_sub_mcu_fw_ver
*****************************************************************************/
static int get_sub_mcu_fw_ver(void)
{

	int ret = 0;
	u8 sub_ver[SUB_VER_LENGTH];
	
	mutex_lock(&the_sub->io_lock);
	
	ret = sub_main_send_cmd(REQ_MCU_FW_VER_CMD, 0, NULL);
	if(ret)
		goto err;
	
	ret = sub_main_read_response( (u8 *)&sub_ver, 
								REQ_MCU_FW_VER_CMD, SUB_VER_LENGTH );
	
	if(ret)
		goto err;
	
	sub_mcu_fw_ver = (sub_ver[7] | (sub_ver[6] << 8)  | (sub_ver[5] << 16) | (sub_ver[4] << 24));
	
err:
	
	mutex_unlock(&the_sub->io_lock);
	
	return ret;
	
}

/*****************************************************************************
	comfirm_sub_mcu_fw_ver
*****************************************************************************/
static unsigned int comfirm_sub_mcu_fw_ver(void)
{
	return sub_mcu_fw_ver;
}

/*****************************************************************************
	wm8321_init_complete_cmd();
*****************************************************************************/
void wm8321_init_complete_cmd(void)
{
	int ret;
	
	if( comfirm_sub_mcu_fw_ver() < 21){
		printk("This sub CPU version doesn't support PMIC_INIT_OK command.\n");
		return;
	}
	
	mutex_lock(&the_sub->io_lock);
	
	if( the_sub->control_mode != SUB_NORMAL_MODE){
		return;
	}

retry:
	
	ret = sub_main_send_cmd(PMIC_INIT_COMPLETE_CMD, 0, NULL);
	if(ret)
		goto retry;
	
	ret = sub_main_read_response( NULL, PMIC_INIT_COMPLETE_CMD, 0 );
	
	if(ret)
		goto retry;

	mutex_unlock(&the_sub->io_lock);
	
}
EXPORT_SYMBOL(wm8321_init_complete_cmd);

/*****************************************************************************
	set_sub_standby_flag()
*****************************************************************************/
void set_sub_standby_flag(int flag)
{
	mutex_lock(&the_sub->io_lock);
	
	the_sub->standby_flag = flag;
	
	mutex_unlock(&the_sub->io_lock);
}
EXPORT_SYMBOL(set_sub_standby_flag);

/*****************************************************************************
	sub_cpu_preparing_for_standby()
*****************************************************************************/
static void sub_cpu_preparing_for_standby(void)
{
	
	if(the_sub->standby_flag)
	{
		//printk("Send Standby Command!!!\n");
		
		//Disable IRQ from SUB CPU
		disable_irq( IOMUX_TO_IRQ_V3(SPI_SUB_INT) );
		disable_irq_wake( IOMUX_TO_IRQ_V3(SPI_SUB_INT) );	
		
		//Send standby request command to sub CPU
		send_standby_req_cmd();
		
		//Set NOR CS to low.
		gpio_direction_output(CSPI_CS1, 0);
		
		
		//Set V2.9 off setting
		if( ldo8 == NULL ){
			ldo8 = regulator_get(NULL, "LDO8");	//USBPHY 2.5V
			if(IS_ERR(ldo8)){
				printk(KERN_ERR"error: regulator_get LDO8\n"); 
				ldo8 = NULL;  
			}
		}
		ldo8_enabled = regulator_is_enabled(ldo8);
		if( !ldo8_enabled && gpio_get_value(BOARD_ID) ){
			// USBPHY 2.5V is turned on and platform is set
			turn_off_v29_vcc_at_standby();
			v29_vcc_off_flag = 1;
		}
		
		// Enable LOG_INT IRQ wakeup from standby.
		//enable_irq_wake(IOMUX_TO_IRQ_V3(LOG_INT));
	}
	
}

/*****************************************************************************
	sub_cpu_preparing_for_resume()
*****************************************************************************/
static void sub_cpu_preparing_for_resume(void)
{	
	if(the_sub->standby_flag)
	{
		//printk("Send Resume Command!!!\n");
		
		// Disable LOG_INT IRQ wakeup from standby.
		//disable_irq_wake(IOMUX_TO_IRQ_V3(LOG_INT));
		
		
		// Set V2.9 on setting
		if( !ldo8_enabled && gpio_get_value(BOARD_ID) ){
			turn_on_v29_vcc_at_standby();
			v29_vcc_off_flag = 0;
		}
		
		// Set NOR CS to hi.
		gpio_direction_output(CSPI_CS1, 1);
		
		//Enable IRQ from SUB CPU
		enable_irq( IOMUX_TO_IRQ_V3(SPI_SUB_INT) );
		enable_irq_wake( IOMUX_TO_IRQ_V3(SPI_SUB_INT) );
		
		// Send resume request command to sub CPU
		send_resume_req_cmd();
		
	}
}

int confirme_v29_vcc_off_flag(void)
{
	return v29_vcc_off_flag;
}
EXPORT_SYMBOL(confirme_v29_vcc_off_flag);

/*****************************************************************************
	disable_sub_wake_int_irq()
*****************************************************************************/
void disable_spi_sub_int_irq(void)
{
	
	disable_irq(IOMUX_TO_IRQ_V3(SPI_SUB_INT));
	disable_irq_wake(IOMUX_TO_IRQ_V3(SPI_SUB_INT));
	
}
EXPORT_SYMBOL(disable_spi_sub_int_irq);

/*****************************************************************************
	write_sub_cpu_reg
*****************************************************************************/
int write_sub_cpu_reg(u8 reg_address, u8 write_data)
{
	int ret;
	u8 write_buf[2];
	
	mutex_lock(&the_sub->io_lock);
	if( the_sub->control_mode != SUB_NORMAL_MODE){
		ret = -1;
		goto err;
	}
	
	write_buf[0] = reg_address;
	write_buf[1] = write_data;
	
	ret = sub_main_send_cmd(REG_WRITE_CMD, sizeof(write_buf) , (u8 *)write_buf);
	if(ret)
		goto err;
	
	ret = sub_main_read_response( (u8 *)write_buf, REG_WRITE_CMD, sizeof(write_buf) );
	
	if(ret)
		goto err;

err:
	mutex_unlock(&the_sub->io_lock);
	
	return ret;
	
}

/*****************************************************************************
	read_sub_cpu_reg
*****************************************************************************/
int read_sub_cpu_reg(u8 reg_address, u8 *read_data)
{
	int ret;
	u8 read_buf[2];
	
	mutex_lock(&the_sub->io_lock);
	
	if( the_sub->control_mode != SUB_NORMAL_MODE){
		ret = -ECANCELED;
		goto err;
	}
	
	ret = sub_main_send_cmd(REG_READ_CMD, sizeof(*read_data), &reg_address);
	if(ret)
		goto err;
	
	ret = sub_main_read_response( (u8 *)read_buf, REG_READ_CMD, sizeof(read_buf));
	
	if( reg_address != read_buf[0] ){
		printk("Error!!! [read_sub_cpu_reg] read reg_address.\n");
	}
	
	*read_data = read_buf[1];
	
	if(ret)
		goto err;

err:
	mutex_unlock(&the_sub->io_lock);
	
	return ret;
	
}

/*****************************************************************************
	set_bit_sub_cpu_reg
*****************************************************************************/
int set_bit_sub_cpu_reg(u8 reg_address, u8 set_bit)
{
	int ret;
	u8 temp;
	
	ret = read_sub_cpu_reg(reg_address, &temp);
	if(ret)
		goto err;
	
	temp |= set_bit;
	
	ret = write_sub_cpu_reg(reg_address, temp);
	if(ret)
		goto err;

err:
	
	return ret;
	
}

/*****************************************************************************
	clear_bit_sub_cpu_reg
*****************************************************************************/
int clear_bit_sub_cpu_reg(u8 reg_address, u8 clear_bit)
{
	int ret;
	u8 temp;
	
	ret = read_sub_cpu_reg(reg_address, &temp);
	if(ret)
		goto err;
	
	temp &= ~clear_bit;
	
	ret = write_sub_cpu_reg(reg_address, temp);
	if(ret)
		goto err;

err:
	
	return ret;
	
}

/*****************************************************************************
	read_all_config_reg
*****************************************************************************/
int read_all_config_reg(void)
{
	int ret;
	
	mutex_lock(&the_sub->io_lock);
	
	if( the_sub->control_mode != SUB_NORMAL_MODE){
		ret = -ECANCELED;
		goto err;
	}
	
	ret = sub_main_send_cmd(ALL_CONFIG_READ_CMD, 0, NULL);
	if(ret)
		goto err;
	
	ret = sub_main_read_response( (u8 *)&the_sub->chg_ic_config_reg, 
								ALL_CONFIG_READ_CMD, sizeof(CHG_IC_CONFIG_REG) );
	
	if(ret)
		goto err;

err:
	mutex_unlock(&the_sub->io_lock);
	
	return ret;
	
}

static void store_pre_sub_cpu_status(void)
{
	the_sub->pre_chg_ic_status_reg.a = the_sub->chg_ic_status_reg.a;
	the_sub->pre_chg_ic_status_reg.b = the_sub->chg_ic_status_reg.b;
	the_sub->pre_chg_ic_status_reg.c = the_sub->chg_ic_status_reg.c;
	the_sub->pre_chg_ic_status_reg.d = the_sub->chg_ic_status_reg.d;
	the_sub->pre_chg_ic_status_reg.e = the_sub->chg_ic_status_reg.e;
	the_sub->pre_chg_ic_status_reg.f = the_sub->chg_ic_status_reg.f;
	the_sub->pre_chg_ic_status_reg.g = the_sub->chg_ic_status_reg.g;
	the_sub->pre_chg_ic_status_reg.h = the_sub->chg_ic_status_reg.h;
	the_sub->pre_chg_ic_status_reg.ext_a = the_sub->chg_ic_status_reg.ext_a;
	the_sub->pre_chg_ic_status_reg.ext_b = the_sub->chg_ic_status_reg.ext_b;
	the_sub->pre_chg_ic_status_reg.ext_c = the_sub->chg_ic_status_reg.ext_c;
}

/*****************************************************************************
	read_all_status_reg
*****************************************************************************/
int read_all_status_reg(void)
{
	int ret;
	
	mutex_lock(&the_sub->io_lock);
	
	if( the_sub->control_mode != SUB_NORMAL_MODE){
		ret = -ECANCELED;
		goto err;
	}
	
	store_pre_sub_cpu_status();
	
	ret = sub_main_send_cmd(ALL_STATUS_READ_CMD, 0, NULL);
	if(ret)
		goto err;
	
	ret = sub_main_read_response( (u8 *)&the_sub->chg_ic_status_reg, 
								ALL_STATUS_READ_CMD, sizeof(CHG_IC_STATUS_REG) );
	
	if(ret)
		goto err;

err:
	mutex_unlock(&the_sub->io_lock);
	
	return ret;
	
}

/*****************************************************************************
	update_sub_reg
*****************************************************************************/
int update_sub_reg(void)
{
	int ret;
	
	mutex_lock(&the_sub->io_lock);
	
	if( the_sub->control_mode != SUB_NORMAL_MODE){
		ret = -ECANCELED;
		goto err;
	}
	
	ret = sub_main_send_cmd(UPDATE_REG_CMD, 0, NULL);
	if(ret)
		goto err;
	
	ret = sub_main_read_response( NULL, UPDATE_REG_CMD, 0 );
	
	if(ret)
		goto err;

err:
	mutex_unlock(&the_sub->io_lock);
	
	return ret;
	
}


/*****************************************************************************
	char_to_hex
*****************************************************************************/
unsigned char char_to_hex(unsigned char byte_data)
{
	if( ('0' <= byte_data) && (byte_data <= '9')){
		byte_data = byte_data - '0';
	}else if(('a' <= byte_data) && (byte_data <= 'f')){
		byte_data = byte_data - 'a' + 0x0A;
	}else if(('A' <= byte_data) && (byte_data <= 'F')){
		byte_data = byte_data - 'A'+ 0x0A;
	}
	return byte_data;
}

/*****************************************************************************
	set_write_sub_cpu_reg
*****************************************************************************/
static ssize_t set_write_sub_cpu_reg(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	u8 reg_address, write_data;
	
	//int status = count;
	if(count != 6){
		printk("input format error!!\n");
		goto err;
	}
	
	reg_address = (char_to_hex(*buf) << 4) | char_to_hex(*(buf + 1));
	write_data = (char_to_hex(*(buf + 3)) << 4) | char_to_hex(*(buf + 4));
	
	printk("write_reg = 0x%02X\n", reg_address);
	printk("write_data = 0x%02X\n", write_data);
	
	
	if( write_sub_cpu_reg(reg_address, write_data) ){
		printk("Fail!! write_sub_cpu_reg()\n");
		goto err;
	}

err:

	return count;
}


/*****************************************************************************
	set_read_sub_cpu_reg
*****************************************************************************/
static ssize_t set_read_sub_cpu_reg(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	u8 reg_address, read_data;
	
	//int status = count;
	if(count != 3){
		printk("input format error!!\n");
		goto err;
	}
	
	reg_address = (char_to_hex(*buf) << 4) | char_to_hex(*(buf + 1));
	
	printk("reg_address = 0x%02X\n", reg_address);
	
	if( read_sub_cpu_reg(reg_address, &read_data) ){
		printk("Fail!!read_sub_cpu_reg()\n");
		goto err;
	}
	
	printk("read_data = 0x%02X\n", read_data);

err:

	return count;
}

/*****************************************************************************
	show_sub_reg_update
*****************************************************************************/
static ssize_t show_sub_reg_update(struct device *dev,
		  struct device_attribute *attr, char *buf)
{
	update_sub_reg();
	
	return 0;
}


/*****************************************************************************
	show_reg_read
*****************************************************************************/
static ssize_t show_read_all_status_reg(struct device *dev,
		  struct device_attribute *attr, char *buf)
{
	if(read_all_status_reg()){
		printk("Fail!! read_all_status_reg()\n");
		goto err;
	}
	
	printk("Status Register A = %02X\n", the_sub->chg_ic_status_reg.a);
	printk("Status Register B = %02X\n", the_sub->chg_ic_status_reg.b);
	printk("Status Register C = %02X\n", the_sub->chg_ic_status_reg.c);
	printk("Status Register D = %02X\n", the_sub->chg_ic_status_reg.d);
	printk("Status Register E = %02X\n", the_sub->chg_ic_status_reg.e);
	printk("Status Register F = %02X\n", the_sub->chg_ic_status_reg.f);
	printk("Status Register G = %02X\n", the_sub->chg_ic_status_reg.g);
	printk("Status Register H = %02X\n", the_sub->chg_ic_status_reg.h);
	printk("Status Register Ext A = %02X\n", the_sub->chg_ic_status_reg.ext_a);
	printk("Status Register Ext B = %02X\n", the_sub->chg_ic_status_reg.ext_b);
	printk("Status Register Ext C = %02X\n", the_sub->chg_ic_status_reg.ext_c);
	
err:
		
	return 0;
}

/*****************************************************************************
	show_reg_read
*****************************************************************************/
static ssize_t show_read_all_config_reg(struct device *dev,
		  struct device_attribute *attr, char *buf)
{
	if(read_all_config_reg()){
		printk("Fail!! read_all_config_reg()\n");
		goto err;
	}
	
	printk("Charge current = %02X\n", the_sub->chg_ic_config_reg.charge_current);
	printk("Input current limit, OTG/LBR Vout = %02X\n", the_sub->chg_ic_config_reg.input_current_limit);
	printk("Float Voltage = %02X\n", the_sub->chg_ic_config_reg.float_voltage);
	printk("Control Register A = %02X\n", the_sub->chg_ic_config_reg.control_reg_a);
	printk("Control Register B = %02X\n", the_sub->chg_ic_config_reg.control_reg_b);
	printk("Pin Control = %02X\n", the_sub->chg_ic_config_reg.pin_control);
	printk("OTG/LBR Control = %02X\n", the_sub->chg_ic_config_reg.otg_lbr_control);
	printk("FAULT Interrupt Register = %02X\n", the_sub->chg_ic_config_reg.fault_interrupt_reg);
	printk("Cell temperature monitor = %02X\n", the_sub->chg_ic_config_reg.cell_temperature_monitor);
	printk("Safety Timers/Thermal Shutdown =  %02X\n", the_sub->chg_ic_config_reg.safety_timers);
	printk("Vsys = %02X\n", the_sub->chg_ic_config_reg.vsys);
	printk("I2C Bus/Slave Address = %02X\n", the_sub->chg_ic_config_reg.i2c_address);
	printk("IRQ reset = %02X\n", the_sub->chg_ic_config_reg.irq_reset);
	printk("Command Register A = %02X\n", the_sub->chg_ic_config_reg.command_reg_a);
	
err:
		
	return 0;
}

 /*****************************************************************************
	set_control_mode
*****************************************************************************/
static ssize_t set_control_mode(struct device *dev, struct device_attribute *attr,
					  const char *buf, size_t count)
{
	int status = count;
	//struct sub_cpu_bci_device_info *di = dev_get_drvdata(dev);

	if (strncmp(buf, "normal", 6) == 0) 
	{
		the_sub->control_mode = SUB_NORMAL_MODE;
		if(gpio_get_value(SPI_SUB_INT)){
			read_all_status_reg();
		}
		set_watch_dog_timer_2(WDT2_ENABLE, WDT2_TIME);
	} 
	else if (strncmp(buf, "update", 6) == 0) 
	{
		set_watch_dog_timer_2(WDT2_DISABLE, WDT2_TIME);
		
		the_sub->control_mode = SUB_UPDATE_MODE;
		
	} 
	else
	{
		printk("Error!! Can't change the mode.\n");
		return -EINVAL;
	}
	
	return status;
}

/*****************************************************************************
	show_sub_mcu_fw_ver
*****************************************************************************/
static ssize_t show_sub_mcu_fw_ver(struct device *dev,
		  struct device_attribute *attr, char *buf)
{

	int ret = 0;
	u8 sub_ver[SUB_VER_LENGTH];
	
	mutex_lock(&the_sub->io_lock);
	
	ret = sub_main_send_cmd(REQ_MCU_FW_VER_CMD, 0, NULL);
	if(ret)
		goto err;
	
	ret = sub_main_read_response( (u8 *)&sub_ver, 
								REQ_MCU_FW_VER_CMD, SUB_VER_LENGTH );
	
	if(ret)
		goto err;
	
	mutex_unlock(&the_sub->io_lock);
	
	return snprintf(buf, PAGE_SIZE, "The release date : %04d/%02d/%02d\nVersion : %d\n"
		, (sub_ver[1] | (sub_ver[0] << 8)), sub_ver[2], sub_ver[3]
		, (sub_ver[7] | (sub_ver[6] << 8)  | (sub_ver[5] << 16) | (sub_ver[4] << 24)));
	
err:
	
	mutex_unlock(&the_sub->io_lock);
	
	return ret;
	
}

/*****************************************************************************
	show_sub_mcu_fw_ver
*****************************************************************************/
static ssize_t show_sub_upd_pgm_ver(struct device *dev,
		  struct device_attribute *attr, char *buf)
{
	int ret = 0;
	u8 sub_ver[SUB_VER_LENGTH];
	
	mutex_lock(&the_sub->io_lock);
	
	ret = sub_main_send_cmd(REQ_UPD_PGM_VER_CMD, 0, NULL);
	
	if(ret)
		goto err;
	
	ret = sub_main_read_response( (u8 *)&sub_ver, 
								REQ_UPD_PGM_VER_CMD, SUB_VER_LENGTH );
	
	if(ret)
		goto err;
	
	mutex_unlock(&the_sub->io_lock);
	
	return snprintf(buf, PAGE_SIZE, "The release date : %04d/%02d/%02d\nVersion : %d\n"
		, (sub_ver[1] | (sub_ver[0] << 8)), sub_ver[2], sub_ver[3]
		, (sub_ver[7] | (sub_ver[6] << 8)  | (sub_ver[5] << 16) | (sub_ver[4] << 24)));
	
err:
	
	mutex_unlock(&the_sub->io_lock);
	
	return ret;
}

static DEVICE_ATTR(write_sub_cpu_reg, S_IWUSR | S_IRUGO, NULL, set_write_sub_cpu_reg);
static DEVICE_ATTR(read_sub_cpu_reg, S_IWUSR | S_IRUGO, NULL, set_read_sub_cpu_reg);
static DEVICE_ATTR(read_all_status_reg, S_IWUSR | S_IRUGO, show_read_all_status_reg, NULL);
static DEVICE_ATTR(read_all_config_reg, S_IWUSR | S_IRUGO, show_read_all_config_reg, NULL);
static DEVICE_ATTR(sub_reg_update, S_IWUSR | S_IRUGO, show_sub_reg_update, NULL);
static DEVICE_ATTR(control_mode, S_IWUSR | S_IRUGO, NULL, set_control_mode);
static DEVICE_ATTR(sub_mcu_fw_ver, S_IWUSR | S_IRUGO, show_sub_mcu_fw_ver, NULL);
static DEVICE_ATTR(sub_upd_pgm_ver, S_IWUSR | S_IRUGO, show_sub_upd_pgm_ver, NULL);


static struct attribute *sub_main_attributes[] = {
	&dev_attr_write_sub_cpu_reg.attr,
	&dev_attr_read_sub_cpu_reg.attr,
	&dev_attr_read_all_status_reg.attr,
	&dev_attr_read_all_config_reg.attr,
	&dev_attr_sub_reg_update.attr,
	&dev_attr_control_mode.attr,
	&dev_attr_sub_mcu_fw_ver.attr,
	&dev_attr_sub_upd_pgm_ver.attr,
	NULL,
};

static const struct attribute_group sub_main_attr_group = {
	.attrs = sub_main_attributes,
};


 /*****************************************************************************
	print_sub_state
*****************************************************************************/
static void print_sub_state(void)
{
	printk("Status Register A = %02X\n", the_sub->chg_ic_status_reg.a);
	printk("Status Register B = %02X\n", the_sub->chg_ic_status_reg.b);
	printk("Status Register C = %02X\n", the_sub->chg_ic_status_reg.c);
	printk("Status Register D = %02X\n", the_sub->chg_ic_status_reg.d);
	printk("Status Register E = %02X\n", the_sub->chg_ic_status_reg.e);
	printk("Status Register F = %02X\n", the_sub->chg_ic_status_reg.f);
	printk("Status Register G = %02X\n", the_sub->chg_ic_status_reg.g);
	printk("Status Register H = %02X\n", the_sub->chg_ic_status_reg.h);
	printk("Status Register Ext A = %02X\n", the_sub->chg_ic_status_reg.ext_a);
	printk("Status Register Ext B = %02X\n", the_sub->chg_ic_status_reg.ext_b);
	printk("Status Register Ext C = %02X\n", the_sub->chg_ic_status_reg.ext_c);
}

 /*****************************************************************************
	sub_wdt_start
*****************************************************************************/
int sub_wdt_start(void)
{
	queue_work(sub_wdt_queue, &sub_wdt_start_work);
	return 0;
}

 /*****************************************************************************
	sub_wdt_stop
*****************************************************************************/
int sub_wdt_stop(void)
{
	queue_work(sub_wdt_queue, &sub_wdt_stop_work);
	return 0;
}

 /*****************************************************************************
	sub_wdt_cler
*****************************************************************************/
int sub_wdt_clear(void)
{
	queue_work(sub_wdt_queue, &sub_wdt_clear_work);
	return 0;
}

 /*****************************************************************************
	sub_wdt_start_queue
*****************************************************************************/
static void sub_wdt_start_queue(struct work_struct *ignored)
{
	int i = 0, ret;
	
	ret = set_bit_sub_cpu_reg(EXT_REG_A, WDT_START_BIT);
	while( ret && i < 5){
		ret = set_bit_sub_cpu_reg(EXT_REG_A, WDT_START_BIT);
		i++;
	}
}


 /*****************************************************************************
	sub_wdt_stop_queue
*****************************************************************************/
static void sub_wdt_stop_queue(struct work_struct *ignored)
{
	int i = 0, ret;

	ret = clear_bit_sub_cpu_reg(EXT_REG_A, WDT_START_BIT);
	while( ret && i < 5){
		ret = clear_bit_sub_cpu_reg(EXT_REG_A, WDT_START_BIT);
		i++;
	}
}

 /*****************************************************************************
	sub_wdt_clear_queue
*****************************************************************************/
static void sub_wdt_clear_queue(struct work_struct *ignored)
{
	int i = 0, ret;
	
	ret = set_bit_sub_cpu_reg(EXT_REG_A, WDT_CLEAR_BIT);
	while( ret && i < 5){
		ret = set_bit_sub_cpu_reg(EXT_REG_A, WDT_CLEAR_BIT);
		i++;
	}
}

/*****************************************************************************
	confirm_wdt_expired
*****************************************************************************/
static void confirm_wdt_expired(void)
{
	if( the_sub->chg_ic_status_reg.ext_c & WDT_EXPIRE_BIT )
		printk("Sub CPU WDT expired!!!!\n");
}

/*
 * This thread processes interrupts reported by the Primary Interrupt Handler.
 */
 /*****************************************************************************
	sub_main_irq_thread
*****************************************************************************/
extern void sub_cpu_check_battery_state_work(void);
extern void power_key_detect_work(void);

static int sub_main_irq_thread(void *data)
{
	long irq = (long)data;
	int ret;
	int retry_flag;

	while (!kthread_should_stop()) {

cancel:
		
		retry_flag = 0;

		/* Wait for IRQ, then read irq status (also blocking) */
		wait_for_completion_interruptible(&the_sub->irq_event);
		
		wake_lock(&the_sub->sub_irq_wake_lock);
		
retry:
		ret = read_all_status_reg();
		if(ret){
			
			if(ret == -ECANCELED){
				wake_unlock(&the_sub->sub_irq_wake_lock);
				enable_irq(irq);
				goto cancel;
			}else{
				printk("Fail!! read_all_status_reg()\n");
				goto retry;
			}
		}
		
		if( confirm_the_di() ){
		
			//print_sub_state();
			
			confirm_wdt_expired();
		
			//sub_power_state_change_detect();
			sub_cpu_check_battery_state_work();
		
			//sub_power_key_state_change_detect();
			power_key_detect_work();
			
		}
		
		if(gpio_get_value(SPI_SUB_INT)){
			goto retry;
		}
		
		if(!retry_flag){
			enable_irq(irq);
		}

		if(gpio_get_value(SPI_SUB_INT)){
			retry_flag = 1;
			goto retry;
		}
		
		wake_unlock(&the_sub->sub_irq_wake_lock);
		
	}

	return 0;
}

 /*****************************************************************************
	handle_spi_sub_int_irq
*****************************************************************************/
static irqreturn_t handle_spi_sub_int_irq(int irq, void *devid)
{
	//printk("handle_spi_sub_int_irq\n");
	
	wake_lock_timeout(&the_sub->sub_irq_wake_lock, 2 * HZ);
	
	disable_irq_nosync(irq);
	complete(devid);
	return IRQ_HANDLED;
}

 /*****************************************************************************
	acquire_wake_up_wake_lock
*****************************************************************************/
void acquire_wake_up_wake_lock(void)
{
	wake_lock_timeout(&the_sub->wake_up_wake_lock, 2 * HZ);
}
EXPORT_SYMBOL(acquire_wake_up_wake_lock);

/*****************************************************************************
	sub_main_init_irq
*****************************************************************************/
int sub_main_init_irq(int irq_num, struct sub_main *sub_main_module)
//int sub_main_init_irq(int irq_num, unsigned irq_base, unsigned irq_end)
{

	int	status = 0;
	struct task_struct	*task;
	
	task = kthread_run(sub_main_irq_thread, (void *)irq_num, "sub_int_irq");
	
	if (IS_ERR(task)) {
		pr_err("sub-main: could not create irq %d thread!\n", irq_num);
		status = PTR_ERR(task);
		goto fail_kthread;
	}
	
	status = request_irq(irq_num, handle_spi_sub_int_irq, IRQF_DISABLED | IRQF_TRIGGER_RISING,
				"SPI_SUB_INT_IRQ", &sub_main_module->irq_event);

	if (status < 0) {
		pr_err("Sub-Main: could not claim irq%d: %d\n", irq_num, status);
		goto fail_irq;
	}
	
	enable_irq_wake(irq_num);
	
	return status;
fail_irq:
	free_irq(irq_num, &sub_main_module->irq_event);

fail_kthread:
//	for (i = irq_base; i < irq_end; i++)
//		set_irq_chip_and_handler(i, NULL, NULL);
	return status;
}

/*****************************************************************************
	sub_main_device_init
*****************************************************************************/
static int sub_main_device_init(struct sub_main *sub_main, int irq)
{
	wake_lock_init(&sub_main->sub_irq_wake_lock, 
					WAKE_LOCK_SUSPEND, "sub_irq_wake_lock");
	wake_lock_init(&sub_main->wake_up_wake_lock, 
					WAKE_LOCK_SUSPEND, "wake_up_wake_lock");
	mutex_init(&sub_main->io_lock);
	init_completion(&sub_main->irq_event);
	dev_set_drvdata(sub_main->dev, sub_main);

	return 0;
}


/*****************************************************************************
	sub_cpu_open
*****************************************************************************/
static int sub_cpu_open( struct inode *inode, struct file *file )
{
	return 0;
}

/*****************************************************************************
	sub_cpu_release
*****************************************************************************/
static int sub_cpu_release( struct inode *inode, struct file *file )
{
	return 0;
}

/*****************************************************************************
	sub_cpu_read
*****************************************************************************/
static ssize_t sub_cpu_read(struct file *filp, char __user *data, size_t count, loff_t *ppos)
{

	u8 rbuf[READ_BUFFER_LENGTH], i = 0;
	u8 retry_cnt = 0;
	
	mutex_lock(&the_sub->io_lock);
	
	if( the_sub->control_mode != SUB_UPDATE_MODE){
		goto err;
	}
	
	if(count >= READ_BUFFER_LENGTH)
		goto err;
	
	while( i < count ){
		while( sub_main_spi_byte_read(&rbuf[i]) && (retry_cnt < 3))
		{
			retry_cnt++;
			mdelay(1000);
		}
		i++;
	}
	
	if(copy_to_user(data, rbuf, count))
		goto err;
	
	mutex_unlock(&the_sub->io_lock);
	
	return count;
	
err:
	mutex_unlock(&the_sub->io_lock);
	return -EFAULT;
}

/*****************************************************************************
	sub_cpu_write
*****************************************************************************/
static ssize_t sub_cpu_write(struct file *filp, const char __user *data, size_t count, loff_t *ppos)
{
	u8 wbuf[SEND_BUFFER_LENGTH], i = 0;
	u8 retry_cnt = 0;
	
	mutex_lock(&the_sub->io_lock);
	
	if( the_sub->control_mode != SUB_UPDATE_MODE){
		goto err;
	}
	
	if( count >= SEND_BUFFER_LENGTH ){
		goto err;
	}
	
	if(copy_from_user(wbuf, data, count))
		goto err;
	
	while( i < count ){
		while( sub_main_spi_byte_write(&wbuf[i]) && (retry_cnt < 3))
		{
			retry_cnt++;
			mdelay(1000);
		}			
		i++;
	}
	
	mutex_unlock(&the_sub->io_lock);
	
	return count;
	
err:
	mutex_unlock(&the_sub->io_lock);
	return -EFAULT;
	
}

const struct file_operations sub_cpu_fops = {
	.owner   = THIS_MODULE,
	.open    = sub_cpu_open,
	.release = sub_cpu_release,
	.read   = sub_cpu_read,
	.write  = sub_cpu_write,
};

static struct miscdevice sub_cpu_miscdev = {
	.minor = MISC_DYNAMIC_MINOR,	/* dynamic allocation */
	.name = "sub_cpu",			/* /dev/sub_cpu */
	.fops = &sub_cpu_fops
};

/*****************************************************************************
	sub_main_spi_remove
*****************************************************************************/
static int __devexit sub_main_spi_remove(struct spi_device *spi)
{
	//struct sub_main *sub_main = dev_get_drvdata(&spi->dev);
	
	destroy_workqueue(sub_wdt_queue);
	
	sysfs_remove_group(&spi->dev.kobj, &sub_main_attr_group);
	
	misc_deregister( &sub_cpu_miscdev );

	//sub_main_device_exit(sub_main);

	return 0;
}

static struct device *
add_numbered_child(struct spi_device *spi, const char *name, int num,
		void *pdata, unsigned pdata_len,
		bool can_wakeup, int irq0, int irq1)
{
	struct platform_device	*pdev;
	int			status;

	pdev = platform_device_alloc(name, num);
	if (!pdev) {
		dev_dbg(&spi->dev, "can't alloc dev\n");
		status = -ENOMEM;
		goto err;
	}

	device_init_wakeup(&pdev->dev, can_wakeup);
	pdev->dev.parent = &spi->dev;

	if (pdata) {
		status = platform_device_add_data(pdev, pdata, pdata_len);
		if (status < 0) {
			dev_dbg(&pdev->dev, "can't add platform_data\n");
			goto err;
		}
	}

	if (irq0) {
		struct resource r[2] = {
			{ .start = irq0, .flags = IORESOURCE_IRQ, },
			{ .start = irq1, .flags = IORESOURCE_IRQ, },
		};

		status = platform_device_add_resources(pdev, r, irq1 ? 2 : 1);
		if (status < 0) {
			dev_dbg(&pdev->dev, "can't add irqs\n");
			goto err;
		}
	}

	status = platform_device_add(pdev);

err:
	if (status < 0) {
		platform_device_put(pdev);
		dev_err(&spi->dev, "can't add %s dev\n", name);
		return ERR_PTR(status);
	}
	return &pdev->dev;
}

/*****************************************************************************
	set_watch_dog_timer_2
*****************************************************************************/
static int set_watch_dog_timer_2(u8 enable, u8 wdt_time)
{
	u8 write_data;
	
	if( comfirm_sub_mcu_fw_ver() < 26){
		printk("This sub CPU version doesn't support WDT2 function.\n");
		return -EPERM;
	}
	
	if(enable)
		write_data = WDT2_ENABLE_BIT | wdt_time;
	else
		write_data = wdt_time & ~WDT2_ENABLE_BIT;
	
	 return write_sub_cpu_reg(EXT_D_REG_ADDRS, write_data);
}

/*****************************************************************************
	sub_main_spi_probe
*****************************************************************************/
static int __devinit sub_main_spi_probe(struct spi_device *spi)
{
	int ret;
	struct sub_main *sub_main_module;
	struct sub_main_platform_data *pdata = spi->dev.platform_data;
	
	sub_main_module = kzalloc(sizeof(struct sub_main), GFP_KERNEL);
	if (sub_main_module == NULL)
		return -ENOMEM;
	
	the_sub = sub_main_module;
	the_sub->control_mode = SUB_NORMAL_MODE;

	spi->bits_per_word = 8;
	spi->mode = SPI_MODE_0;
	spi_setup(spi);
	
	dev_set_drvdata(&spi->dev, sub_main_module);
	sub_main_module->dev = &spi->dev;
	sub_main_module->control_data = spi;
	
	sub_main_device_init(sub_main_module, spi->irq);
	
	ret = sub_main_init_irq(spi->irq, sub_main_module);
	if (ret < 0)
			goto fail;
	
	
	get_sub_mcu_fw_ver();
	
	set_watch_dog_timer_2(WDT2_ENABLE, WDT2_TIME);
	
	while( read_all_status_reg() ){
		printk("read_all_status_reg Error!!!!!\nRetry read_all_status_reg().\n");
	}		
	
	add_numbered_child(spi, "sub_cpu_pwrbutton", -1, 
						NULL, 0, false, 0, 0);
	
	add_numbered_child(spi, "sub_cpu_bci", -1, 
						pdata->bci, sizeof(*pdata->bci), false, 0, 0);
	
	ret = sysfs_create_group(&spi->dev.kobj, &sub_main_attr_group);
	if (ret)
		dev_dbg(&spi->dev, "could not create sysfs files\n");
	
	ret = misc_register( &sub_cpu_miscdev );
	if (ret)
		goto fail;
	
	sub_wdt_queue = create_singlethread_workqueue("sub_wdt_queue");
	if (sub_wdt_queue == NULL) {
		printk(KERN_ERR "Coulndn't create Sub WDT queue\n");
		return -ENOMEM;
	}

fail:
	if (ret < 0){
		printk("\nFail!! sub_main_spi_probe\n");
		sub_main_spi_remove(spi);
	}
	
	return ret;
}

/*****************************************************************************
	confirm_the_sub
*****************************************************************************/
struct sub_main *confirm_the_sub(void)
{
	return the_sub;
}
EXPORT_SYMBOL(confirm_the_sub);

/*****************************************************************************
	sub_main_spi_early_suspend
*****************************************************************************/
static void sub_main_spi_early_suspend(struct early_suspend *h)
{
	if (h->pm_mode == EARLY_SUSPEND_MODE_NORMAL)
		set_sub_standby_flag(1);
}

/*****************************************************************************
	sub_main_spi_early_suspend
*****************************************************************************/
static void sub_main_spi_late_resume(struct early_suspend *h)
{
	if (h->pm_mode == EARLY_SUSPEND_MODE_NORMAL)
		set_sub_standby_flag(0);
}


static struct early_suspend sub_main_spi_earlysuspend = {
	.level = EARLY_SUSPEND_LEVEL_DISABLE_FB,
	.suspend = sub_main_spi_early_suspend,
	.resume = sub_main_spi_late_resume,
};


 /*****************************************************************************
	sub_main_spi_suspend
*****************************************************************************/
static int sub_main_spi_suspend(struct spi_device *spi, pm_message_t m)
{
	//struct sub_main *sub_main = dev_get_drvdata(&spi->dev);
	//return sub_main_device_suspend(sub_main);
	
	flush_workqueue(sub_wdt_queue);
	
	sub_cpu_preparing_for_standby();
	
	return 0;
}

 /*****************************************************************************
	sub_main_spi_resume
*****************************************************************************/
static int sub_main_spi_resume(struct spi_device *spi)
{

	sub_cpu_preparing_for_resume();
	
	return 0;
}

static struct spi_driver sub_main_spi_driver = {
	.driver = {
		.name	= "sub_main",
		.bus	= &spi_bus_type,
		.owner	= THIS_MODULE,
	},
	.probe		= sub_main_spi_probe,
	.remove		= __devexit_p(sub_main_spi_remove),
	.suspend	= sub_main_spi_suspend,
	.resume		= sub_main_spi_resume,
};

static int __init sub_main_spi_init(void)
{
	int ret;

	ret = spi_register_driver(&sub_main_spi_driver);
	if (ret != 0)
		pr_err("Failed to register Sub-Main SPI driver: %d\n", ret);

	register_early_suspend(&sub_main_spi_earlysuspend);
	
	return 0;
}
subsys_initcall(sub_main_spi_init);

static void __exit sub_main_spi_exit(void)
{
	wake_lock_destroy(&the_sub->sub_irq_wake_lock);
	wake_lock_destroy(&the_sub->wake_up_wake_lock);
	
	spi_unregister_driver(&sub_main_spi_driver);

	unregister_early_suspend(&sub_main_spi_earlysuspend);
	
	if(ldo8){
		regulator_put(ldo8);	
	}
	
}
module_exit(sub_main_spi_exit);

MODULE_DESCRIPTION("SPI support for Reader FY11 Sub CPU");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("xxxx");
