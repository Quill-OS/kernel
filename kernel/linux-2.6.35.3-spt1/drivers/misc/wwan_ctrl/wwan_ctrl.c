/*
	wwan_ctrl.c
	
	Copyright (C) 2011 Sony Corporation
	
	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License, version 2, as
	published by the Free Software Foundation.
	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
	GNU General Public License for more details.
	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA 
*/

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/sysfs.h>
#include <linux/fs.h>
#include <linux/err.h>
#include <linux/miscdevice.h>
#include <linux/regulator/consumer.h>
#include <linux/wwan_ctrl.h>
#include <mach/gpio.h>



#define WAN_POWER_EN	(3*32 + 3)	/*GPIO_4_3 */
#define WWAN_DISABLE	(3*32 + 7)	/*GPIO_4_7 */

/* device info */
struct wwan_info {
	int w_disable;				//H:1 L:0
	int wan_power;				//ON:1 OFF:0
};

static struct platform_device *wwan_ctrl_pdev;





/*****************************************************************************
	set_w_disable
		W_DISABLE port control
*****************************************************************************/
static void set_w_disable( struct wwan_info *info,int state )
{
	if( state )
	{
		info->w_disable = 1;
		gpio_set_value( WWAN_DISABLE,info->w_disable );		//H:Disable
	}
	else
	{
		info->w_disable = 0;
		gpio_set_value( WWAN_DISABLE,info->w_disable );		//L:Enable
	}
//printk("w_disable %d\n",info->w_disable);
}

/*****************************************************************************
	set_power
		WWAN & USB Host power control
*****************************************************************************/
static void set_power( struct wwan_info *info,int on )
{
	if( on )
	{
		info->wan_power = 1;
		//ON
		gpio_set_value( WAN_POWER_EN,1 );		//WWAN3.1V H:ON
//printk("wwan power on %d\n",info->wan_power);
	}
	else
	{
		info->wan_power = 0;
		//OFF
		gpio_set_value( WAN_POWER_EN,0 );		//WWAN3.1V L:OFF
//printk("wwan power off %d\n",info->wan_power);
	}
}



/*****************************************************************************
	show_w_disable
*****************************************************************************/
static ssize_t show_w_disable(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct wwan_info *info = dev_get_drvdata( dev );
	
	snprintf( buf,PAGE_SIZE,"%d\n", info->w_disable );
	
	return strlen( buf );
}
/*****************************************************************************
	store_w_disable
*****************************************************************************/
static ssize_t store_w_disable(struct device *dev, struct device_attribute *attr,
								const char *buf, size_t count)
{
	struct wwan_info *info = dev_get_drvdata( dev );
	int state;
	
	sscanf( buf,"%d", &state );
	set_w_disable( info,state );		//W_DISABLE control
	
	return strlen( buf );
}

/*****************************************************************************
	show_wan_power
*****************************************************************************/
static ssize_t show_wan_power(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct wwan_info *info = dev_get_drvdata( dev );
	
	snprintf( buf,PAGE_SIZE,"%d\n", info->wan_power );
	return strlen( buf );
}
/*****************************************************************************
	store_wan_power
*****************************************************************************/
static ssize_t store_wan_power(struct device *dev, struct device_attribute *attr,
								const char *buf, size_t count)
{
	struct wwan_info *info = dev_get_drvdata( dev );
	int state;
	
	sscanf( buf,"%d", &state );
	set_power( info,state );			//power control
	
	return strlen( buf );
}

static DEVICE_ATTR( w_disable,  S_IRUGO | S_IWUGO, show_w_disable, store_w_disable);
static DEVICE_ATTR( wwan_power, S_IRUGO | S_IWUGO, show_wan_power, store_wan_power);


static struct attribute *wwan_attrs[] = {
	&dev_attr_w_disable.attr,
	&dev_attr_wwan_power.attr,
	NULL,
};

