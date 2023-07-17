#include <linux/kernel.h>  
#include <linux/module.h>  
#include <linux/fs.h>  
#include <linux/slab.h>  
#include <linux/init.h>  
#include <linux/list.h>  
#include <linux/i2c.h>  
#include <linux/i2c-dev.h>  
#include <linux/input.h>
#include <linux/jiffies.h>  
#include <asm/uaccess.h>  
#include <linux/delay.h>  
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/of_gpio.h>
#include <linux/input/mt.h>
#include <linux/mutex.h>


#define DEBUG 1  
#ifdef DEBUG  
#define dbg(x...) printk(x)  
#else   
#define dbg(x...) (void)(0)  
#endif  

#ifdef CONFIG_ARCH_SUNXI //[
#include <linux/sunxi-gpio.h>
#define USE_IN_SUNXI	1

#include  "../../usb/sunxi_usb/manager/usb_msg_center.h"
#include  "../../usb/sunxi_usb/manager/usbc_platform.h"
#include  "../../usb/sunxi_usb/include/sunxi_usb_board.h"
#endif

#define I2C_MAJOR 89  

enum USB_ROLE
{
	USB_DEFAULT=0,
	USB_DEVICE =1,
	USB_HOST =2,
};

extern int thread_stopped_flag;		// For USBC

static const char DEVICE_NAME[]	= "P15USB30216C";
static struct class *my_dev_class;
static struct i2c_driver my_i2c_driver;  
static int g_AutoDetect = 1 ;				// 
static int g_USB_role = USB_DEFAULT ;	
static int g_USB_plug = 0 ;					// 0: unplug , 1:plug
struct mutex i_Role_mutex;
int g_usb_id_gpio_port = -1 ;
int g_0a5_fault_irq = 0;

char gi2c_read_buf[4]={0x00,0x00,0x00,0x00};
char gi2c_write_buf[2]={0x00,0x00};
static struct delayed_work P15USB30216C_delay_work;
#define P15USB30216C_NOT_EXIST						(-1)
#define P15USB30216C_CHARGER_NONE					0
#define P15USB30216C_CHARGER_DEF_CURRENT	1
#define P15USB30216C_CHARGER_MED_CURRENT	2
#define P15USB30216C_CHARGER_HIGH_CURRENT	3
static volatile int giChargerType = P15USB30216C_NOT_EXIST;
static volatile unsigned long gulJiffies_Detecting ; 
static struct P15USB30216C_data {
	int intr_gpio;					//
	int use_irq;					//
	int twi_id;						//
	int twi_addr;					// i2c addr
	int host_power_on;				// 1: enable Host power on (OTG_5V) , 0: disable Host power on
	int irq;
#ifdef USE_IN_SUNXI
	struct gpio_config irq_gpio;
	struct gpio_config otg_5v_gpio;
	struct gpio_config ovp_gpio;
	struct gpio_config _0a5_fault;
#endif
	struct i2c_client *client;
	struct input_dev *input;
	int is_hw_ready;
	int is_hw_enabled;// enabled in dts or sys_config , status=okay . 
} P15USB30216C_Board_data;

#ifdef USE_IN_SUNXI
/*******************************************************
Function: Set USB as Device Role (Kernel)

Noted: Copy from driver/usb/sunxi_usb/manager/usbc_platform.c
********************************************************/
static void usb_device_chose(void)
{
	thread_run_flag = 0;

	while (!thread_stopped_flag) {
		printk("%s() waiting for usb scanning thread stop ...\n",__func__);
		msleep(1000);
	}

	hw_rmmod_usb_host();
	hw_rmmod_usb_device();
	usb_msg_center(&g_usb_cfg);

	hw_insmod_usb_device();
	usb_msg_center(&g_usb_cfg);

}

/*******************************************************
Function: Set USB as Host Role (Kernel)

Noted: Copy from driver/usb/sunxi_usb/manager/usbc_platform.c
********************************************************/
static void usb_host_chose(void)
{
	thread_run_flag = 0;

	while (!thread_stopped_flag) {
		printk("%s() waiting for usb scanning thread stop ...\n",__func__);
		msleep(1000);
	}
	//g_usb_cfg.port.port_type = USB_PORT_TYPE_HOST;

	hw_rmmod_usb_host();
	hw_rmmod_usb_device();
	usb_msg_center(&g_usb_cfg);

	hw_insmod_usb_host();
	usb_msg_center(&g_usb_cfg);

}

/*******************************************************
Function: Set USB as Null Role

Noted: Copy from driver/usb/sunxi_usb/manager/usbc_platform.c
********************************************************/
static void usb_null_chose(void)
{
	thread_run_flag = 0;

	while (!thread_stopped_flag) {
		printk("%s() waiting for usb scanning thread stop ...\n",__func__);
		msleep(1000);
	}
	hw_rmmod_usb_host();
	hw_rmmod_usb_device();
	usb_msg_center(&g_usb_cfg);

}

