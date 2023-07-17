
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/delay.h>

#include <linux/backlight.h>


#include "../../../arch/arm/mach-imx/ntx_hwconfig.h"
#include "../../../arch/arm/mach-imx/ntx_firmware.h"
#include "../../../arch/arm/mach-imx/ntx_firmware_parser.h"



extern volatile unsigned long gdwNTXFW_size;
static struct backlight_device *gptMain_BL_dev;
static int giLastPercent ;

extern int fl_lm3630a_percentageEx (int iChipIdx,int iFL_Percentage);
extern int fl_tlc5947_percentageEx(int iFL_Percentage);
extern int fl_aw99703_x2_percentage(int iFL_Percentage);

extern volatile NTX_HWCONFIG *gptHWCFG;
extern volatile NTX_FIRMWARE_HDR *gptNTXFW;


struct ntxfw_dump_param {
	char *buf;
	int tot_len;
};

static int ntxfw_item_proc(
		NTX_FIRMWARE_HDR *I_ptFWHdr,
		NTX_FIRMWARE_ITEM_HDR *I_ptFWItemHdr,
		void *I_pvFWItemBin,int I_iItemIdx,void *pvParam)
{
	int iRet = 0;
	struct ntxfw_dump_param *ntxfw_dump_params = pvParam;
	int len;
	char *buf;
	
	buf = ntxfw_dump_params->buf + ntxfw_dump_params->tot_len;
	len = sprintf(buf,"subfw[%d].name=\"%s\"\n",I_iItemIdx,
			I_ptFWItemHdr->szFirmwareName);
	ntxfw_dump_params->tot_len += len;

	buf = ntxfw_dump_params->buf + ntxfw_dump_params->tot_len;
	len = sprintf(buf,"subfw[%d].type=0x%x\n",I_iItemIdx,
			I_ptFWItemHdr->wFirmwareType);
	ntxfw_dump_params->tot_len += len;

	buf = ntxfw_dump_params->buf + ntxfw_dump_params->tot_len;
	len = sprintf(buf,"subfw[%d].size=%d\n",I_iItemIdx,
			(int)I_ptFWItemHdr->dwFirmwareSize);
	ntxfw_dump_params->tot_len += len;

	return iRet;
}

static ssize_t fw_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int len , tot_len=0;
	if (0==gptNTXFW) {
		printk(KERN_ERR"ntxfw not exist !!\n");
	}
	else {
		int i;
		struct ntxfw_dump_param tNTXFW_Dump_Params;


		len = sprintf(buf+tot_len,"name=\"%s\"\n",gptNTXFW->szFirmwareName);tot_len += len;
		len = sprintf(buf+tot_len,"size=%d\n",gdwNTXFW_size);tot_len += len;
		len = sprintf(buf+tot_len,"versions=%d.%d.%d.%d\n",
				gptNTXFW->bVersionMajor,gptNTXFW->bVersionSub,
				gptNTXFW->bVersionMinor,gptNTXFW->bVersionMini);tot_len += len;
		len = sprintf(buf+tot_len,"subfw_total=%d\n",gptNTXFW->wFirmwareItems);tot_len += len;

		tNTXFW_Dump_Params.buf = buf;
		tNTXFW_Dump_Params.tot_len = tot_len;
		if(ntx_firmware_parse_fw_buf_ex(gptNTXFW,gdwNTXFW_size,ntxfw_item_proc,(void *)&tNTXFW_Dump_Params)>=0) {
			tot_len = tNTXFW_Dump_Params.tot_len;
		}

	}
	return tot_len;
}

int ntx_set_main_bldev(struct backlight_device *main_bldev)
{
	gptMain_BL_dev = main_bldev;
	return 0;
}

int ntx_set_fl_percent(int percent)
{
	int iRet = 0;

	if(gptHWCFG && gptHWCFG->m_val.bFrontLight) 
	{
		if (2==gptHWCFG->m_val.bFL_PWM||6==gptHWCFG->m_val.bFL_PWM||7==gptHWCFG->m_val.bFL_PWM)
			fl_lm3630a_percentageEx (0,percent);
		else if(5==gptHWCFG->m_val.bFL_PWM)
			fl_lm3630a_percentageEx (1,percent);
		else if(8==gptHWCFG->m_val.bFL_PWM)
			fl_tlc5947_percentageEx(percent);
		else if (0x0c == gptHWCFG->m_val.bFL_PWM) 
			fl_aw99703_x2_percentage(percent);
		else {
			printk(KERN_ERR"%s() fl type %d not ready !\n",
					__FUNCTION__,(int)gptHWCFG->m_val.bFL_PWM);
			iRet = -1;
		}
	}
	else {
		if(gptMain_BL_dev) {
			gptMain_BL_dev->props.brightness = percent * \
				gptMain_BL_dev->props.max_brightness/100;

#if 0
			printk("%s(),brightness=%d set to main bldev %d\n",__FUNCTION__,
				percent,gptMain_BL_dev->props.brightness);
#endif

			backlight_update_status(gptMain_BL_dev);
		
		}
		else {
			printk(KERN_ERR"ntx main bl device not be set !!\n");
			iRet = -2;
		}
	}

	if(iRet>=0) {
		giLastPercent = percent;
	}

	return iRet;	
}