static struct attribute_group attr_group = {
	.attrs = wwan_attrs,
};



/*****************************************************************************
	wwan_ctrl_open
*****************************************************************************/
static int wwan_ctrl_open( struct inode *inode, struct file *file )
{
	struct wwan_info *info = platform_get_drvdata( wwan_ctrl_pdev );
	
	file->private_data = info;
	
	return 0;
}

/*****************************************************************************
	wwan_ctrl_release
*****************************************************************************/
static int wwan_ctrl_release( struct inode *inode, struct file *file )
{
	return 0;
}

/*****************************************************************************
	wwan_ctrl_ioctl
*****************************************************************************/
static int wwan_ctrl_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg )
{
	struct wwan_info *info = file->private_data;
	
	switch( cmd )
	{
		case WWAN_IOCTL_SET_W_DISABLE_H:
			set_w_disable( info,1 );
			break;
		case WWAN_IOCTL_SET_W_DISABLE_L:
			set_w_disable( info,0 );
			break;
		case WWAN_IOCTL_SET_POWER_ON:
			set_power( info,1 );
			break;
		case WWAN_IOCTL_SET_POWER_OFF:
			set_power( info,0 );
			break;
	}
	
	return 0;
}

const struct file_operations wwan_ctrl_fops = {
	.owner   = THIS_MODULE,
	.open    = wwan_ctrl_open,
	.release = wwan_ctrl_release,
	.ioctl   = wwan_ctrl_ioctl,
};

static struct miscdevice wwan_ctrl_miscdev = {
	.minor = MISC_DYNAMIC_MINOR,	/* dynamic allocation */
	.name = "wwan_ctrl",			/* /dev/wwan_ctrl */
	.fops = &wwan_ctrl_fops
};

/*****************************************************************************
	wwan_ctrl_probe
*****************************************************************************/
static int wwan_ctrl_probe(struct platform_device *pdev)
{
	int ret;
	struct wwan_info *info;
	
	info = kzalloc(sizeof(*info), GFP_KERNEL);
	if (info == NULL) {
		dev_err( pdev->dev.parent,"Failed to allocate private data\n");
		return -ENOMEM;
	}
	
	platform_set_drvdata( pdev,info );
	wwan_ctrl_pdev = pdev;
	
	ret = sysfs_create_group( &pdev->dev.kobj,&attr_group );
	if( ret ) {
		dev_err( pdev->dev.parent,"Failed to register sysfs.\n");
		kfree( info );
		return ret;
	}
	
	ret = misc_register( &wwan_ctrl_miscdev );
	if (ret) {
		kfree( info );
		return ret;
	}
	//initialize
	set_w_disable( info,0 );		//W_DISABLE:Enable
//	set_power( info,0 );			//power:OFF
	
	return 0;
}

/*****************************************************************************
	wwan_ctrl_remove
*****************************************************************************/
static int wwan_ctrl_remove(struct platform_device *pdev)
{
	struct wwan_info *info = platform_get_drvdata( pdev );
	
	//initialize
	set_w_disable( info,0 );		//W_DISABLE:Enable
//	set_power( info,0 );			//power:OFF
	
	misc_deregister( &wwan_ctrl_miscdev );
	
	kfree( info );
	wwan_ctrl_pdev = NULL;
	
	sysfs_remove_group( &pdev->dev.kobj, &attr_group );
	return 0;
}

static struct platform_driver wwan_ctrl_driver = {
	.probe		= wwan_ctrl_probe,
	.remove		= wwan_ctrl_remove,
	.driver		= {
		.name	= "wwan_ctrl",
	},
};

static int __devinit wwan_ctrl_init(void)
{
	return platform_driver_register( &wwan_ctrl_driver );
}

static void __exit wwan_ctrl_exit(void)
{
	return platform_driver_unregister( &wwan_ctrl_driver );
}

module_init(wwan_ctrl_init);
module_exit(wwan_ctrl_exit);