static void set_USB(int role)
{
	mutex_lock(&i_Role_mutex);
	if( role == USB_DEVICE)
	{
		if ( 0 == ((gi2c_read_buf[3] & 0x1c) >> 2 ) )	// Not Detect device
		{
			if(P15USB30216C_Board_data.host_power_on){
				printk(KERN_ERR"[P15USB30216C] SET OTG_5V / OPEN OVP  0 \n");
				if(gpio_is_valid (P15USB30216C_Board_data.otg_5v_gpio.gpio ))
					gpio_direction_output(P15USB30216C_Board_data.otg_5v_gpio.gpio, 0);
				else
					printk(KERN_INFO"[P15USB30216C] OTG_5V is not set !!");
				if(gpio_is_valid (P15USB30216C_Board_data.ovp_gpio.gpio ))
					gpio_direction_output(P15USB30216C_Board_data.ovp_gpio.gpio, 0);
				else
					printk(KERN_INFO"[P15USB30216C] OVP_EN is not set !!");
				g_USB_role = USB_DEVICE;
				usb_device_chose();
			}
			else
				printk(KERN_ERR"[P15USB30216C] P15USB_HOST_POWER is 0\n");
		}
		else
			printk(KERN_ERR"P15USB30216C can't detect host , so can't set DEVICE mode\n");
	}
	else if (role == USB_HOST){
		if ( 1 == ((gi2c_read_buf[3] & 0x1c) >> 2 ) )	// Detect device
		{
			if(P15USB30216C_Board_data.host_power_on){
				printk(KERN_ERR"[P15USB30216C] SET OVP / OPEN OTG_5V  1 \n");
				if( gpio_is_valid (P15USB30216C_Board_data.ovp_gpio.gpio ))
					gpio_direction_output(P15USB30216C_Board_data.ovp_gpio.gpio, 1);
				else
					printk(KERN_INFO"[P15USB30216C] OVP_EN is not set !!");
				if( gpio_is_valid (P15USB30216C_Board_data.otg_5v_gpio.gpio ))
					gpio_direction_output(P15USB30216C_Board_data.otg_5v_gpio.gpio, 1);
				else
					printk(KERN_INFO"[P15USB30216C] OTG_5V is not set !!");
				g_USB_role = USB_HOST;
				usb_host_chose();
			}
			else
				printk(KERN_ERR"[P15USB30216C] P15USB_HOST_POWER is 0\n");
		}
		else
			printk(KERN_ERR"P15USB30216C can't detect device , so can't set HOST mode\n");
	}
	mutex_unlock(&i_Role_mutex);
}
#endif