int ntx_get_fl_percent(int *O_piPercent)
{
#if 1 //[
	if(O_piPercent) {
		*O_piPercent = giLastPercent;
	}
	return 0;
#else //][
	if(gptMain_BL_dev) {
		if(O_piPercent) {
			*O_piPercent = gptMain_BL_dev->props.brightness * \
			100/gptMain_BL_dev->props.max_brightness;
		}
		return 0;
	}
	else {
		printk(KERN_ERR"ntx main bl device not be set !!\n");
		return -1;
	}
#endif
}


struct ntx_bl_data {
	//struct device *dev;
	struct backlight_device *ptNTX_bl;
	int iInitPercent;
};
static const struct of_device_id ntx_bl_dt_ids[] = {
	{ .compatible = "ntx,ntx_bl", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, ntx_bl_dt_ids);

static int ntx_bl_update_status(struct backlight_device *bl)
{
	struct ntx_bl_data *ptNTX_BL_Data=0;

	ptNTX_BL_Data = bl_get_data(bl);

	if(ntx_set_fl_percent(bl->props.brightness)>=0) {
		return 0;
	}
	else {
		return -EIO;
	}
}

static int ntx_bl_get_brightness(struct backlight_device *bl)
{
	struct ntx_bl_data *ptNTX_BL_Data=0;
	int brightness;

	ptNTX_BL_Data = bl_get_data(bl);

	ntx_get_fl_percent(&brightness);
	//printk("%s(),brightness=%d\n",__FUNCTION__,brightness);

	return brightness;
}
static const struct backlight_ops ntx_bl_ops = {
//	.options = BL_CORE_SUSPENDRESUME,
	.update_status = ntx_bl_update_status,
	.get_brightness = ntx_bl_get_brightness,
};
static DEVICE_ATTR (fw, 0444, fw_show, NULL);

static int ntx_bl_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct backlight_properties props;

	struct ntx_bl_data *ptNTX_BL_Data=0;

	ptNTX_BL_Data = kmalloc(sizeof(struct ntx_bl_data),GFP_KERNEL);
	if(!ptNTX_BL_Data) {
		dev_err(&pdev->dev,"%s() : memory not enought !\n",__FUNCTION__);
		return -ENOMEM;
	}

	memset(ptNTX_BL_Data,0,sizeof(struct ntx_bl_data));

	platform_set_drvdata(pdev,ptNTX_BL_Data);
	

	//////////////////////////////////////
	// put custom initial data here :
	//
	

	//////////////////////////////////////
	props.type = BACKLIGHT_RAW;
	props.brightness = ptNTX_BL_Data->iInitPercent;
	props.max_brightness = 100;

	ptNTX_BL_Data->ptNTX_bl =
	    devm_backlight_device_register(&pdev->dev, "mxc_msp430.0",&pdev->dev,
					ptNTX_BL_Data ,&ntx_bl_ops, &props);

	if (IS_ERR(ptNTX_BL_Data->ptNTX_bl)) {
		dev_err(&pdev->dev,"%s() : backlight_device_register failed \n",__FUNCTION__);
		goto probe_failed;
	}

	if (device_create_file(&pdev->dev, &dev_attr_fw)< 0) {
		dev_err(&pdev->dev, "fail : msp430 fw create.\n");
	}
	
	return 0;

probe_failed:
	if(ptNTX_BL_Data) {
		kfree(ptNTX_BL_Data);ptNTX_BL_Data=0;
	}
	return -EIO;
}


static int ntx_bl_remove(struct platform_device *pdev)
{
	struct ntx_bl_data *ptNTX_BL_Data;

	ptNTX_BL_Data = platform_get_drvdata(pdev);

	kfree(ptNTX_BL_Data);

	return 0;
}

void ntx_bl_shutdown(struct platform_device *pdev)
{

}

#ifdef CONFIG_PM_SLEEP
static int ntx_bl_suspend(struct device *dev)
{
	return 0;
}

static int ntx_bl_resume(struct device *dev)
{
	return 0;
}
#else
#define ntx_bl_suspend	NULL
#define ntx_bl_resume	NULL
#endif

#ifdef CONFIG_PM
static int ntx_bl_runtime_suspend(struct device *dev)
{
	return 0;
}

static int ntx_bl_runtime_resume(struct device *dev)
{
	return 0;
}
#else
#define ntx_bl_runtime_suspend	NULL
#define ntx_bl_runtime_resume	NULL
#endif


static const struct dev_pm_ops ntx_bl_pm_ops = {
	SET_RUNTIME_PM_OPS(ntx_bl_runtime_suspend,ntx_bl_runtime_resume, NULL)
	SET_SYSTEM_SLEEP_PM_OPS(ntx_bl_suspend, ntx_bl_resume)
};

static struct platform_driver ntx_bl_driver = {
	.probe = ntx_bl_probe,
	.remove = ntx_bl_remove,
	.shutdown = ntx_bl_shutdown,
	.driver = {
		   .name = "ntx_bl",
		   .owner = THIS_MODULE,
		   .of_match_table = of_match_ptr(ntx_bl_dt_ids),
		   .pm = &ntx_bl_pm_ops,
		   },
};

static int __init ntx_bl_init(void)
{
	return platform_driver_register(&ntx_bl_driver);
}

static void __exit ntx_bl_exit(void)
{
	return platform_driver_unregister(&ntx_bl_driver);
}


/*
 * Module entry points
 */
subsys_initcall(ntx_bl_init);
module_exit(ntx_bl_exit);