static int P15USB30216C_read_reg(void)
{
	int val , res = 0;
	struct input_dev *input = P15USB30216C_Board_data.input;
	int iChargerType = P15USB30216C_CHARGER_NONE;

	res = i2c_master_recv(P15USB30216C_Board_data.client, gi2c_read_buf, 4);
	//printk(KERN_ERR"[P15USB30216C] reg %x,%x,%x,%x\n",gi2c_read_buf[0],gi2c_read_buf[1],gi2c_read_buf[2],gi2c_read_buf[3]);

	if( g_AutoDetect == 1)
	{
		mutex_lock(&i_Role_mutex);
		if ( (1 == ((gi2c_read_buf[3] & 0x1c) >> 2 ))  )	// Detect device 
		{
			if(P15USB30216C_Board_data.host_power_on){
#ifdef USE_IN_SUNXI
				printk(KERN_ERR"[P15USB30216C] SET OVP / OPEN OTG_5V  1 \n");
				if( gpio_is_valid (P15USB30216C_Board_data.ovp_gpio.gpio ))
					gpio_direction_output(P15USB30216C_Board_data.ovp_gpio.gpio, 1);
				else
					printk(KERN_INFO"[P15USB30216C] OVP_EN is not set !!");
				if( gpio_is_valid (P15USB30216C_Board_data.otg_5v_gpio.gpio) )
					gpio_direction_output(P15USB30216C_Board_data.otg_5v_gpio.gpio, 1);
				else
#endif
					printk(KERN_INFO"[P15USB30216C] OTG_5V is not set !!");

#ifdef USE_IN_SUNXI
				if(g_usb_cfg.port.port_type == 2)
				{
					if( g_USB_role != USB_HOST)
					{
						g_USB_role = USB_HOST;
						usb_host_chose();
					}
				}
#endif
			}
			else
				printk(KERN_ERR"[P15USB30216C] P15USB_HOST_POWER is 0\n");
		}
		else
		{
			if(P15USB30216C_Board_data.host_power_on){
				printk(KERN_ERR"[P15USB30216C] SET OTG_5V / OPEN OVP  0 \n");
#ifdef USE_IN_SUNXI
				if( gpio_is_valid(P15USB30216C_Board_data.otg_5v_gpio.gpio ) )
					gpio_direction_output(P15USB30216C_Board_data.otg_5v_gpio.gpio, 0);
				else
					printk(KERN_INFO"[P15USB30216C] OTG_5V is not set !!");
				if( gpio_is_valid(P15USB30216C_Board_data.ovp_gpio.gpio ))
					gpio_direction_output(P15USB30216C_Board_data.ovp_gpio.gpio, 0);
				else
#endif
					printk(KERN_INFO"[P15USB30216C] OVP_EN is not set !!");

#ifdef USE_IN_SUNXI
				if(g_usb_cfg.port.port_type == 2)
				{	
					if (g_USB_role != USB_DEVICE)
					{
						g_USB_role = USB_DEVICE;

						usb_device_chose();
					}
				}
#endif
			}
			else
				printk(KERN_ERR"[P15USB30216C] P15USB_HOST_POWER is 0\n");
		}
			mutex_unlock(&i_Role_mutex);
	}

	if( (gi2c_read_buf[1] >>7) ==0)
		printk(KERN_INFO" Enable/Active state \n");
	else
		printk(KERN_INFO" Disable and low power state \n");

	if( ((gi2c_read_buf[1] >>6) & 1) ==0)
		printk(KERN_INFO" Enable Try.SRC supported \n");
	else
		printk(KERN_INFO" Enable Try.SNK supported \n");

	if( (gi2c_read_buf[1] &  0x20) ==0)
		printk(KERN_INFO" Disable Accessory Detection \n");
	else
		printk(KERN_INFO" Enable Accessory Detection \n");	


	val = (gi2c_read_buf[1] >>3 ) & 0x3 ;
	switch(val)
	{
		case 0: printk(KERN_INFO" Charging control : Default current mode \n");
			break;
		case 1: printk(KERN_INFO" Charging control : Medium current mode (1.5A) \n");
			break;
		case 2: printk(KERN_INFO" Charging control : High Current mode \n");
			break;
	}

	val = (gi2c_read_buf[1] & 6 ) >> 1 ;
	switch(val)
	{
		case 0: printk(KERN_INFO" Device (SNK) \n");break;
		case 1: printk(KERN_INFO" Host (SRC) \n");break;
		case 2: printk(KERN_INFO" Dual Role (DRP) \n");break;
		case 3: printk(KERN_INFO" Dual Role 2(DRP) where Try.SRC or Try.SNK\n");break;
	}

	if( (gi2c_read_buf[3] >>7) ==0) {
		iChargerType = P15USB30216C_CHARGER_NONE;
		printk(KERN_INFO" VBUS not detected \n");
	}
	else {
		printk(KERN_INFO" VBUS detected \n");
	}

	val = (gi2c_read_buf[3] >>5 ) & 0x3 ;
	switch(val)
	{
		case 1: printk(KERN_INFO" Charging : Default current mode \n");
			iChargerType = P15USB30216C_CHARGER_DEF_CURRENT;
			break;
		case 2: printk(KERN_INFO" Charging : Medium current mode (1.5A) \n");
			iChargerType = P15USB30216C_CHARGER_MED_CURRENT;
			break;
		case 3: printk(KERN_INFO" Charging : High Current mode \n");
			iChargerType = P15USB30216C_CHARGER_HIGH_CURRENT;
			break;
		default:
			printk(KERN_INFO" Charging :Standby \n");
	}


	val = (gi2c_read_buf[3] & 0x1c) >> 2 ;
	switch(val)
	{
		case 1: printk(KERN_ERR" Attatched Port : Device \n");
				g_USB_plug=1;
				//input_mt_slot(input, 1);
				//input_mt_report_slot_state(input, MT_TOOL_FINGER, true);
				//input_report_abs(input, ABS_MT_POSITION_X, 255);
				input_report_switch(input,SW_DOCK,1);
				input_sync(input);break;
		case 2: printk(KERN_ERR" Attatched Port : Host \n");
				g_USB_plug=1;
				input_report_switch(input,SW_DOCK,1);
				input_sync(input);break;
				break;
		case 3: printk(KERN_ERR" Attatched Port : Audio Adapter Accessory \n");
				g_USB_plug=1;
				input_report_switch(input,SW_DOCK,1);
				input_sync(input);break;
				break;
		case 4: printk(KERN_ERR" Attatched Port : Debug Accessory \n");
				g_USB_plug=1;
				input_report_switch(input,SW_DOCK,1);
				input_sync(input);break;
				break;
		case 5: printk(KERN_ERR" Attatched Port : Device with Active Cable \n");
				g_USB_plug=1;
				input_report_switch(input,SW_DOCK,1);
				input_sync(input);break;
				break;
		default:
			printk(KERN_ERR" Attatched Port : Standby \n");
			input_report_switch(input,SW_DOCK,0);
			g_USB_plug = 0;
			input_sync(input);
	}

	val = gi2c_read_buf[3] & 0x03 ;
	switch(val)
	{
		case 1: printk(KERN_INFO" CC1 makes connection \n");break;
		case 2: printk(KERN_INFO" CC2 makes connection \n");break;
		case 3: printk(KERN_INFO" Undetermined \n");break;
		default:
			printk(KERN_INFO" Standby \n");
	}
	return iChargerType;
}


static void P15USB30216C_work_func(struct work_struct *work)
{

}

static irqreturn_t P15USB30216C_interrupt(int irq, void *dev_id)
{
	schedule_delayed_work(&P15USB30216C_delay_work, 0);
	return IRQ_HANDLED;
}

static irqreturn_t P15USB30216C_irq_handler(int irq, void *dev)
{
	gulJiffies_Detecting = jiffies + msecs_to_jiffies(100);
	giChargerType = P15USB30216C_read_reg();
	return IRQ_HANDLED;
}


static irqreturn_t P15USB30216C_05a_fault_irq_handler(int irq, void *dev)
{
#ifdef USE_IN_SUNXI
	printk(KERN_ERR"[P15USB30216C] (%s) SET OTG_5V / OPEN OVP  0 \n",__FUNCTION__);
	if( gpio_is_valid (P15USB30216C_Board_data.otg_5v_gpio.gpio ))
		gpio_direction_output(P15USB30216C_Board_data.otg_5v_gpio.gpio, 0);
	else
		printk(KERN_INFO"[P15USB30216C] OTG_5V is not set !!");
	if( gpio_is_valid (P15USB30216C_Board_data.ovp_gpio.gpio ))
		gpio_direction_output(P15USB30216C_Board_data.ovp_gpio.gpio, 0);
	else
#endif
		printk(KERN_INFO"[P15USB30216C] OVP_EN is not set !!");
	return IRQ_HANDLED;
}

/*******************************************************
Function:
Noted: 
********************************************************/
static ssize_t get_usb_plug(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n",g_USB_plug);
}

/*******************************************************
Function:
Noted: 
********************************************************/
static ssize_t get_role_usb(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n",g_USB_role);
}

/*******************************************************
Function: 
Noted:
********************************************************/
static ssize_t set_role_usb(struct device *dev,
				struct device_attribute *attr,const char *buf, size_t len)
{
	int value ;
	char *pHead = NULL;

	if(g_AutoDetect==1)
	{
		printk(KERN_ERR" Can't set USB role , USB role is auto switch  now !! \n");
		return len;
	}

	if( pHead = strstr(buf,"0x") )
		value = simple_strtol(buf, NULL, 16);
	else
		value = simple_strtoul(buf, NULL, 10);

#ifdef USE_IN_SUNXI
	if(value == USB_HOST)
		set_USB(value);
	else if(value == USB_DEVICE)
		set_USB(value);
	else
#endif
		printk(KERN_ERR"[P15USB30216C] set set_usb_role failed , value:%d !!!\n",value);

	return len;
}


/*******************************************************
Function:
Noted: 
********************************************************/
static ssize_t get_auto_status(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	
	if(g_AutoDetect==1)
		dbg(KERN_ERR" USB role auto switch !! \n");
	else if(g_AutoDetect==0)
		dbg(KERN_ERR" Close USB role auto switch !! \n");
		
	return sprintf(buf, "%d\n",g_AutoDetect);
}

/*******************************************************
Function: 
Noted:
********************************************************/
static ssize_t set_auto_status(struct device *dev,
				struct device_attribute *attr,const char *buf, size_t len)
{
	int value ,res;
	char *pHead = NULL;

	if( pHead = strstr(buf,"0x") )
		value = simple_strtol(buf, NULL, 16);
	else
		value = simple_strtoul(buf, NULL, 10);

	if(value==1){
		g_AutoDetect = 1 ;
		printk(KERN_ERR" USB role auto switch !! \n");
	}
	else if(value==0){
		g_AutoDetect = 0 ;
		printk(KERN_ERR" Close USB role auto switch !! \n");
	}
	else
		printk(KERN_ERR"[P15USB30216C] set auto_status failed , value:%d !!!\n",value);

	return len;
}

/*******************************************************
Function:
Noted: P15USB30216C must be read sequentially
********************************************************/
static ssize_t get_OVP_EN(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int val=0;
#ifdef USE_IN_SUNXI
	val = gpio_get_value(P15USB30216C_Board_data.ovp_gpio.gpio);
#endif

	return sprintf(buf, "%d\n",val);
}

/*******************************************************
Function: 
Noted:
********************************************************/
static ssize_t set_OVP_EN(struct device *dev,
				struct device_attribute *attr,const char *buf, size_t len)
{
	int value ;
	char *pHead = NULL;

	if( pHead = strstr(buf,"0x") )
		value = simple_strtol(buf, NULL, 16);
	else
		value = simple_strtoul(buf, NULL, 10);

#ifdef USE_IN_SUNXI
	if(value==1)
		gpio_direction_output(P15USB30216C_Board_data.ovp_gpio.gpio, 1);
	else if(value==0)
		gpio_direction_output(P15USB30216C_Board_data.ovp_gpio.gpio, 0);
	else
#endif
		printk(KERN_ERR"[P15USB30216C] set OVP_EN failed , value:%d !!!\n",value);

	return len;
}


/*******************************************************
Function:
Noted: P15USB30216C must be read sequentially
********************************************************/
static ssize_t get_OTG_5V_EN(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int val=0;
#ifdef USE_IN_SUNXI
	if(gpio_is_valid (P15USB30216C_Board_data.otg_5v_gpio.gpio ))
		val = gpio_get_value(P15USB30216C_Board_data.otg_5v_gpio.gpio);
	else
#endif
		printk(KERN_INFO"[P15USB30216C] OTG_5V is not set !!");

	return sprintf(buf, "%d\n",val);
}

/*******************************************************
Function: 
Noted:
********************************************************/
static ssize_t set_OTG_5V_EN(struct device *dev,
				struct device_attribute *attr,const char *buf, size_t len)
{
	int value;
	char *pHead = NULL;

	if( pHead = strstr(buf,"0x") )
		value = simple_strtol(buf, NULL, 16);
	else
		value = simple_strtoul(buf, NULL, 10);

#ifdef USE_IN_SUNXI
	if( gpio_is_valid (P15USB30216C_Board_data.otg_5v_gpio.gpio )){
		printk(KERN_ERR"[ERROR] OTG_5V is not set !!");
		return -1;
	}

	if(value==1)
		gpio_direction_output(P15USB30216C_Board_data.otg_5v_gpio.gpio, 1);
	else if(value==0)
		gpio_direction_output(P15USB30216C_Board_data.otg_5v_gpio.gpio, 0);
	else
#endif
		printk(KERN_ERR"[P15USB30216C] set OTG_5V_EN failed , value:%d !!!\n",value);

	return len;
}


/*******************************************************
Function: Read register from the i2c slave device.

Noted: P15USB30216C must be read sequentially
********************************************************/
static ssize_t get_register(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int res ,val=0;

	P15USB30216C_read_reg();

	return sprintf(buf, "\n");
}

/*******************************************************
Function: Set register from the i2c slave device.

Noted:
********************************************************/
static ssize_t set_register(struct device *dev,
				struct device_attribute *attr,const char *buf, size_t len)
{
	int res = 0 , value;
	char *pHead = NULL;

	if( pHead = strstr(buf,"0x") )
		value = simple_strtol(buf, NULL, 16);
	else
		value = simple_strtoul(buf, NULL, 10);

	gi2c_write_buf[0] = 0x20 ;	// Default value
	gi2c_write_buf[1] = value;

	printk(KERN_ERR"[P15USB30216C] set reg :%x \n",value);
	res = i2c_master_send(P15USB30216C_Board_data.client,gi2c_write_buf,2);
	if(res!=2)
		printk(KERN_ERR"[P15USB30216C] set reg failed !!!\n");

	return len;
}
/*******************************************************
Function: Read register from the i2c slave device.

Noted: P15USB30216C must be read sequentially
********************************************************/
static ssize_t get_int_gpio(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int res ,val=-1;
#ifdef USE_IN_SUNXI
	if(P15USB30216C_Board_data.irq_gpio.gpio > 0)
	{
		val = gpio_get_value(P15USB30216C_Board_data.irq_gpio.gpio);
		printk(KERN_ERR"[P15USB] GPIO Val:%d , gpio:%d \n",val,P15USB30216C_Board_data.irq_gpio.gpio);
	} else 
#endif
	if (gpio_is_valid(P15USB30216C_Board_data.intr_gpio)) {
		val = gpio_get_value(P15USB30216C_Board_data.intr_gpio);
		printk(KERN_ERR"[P15USB] GPIO Val:%d , gpio:%d \n",val,P15USB30216C_Board_data.intr_gpio);
	}
	
	return sprintf(buf, "\n");
}

int P15USB30216C_charger_type_get(void)
{
	if(0==P15USB30216C_Board_data.is_hw_ready) {
		return P15USB30216C_NOT_EXIST;
	}

	while(time_is_after_jiffies(gulJiffies_Detecting)) {
		printk(KERN_DEBUG"waiting for cc detecting ... \n");
		msleep(20);
	}
	printk(KERN_DEBUG"%s() : ChargerType=%d\n",__func__,giChargerType);
	return giChargerType;
}
int P15USB30216C_exist(void)
{
	return (!!P15USB30216C_Board_data.is_hw_ready);
}
int P15USB30216C_enabled(void)
{
	return (!!P15USB30216C_Board_data.is_hw_enabled);
}

static DEVICE_ATTR(USB_PLUG, S_IRUGO , get_usb_plug,NULL);
static DEVICE_ATTR(USB_ROLE, S_IRUGO | S_IWUSR, get_role_usb, set_role_usb);
static DEVICE_ATTR(AUTO_ROLE, S_IRUGO | S_IWUSR, get_auto_status, set_auto_status);
static DEVICE_ATTR(OVP_EN, S_IRUGO | S_IWUSR, get_OVP_EN, set_OVP_EN);
static DEVICE_ATTR(OTG_5V_EN, S_IRUGO | S_IWUSR, get_OTG_5V_EN, set_OTG_5V_EN);
static DEVICE_ATTR(register, S_IRUGO | S_IWUSR, get_register, set_register);
static DEVICE_ATTR(int_gpio, S_IRUGO , get_int_gpio, NULL);

static const struct attribute *sysfs_P15USB30216C_attrs[] = {
	&dev_attr_register.attr,
	&dev_attr_int_gpio.attr,
	&dev_attr_OTG_5V_EN.attr,
	&dev_attr_OVP_EN.attr,
	&dev_attr_USB_ROLE.attr,
	&dev_attr_USB_PLUG.attr,
	&dev_attr_AUTO_ROLE.attr,
	NULL,
};


/*******************************************************
Function: Probe

Noted:
********************************************************/
static int P15USB30216C_probe(struct i2c_client *client, const struct i2c_device_id *id)  
{
	int res,rc,err;
	struct device_node *np = client->dev.of_node;

	dbg("probe:name = %s,flag =%d,addr = %d,adapter = %d\n",client->name,  
	client->flags,client->addr,client->adapter->nr );
    
#ifndef USE_IN_SUNXI
	P15USB30216C_Board_data.is_hw_ready = 0;
	if (np) {
		P15USB30216C_Board_data.intr_gpio = of_get_named_gpio(np, "gpio_intr", 0);
	} else
		return -ENODEV;
    
	if (!gpio_is_valid(P15USB30216C_Board_data.intr_gpio))
		return -ENODEV;

	err = devm_gpio_request_one(&client->dev, P15USB30216C_Board_data.intr_gpio,
				GPIOF_IN, "gpio_p15usb_intr");
	if (err < 0) {
		dev_err(&client->dev,
			"request gpio failed: %d\n", err);
		return err;
	}
	client->irq = gpio_to_irq(P15USB30216C_Board_data.intr_gpio);
#endif

	P15USB30216C_Board_data.client = client ;

	P15USB30216C_Board_data.input = input_allocate_device();
	if (P15USB30216C_Board_data.input == NULL) {
		err = -ENOMEM;
		goto out;
	}

	strlcpy(client->name, DEVICE_NAME, I2C_NAME_SIZE);
	P15USB30216C_Board_data.input->name = "P15USB30216C";
	P15USB30216C_Board_data.input->id.bustype = BUS_I2C;
	P15USB30216C_Board_data.input->id.vendor = 0x0416;
	P15USB30216C_Board_data.input->id.product = 0x1001;
	P15USB30216C_Board_data.input->id.version = 10427;
	//input_set_abs_params(P15USB30216C_Board_data.input, ABS_MT_POSITION_X, 0,255, 0, 0);
	//P15USB30216C_Board_data.input->evbit[0] = BIT_MASK(EV_SYN) | BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);

	__set_bit(EV_SW, P15USB30216C_Board_data.input->evbit);
	__set_bit(SW_DOCK, P15USB30216C_Board_data.input->swbit);
	__set_bit(SW_TABLET_MODE, P15USB30216C_Board_data.input->swbit);


	err = input_register_device(P15USB30216C_Board_data.input);
	if (err < 0) {
		printk(KERN_ERR"[%s_%d] input_register_device failed \n",__FUNCTION__,__LINE__);
		goto out;
	}
	err = sysfs_create_files(&P15USB30216C_Board_data.input->dev.kobj, sysfs_P15USB30216C_attrs);
	if (err) {
		pr_debug("[%s_%d] Can't create device file!\n",__FUNCTION__,__LINE__);
		return -ENODEV;
	}

	mutex_init(&i_Role_mutex);

#ifdef USE_IN_SUNXI
	printk(KERN_ERR"[%s_%d] gpio:%d \n",__FUNCTION__,__LINE__,P15USB30216C_Board_data.irq_gpio.gpio);
	if(gpio_is_valid(P15USB30216C_Board_data.irq_gpio.gpio))
	{
		P15USB30216C_Board_data.irq = gpio_to_irq(P15USB30216C_Board_data.irq_gpio.gpio);
		printk(KERN_ERR"[%s_%d] irq:%d \n",__FUNCTION__,__LINE__,P15USB30216C_Board_data.irq);
		
		if (P15USB30216C_Board_data.irq) {
			INIT_DELAYED_WORK(&P15USB30216C_delay_work, P15USB30216C_work_func);
			err = request_threaded_irq(P15USB30216C_Board_data.irq, NULL,P15USB30216C_irq_handler, IRQF_TRIGGER_LOW | IRQF_ONESHOT,DEVICE_NAME, &P15USB30216C_Board_data);
			//err = request_irq(P15USB30216C_Board_data.irq,P15USB30216C__interrupt,IRQF_TRIGGER_RISING | IRQF_ONESHOT,DEVICE_NAME,P15USB30216C_Board_data);
		}
	} else 
#endif
	if (gpio_is_valid(P15USB30216C_Board_data.intr_gpio)) {
		printk(KERN_ERR"[%s_%d] irq:%d \n",__FUNCTION__,__LINE__, client->irq);
		if (client->irq) {
			INIT_DELAYED_WORK(&P15USB30216C_delay_work, P15USB30216C_work_func);
			err = request_threaded_irq(client->irq, NULL,P15USB30216C_irq_handler, IRQF_TRIGGER_LOW | IRQF_ONESHOT,DEVICE_NAME, &P15USB30216C_Board_data);
		}
	} 

#ifdef USE_IN_SUNXI
	printk(KERN_ERR"[%s_%d] _0a5_fault:%d \n",__FUNCTION__,__LINE__,P15USB30216C_Board_data._0a5_fault.gpio);
	if(gpio_is_valid(P15USB30216C_Board_data._0a5_fault.gpio))
	{
		g_0a5_fault_irq = gpio_to_irq(P15USB30216C_Board_data._0a5_fault.gpio);
		printk(KERN_ERR"[%s_%d] 0a5_dfault irq:%d \n",__FUNCTION__,__LINE__,g_0a5_fault_irq);
		
		if (g_0a5_fault_irq) {
			err = request_threaded_irq(g_0a5_fault_irq, NULL,P15USB30216C_05a_fault_irq_handler, IRQF_TRIGGER_LOW | IRQF_ONESHOT,DEVICE_NAME, &P15USB30216C_Board_data);
		}
	}
#endif

	printk(KERN_ERR"[%s_%d] Set Reg: 0x46 \n",__FUNCTION__,__LINE__); // 0x02 , HOST , 0x46 , DRP
	gi2c_write_buf[0] = 0x20 ;	// Default value
	gi2c_write_buf[1] = 0x46;
	res = i2c_master_send(P15USB30216C_Board_data.client,gi2c_write_buf,2);
	if(res!=2)
		printk(KERN_ERR"[P15USB30216C] set reg  0x46 failed !!!\n");

	/*
	dev = device_create(my_dev_class,client->dev,MKDEV(I2C_MAJOR, 0), NULL,  DEVICE_NAME);
	if (IS_ERR(dev))
	{
		dbg("device create error\n");
		goto out;
	}
	*/
	P15USB30216C_Board_data.is_hw_ready = 1;
	gulJiffies_Detecting = jiffies;
	return 0;  
out:
	return -1;  
}

static int  P15USB30216C_remove(struct i2c_client *client)  
{
	dbg("remove\n");  
	return 0;  
}



static const struct i2c_device_id P15USB30216C_id[] = {
	{"P15USB30216C", 0},
	{},
};

static const struct of_device_id P15USB30216C_dt_ids[] = {
	{
		.compatible = "usb,p15usb30216c",
	}, {
		/* sentinel */
	}
};
MODULE_DEVICE_TABLE(of, P15USB30216C_dt_ids);


static unsigned short normal_i2c[] = {0x1d, I2C_CLIENT_END};
/*******************************************************
Function: detect

Noted:
********************************************************/
static int P15USB30216C_detect(struct i2c_client *client, struct i2c_board_info *info)
{
	struct i2c_adapter *adapter = client->adapter;

	if (P15USB30216C_Board_data.twi_id == adapter->nr) {
		strlcpy(info->type, "P15USB30216C", I2C_NAME_SIZE);
		return 0;
	} else {
		return -ENODEV;
	}
}


static struct i2c_driver P15USB30216C_driver = {
	.probe = P15USB30216C_probe,  
	.remove = P15USB30216C_remove,
	.id_table = P15USB30216C_id,
	.driver		= {
		.name = DEVICE_NAME,//"P15USB30216C"
		.owner = THIS_MODULE,
		.of_match_table = P15USB30216C_dt_ids,
	}
};

static int __init P15USB30216C_init(void)  
{
	int res;
	int ret = -ENODEV;
	int used = -1 ;
	struct device_node *np = NULL;
	struct device_node *usbc_np = NULL;
	const char *status;
#ifdef USE_IN_SUNXI
	P15USB30216C_Board_data.is_hw_ready = 0;
	np = of_find_node_by_name(NULL, "P15USB");
	if (!np) {
		pr_err("[%s_%d]ERROR! get P15USB failed\n",__func__, __LINE__);
		goto _err;
	}

	status = of_get_property(np, "status", NULL);
	if(strcmp(status,"okay"))
	{
		return -1 ;
	}

	P15USB30216C_Board_data.is_hw_enabled = 1;

	ret = of_property_read_u32(np, "P15USB_twi_id", &P15USB30216C_Board_data.twi_id);
	if (ret) {
		pr_err("[%s_%d] get P15USB_twi_id failed, %d\n",__FUNCTION__,__LINE__, ret);
		goto _err;
	}
	
	ret = of_property_read_u32(np, "P15USB_twi_addr", &P15USB30216C_Board_data.twi_addr);
	if (ret) {
		pr_err("[%s_%d] get P15USB30216C_twi_addr failed, %d\n",__FUNCTION__,__LINE__, ret);
		goto _err;
	}

	ret = of_property_read_u32(np, "P15USB_HOST_POWER", &P15USB30216C_Board_data.host_power_on);
	if (ret) {
		pr_err("[%s_%d] get P15USB_HOST_POWER failed, %d\n",__FUNCTION__,__LINE__, ret);
		goto _err;
	}

	// get USB_PORT_TYPE
	usbc_np = of_find_node_by_type(NULL, SET_USB0);
	/* usbc port type */
	ret = of_property_read_u32(usbc_np,KEY_USB_PORT_TYPE,&g_usb_cfg.port.port_type);


	P15USB30216C_Board_data.irq_gpio.gpio= of_get_named_gpio_flags(np, "P15USB_int_port", 0,
				(enum of_gpio_flags *)(&(P15USB30216C_Board_data.irq_gpio)));

	P15USB30216C_Board_data.otg_5v_gpio.gpio= of_get_named_gpio_flags(np, "P15USB_OTG_5V_EN", 0,
				(enum of_gpio_flags *)(&(P15USB30216C_Board_data.otg_5v_gpio)));

	P15USB30216C_Board_data.ovp_gpio.gpio= of_get_named_gpio_flags(np, "P15USB_OVP_EN", 0,
				(enum of_gpio_flags *)(&(P15USB30216C_Board_data.ovp_gpio)));

	P15USB30216C_Board_data._0a5_fault.gpio= of_get_named_gpio_flags(np, "P15USB_0A5_FAULT", 0,
				(enum of_gpio_flags *)(&(P15USB30216C_Board_data._0a5_fault)));

	if(P15USB30216C_Board_data.otg_5v_gpio.gpio > 0)
		g_usb_id_gpio_port = P15USB30216C_Board_data.otg_5v_gpio.gpio ;
	else
		printk(KERN_ERR"[%s_%d] Get sysconfig P15USB_OTG_5V_EN  failed !!");

	if(P15USB30216C_Board_data.irq_gpio.gpio < 0 )
		printk(KERN_ERR"[%s_%d] Get sysconfig P15USB_int_port  failed !!");

	if(P15USB30216C_Board_data.ovp_gpio.gpio < 0 )
		printk(KERN_ERR"[%s_%d] Get sysconfig P15USB_OVP_EN  failed !!");

	//printk(KERN_ERR"=====OTG_5V qpio_%d=====\n",P15USB30216C_Board_data.otg_5v_gpio.gpio);
	//printk(KERN_ERR"=====OVP qpio_%d=====\n",P15USB30216C_Board_data.ovp_gpio.gpio);
	//printk(KERN_ERR"=====qpio_%d=====\n",P15USB30216C_Board_data.irq_gpio.gpio);
	if (!gpio_is_valid(P15USB30216C_Board_data.irq_gpio.gpio))
		pr_err("[%s_%d]: irq_gpio is invalid.\n", __FUNCTION__,__LINE__);

	printk("[%s_%d]  twi%d addr 0x%X\n",__FUNCTION__,__LINE__,P15USB30216C_Board_data.twi_id,P15USB30216C_Board_data.twi_addr);

	/*
	res = register_chrdev(I2C_MAJOR,DEVICE_NAME,&i2c_fops);  
	if (res)
	{
		dbg("register_chrdev error\n");  
		return -1;  
	}
	my_dev_class = class_create(THIS_MODULE, DEVICE_NAME);  
	if (IS_ERR(my_dev_class))  
	{  
		dbg("create class error\n");  
		unregister_chrdev(I2C_MAJOR, DEVICE_NAME);  
		return -1;
	}
	*/
#endif
	pr_info("%s(%d)\n",__func__,__LINE__);
	res = i2c_add_driver(&P15USB30216C_driver); 
	return res;
_err:
	return -1;
}  

static void __exit P15USB30216C_exit(void)  
{
	/*
	unregister_chrdev(I2C_MAJOR, DEVICE_NAME);  
	class_destroy(my_dev_class);  
	*/
	i2c_del_driver(&P15USB30216C_driver);  
}  


MODULE_AUTHOR("TEST");  
MODULE_DESCRIPTION("P15USB30216C");  
MODULE_LICENSE("GPL");  
module_init(P15USB30216C_init);  
module_exit(P15USB30216C_exit);  
