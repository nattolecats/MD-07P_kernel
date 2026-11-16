/* this file function is display all devices name */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/errno.h>
#include <linux/mtd/mtd.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/proc_fs.h>

#define HARDWARE_INFO_VERSION	0x02
#if IS_ENABLED(CONFIG_MTK_FB)
const char *hw_lcmname = "lcm not found";
#else
char *hw_lcmname = "lcm not found";
#endif
char *hw_tpname = "tp not found";
char *mainCameraName = "camera not found";
char *subCameraName = "camera not found";
char *main2CameraName = "camera not found";
char *sub2CameraName = "camera not found";
char *main3CameraName = "camera not found";
char *gsensor_name= "gsensor not found";
char *msensor_name= "msensor not found";
char *gyro_name= "gyro not found";
char *alsps_name= "alsps not found";
EXPORT_SYMBOL(hw_lcmname);
EXPORT_SYMBOL(hw_tpname);
EXPORT_SYMBOL(mainCameraName);
EXPORT_SYMBOL(subCameraName);
EXPORT_SYMBOL(main2CameraName);
EXPORT_SYMBOL(main3CameraName);
EXPORT_SYMBOL(gsensor_name);
EXPORT_SYMBOL(msensor_name);
EXPORT_SYMBOL(gyro_name);
EXPORT_SYMBOL(alsps_name);
#if IS_ENABLED(CONFIG_CUSTOM_KERNEL_BAROMETER)
char *baro_name= "baro not found";
EXPORT_SYMBOL(baro_name);
#endif
#if IS_ENABLED(CONFIG_MTK_FINGERPRINT_SUPPORT)
char *fingerprint_name= "fingerprint not found";
EXPORT_SYMBOL(fingerprint_name);
#endif
int otg_cc1 = 0;
int otg_cc2 = 0;
int otg_flag = 9;
EXPORT_SYMBOL(otg_cc1);
EXPORT_SYMBOL(otg_cc2);
EXPORT_SYMBOL(otg_flag);
int g_ctp_fwvr_info = 0;//for tp firmware version
EXPORT_SYMBOL(g_ctp_fwvr_info);
int psensor_rawdata = 0;
int charger_current = 0;

#if IS_ENABLED(CONFIG_MTK_ALSPSHUB)
extern int st_get_pshub_raw_data(int32_t *data);
#endif

//extern u8 tp_identy_vendor;

static ssize_t show_version(struct device *dev,struct device_attribute *attr, char *buf)
{
    int ret_value = 1;
	
    ret_value = sprintf(buf, "Version: 0x%x\n", HARDWARE_INFO_VERSION); 	

    return ret_value;
}
static ssize_t show_lcm(struct device *dev,struct device_attribute *attr, char *buf)
{
    int ret_value = 1;

	if (strcmp("lcm not found", hw_lcmname) == 0)
		ret_value = sprintf(buf, "lcd not found!!!\n");	
	else
		ret_value = sprintf(buf, "lcd name: %s\n", hw_lcmname);
	
    return ret_value;
}
static ssize_t show_tp(struct device *dev,struct device_attribute *attr, char *buf)
{
    int ret_value = 1;

	if (strcmp("tp not found", hw_tpname) == 0)
	{
		ret_value = sprintf(buf, "tp not found!!!\n");
	}else{
	//	if(tp_identy_vendor == 1)
	//	{
			ret_value = sprintf(buf, "tp name: %s  fw_ver: 0x%x\n", hw_tpname,g_ctp_fwvr_info);
	//	}else{
	//		ret_value = sprintf(buf, "tp name: %s_yx  fw_ver: 0x%x\n", hw_tpname,g_ctp_fwvr_info);
	//	}
	}
	
    return ret_value;
}
static ssize_t show_camera(struct device *dev,struct device_attribute *attr, char *buf)
{
    int ret_value = 1;

	if (strcmp("camera not found", mainCameraName) == 0)
		ret_value = sprintf(buf, "MainCamera not found!!!\n");	
	else
		ret_value = sprintf(buf, "MainCamera name: %s\n", mainCameraName);
	
    return ret_value;
}
static ssize_t show_subcamera(struct device *dev,struct device_attribute *attr, char *buf)
{
    int ret_value = 1;

	if (strcmp("camera not found", subCameraName) == 0)
		ret_value = sprintf(buf, "SubCamera not found!!!\n");	
	else
		ret_value = sprintf(buf, "SubCamera name: %s\n", subCameraName);
	
    return ret_value;
}
static ssize_t show_alsps(struct device *dev,struct device_attribute *attr, char *buf)
{
    int ret_value = 1;
	
    if (strcmp("alsps not found", alsps_name) == 0)
		ret_value = sprintf(buf, "AlSPS not found!!!\n"); 	
    else
        ret_value = sprintf(buf, "AlSPS name: %s\n", alsps_name);   

    return ret_value;
}
static ssize_t show_gsensor(struct device *dev,struct device_attribute *attr, char *buf)
{
    int ret_value = 1;

	if (strcmp("gsensor not found", gsensor_name) == 0)
		ret_value = sprintf(buf, "GSensor not found!!!\n");
    else
        ret_value = sprintf(buf, "GSensor name: %s\n", gsensor_name);     
   
    return ret_value;
}
static ssize_t show_msensor(struct device *dev,struct device_attribute *attr, char *buf)
{
    int ret_value = 1;
    
    if (strcmp("msensor not found", msensor_name) == 0)
        ret_value = sprintf(buf, "MSensor not found!!!\n");
    else 
		ret_value = sprintf(buf, "MSensor name: %s\n", msensor_name);
		
    return ret_value;
}
static ssize_t show_gyro(struct device *dev,struct device_attribute *attr, char *buf)
{
    int ret_value = 1;

    if (strcmp("gyro not found", gyro_name) == 0)
		ret_value = sprintf(buf, "Gyro not found!!!\n");
    else
        ret_value = sprintf(buf, "Gyro name: %s\n", gyro_name);

    return ret_value;
}
#if IS_ENABLED(CONFIG_CUSTOM_KERNEL_BAROMETER)
static ssize_t show_baro(struct device *dev,struct device_attribute *attr, char *buf)
{
    int ret_value = 1;

    if (strcmp("baro not found", baro_name) == 0)
		ret_value = sprintf(buf, "Baro not found!!!\n");
    else
        ret_value = sprintf(buf, "Baro name: %s\n", baro_name);

    return ret_value;
}
#endif
#if IS_ENABLED(CONFIG_MTK_FINGERPRINT_SUPPORT)
static ssize_t show_fingerprint(struct device *dev,struct device_attribute *attr, char *buf)
{
    int ret_value = 1;

    if (strcmp("fingerprint not found", fingerprint_name) == 0)
		ret_value = sprintf(buf, "fingerprint name:Not found\n");
    else
        ret_value = sprintf(buf, "Fingerprint name: %s\n", fingerprint_name);

    return ret_value;
}
#endif
static ssize_t show_main2camera(struct device *dev,struct device_attribute *attr, char *buf)
{
    int ret_value = 1;

	if (strcmp("camera not found", main2CameraName) == 0)
		ret_value = sprintf(buf, "Main2Camera not found!!!\n");	
	else
		ret_value = sprintf(buf, "Main2Camera name: %s\n", main2CameraName);
	
    return ret_value;
}
static ssize_t show_main3camera(struct device *dev,struct device_attribute *attr, char *buf)
{
    int ret_value = 1;

	if (strcmp("camera not found", main3CameraName) == 0)
		ret_value = sprintf(buf, "Main3Camera not found!!!\n");	
	else
		ret_value = sprintf(buf, "Main3Camera name: %s\n", main3CameraName);
	
    return ret_value;
}

static ssize_t show_otgcc(struct device *dev,struct device_attribute *attr, char *buf)
{
    int ret_value = 1;
	printk("show_otgcc otg_cc1:%d otg_cc2:%d\n",otg_cc1,otg_cc2);
	if (otg_flag == 0)
	{
		ret_value = sprintf(buf, "0\n");
	}
	else if (otg_flag == 1)
	{
		ret_value = sprintf(buf, "1\n");
	}else{
		ret_value = sprintf(buf, "999\n");
	}
	
    return ret_value;
}

#if IS_ENABLED(CONFIG_MTK_ALSPSHUB)
static ssize_t show_psensorrawdata(struct device *dev,struct device_attribute *attr, char *buf)
{
    int ret_value = 1;
	
	st_get_pshub_raw_data(&psensor_rawdata);
	
	printk("show_psensorrawdata psensor_rawdata:%d\n",psensor_rawdata);

	ret_value = sprintf(buf, "rawdata:%d\n",psensor_rawdata);
	
    return ret_value;
}
#else
static ssize_t show_psensorrawdata(struct device *dev,struct device_attribute *attr, char *buf)
{
    int ret_value = 1;
	
	printk("show_psensorrawdata psensor_rawdata:%d\n",psensor_rawdata);
	ret_value = sprintf(buf, "rawdata:%d\n",psensor_rawdata);
	
    return ret_value;
}	
#endif

static ssize_t show_chg_current(struct device *dev,struct device_attribute *attr, char *buf)
{
    int ret_value = 1;
	
	printk("show_chg_current:%d\n",charger_current);

	ret_value = sprintf(buf, "%d\n",charger_current);
	
    return ret_value;
}



static DEVICE_ATTR(99_version, 0444, show_version, NULL);
static DEVICE_ATTR(01_lcm, 0444, show_lcm, NULL);
static DEVICE_ATTR(02_tp, 0444, show_tp, NULL);
static DEVICE_ATTR(03_camera, 0444, show_camera, NULL);
static DEVICE_ATTR(04_subcamera, 0444, show_subcamera, NULL);
static DEVICE_ATTR(05_gsensor, 0444, show_gsensor, NULL);
static DEVICE_ATTR(06_msensor, 0444, show_msensor, NULL);
static DEVICE_ATTR(07_alsps, 0444, show_alsps, NULL);
static DEVICE_ATTR(08_gyro, 0444, show_gyro, NULL);
#if IS_ENABLED(CONFIG_CUSTOM_KERNEL_BAROMETER)
static DEVICE_ATTR(09_baro, 0444, show_baro, NULL);
#endif
#if IS_ENABLED(CONFIG_MTK_FINGERPRINT_SUPPORT)
static DEVICE_ATTR(10_fingerprint, 0444, show_fingerprint, NULL);
#endif
static DEVICE_ATTR(11_main2camera, 0444, show_main2camera, NULL);
static DEVICE_ATTR(12_main3camera, 0444, show_main3camera, NULL);
static DEVICE_ATTR(13_otgcc, 0444, show_otgcc, NULL);
static DEVICE_ATTR(14_psensorrawdata, 0444, show_psensorrawdata, NULL);
static DEVICE_ATTR(chg_current, 0444, show_chg_current, NULL);

static struct device_attribute *hardware_attrs[] = {
                &dev_attr_99_version,
				&dev_attr_01_lcm,
				&dev_attr_02_tp,
				&dev_attr_03_camera,
				&dev_attr_04_subcamera,
                &dev_attr_05_gsensor,
				&dev_attr_06_msensor,
				&dev_attr_07_alsps,
				&dev_attr_08_gyro,	
#if IS_ENABLED(CONFIG_CUSTOM_KERNEL_BAROMETER)
				&dev_attr_09_baro,
#endif
#if IS_ENABLED(CONFIG_MTK_FINGERPRINT_SUPPORT)
				&dev_attr_10_fingerprint,
#endif
				&dev_attr_11_main2camera,	
				&dev_attr_12_main3camera,	
				&dev_attr_13_otgcc,	
				&dev_attr_14_psensorrawdata,	
				&dev_attr_chg_current,	
                NULL
};

static void remove_sysfs(struct platform_device *pdev)
{
        int i;
        for (i = 0; hardware_attrs[i]; i++)
                device_remove_file(&pdev->dev, hardware_attrs[i]);
}

static int create_sysfs(struct platform_device *pdev)
{
	int res = 0;
	int i;

	for (i = 0; hardware_attrs[i]; i++) {
		res = device_create_file(&pdev->dev, hardware_attrs[i]);
		if (res) {
			printk("failed to create sysfs entry: %s\n",hardware_attrs[i]->attr.name);
			break;
		}
	}
	if (res)
			remove_sysfs(pdev);
	return res;
}

static int HardwareInfo_driver_probe(struct platform_device *dev)	
{	
	int ret_device_file = 0;
	
	ret_device_file = create_sysfs(dev);
	if (ret_device_file)
		return -EFAULT;

    return 0;
}

static int HardwareInfo_driver_remove(struct platform_device *dev)
{
	printk("*** HardwareInfo_drvier_remove!!! ***");
	remove_sysfs(dev);
    return 0;
}

static struct platform_driver HardwareInfo_driver = {
    .probe	= HardwareInfo_driver_probe,
    .remove = HardwareInfo_driver_remove,
    .driver = {
        .name = "HardwareInfo",
    },
};

static struct platform_device HardwareInfo_device = {
    .name   = "HardwareInfo",
    .id	    = -1,
};

static int __init HardwareInfo_mod_init(void)
{
    int ret = 0;

    ret = platform_device_register(&HardwareInfo_device);
    if (ret) {
        printk("*** HardwareInfo_mod_init  Unable to driver register(%d)!!!\n", ret);
        goto fail_2;
    } 
    ret = platform_driver_register(&HardwareInfo_driver);
    if (ret) {
        printk("*** HardwareInfo_mod_init  Unable to driver register(%d)!!!\n", ret);
        goto fail_1;
    }

    goto ok_result;

fail_1:
	platform_driver_unregister(&HardwareInfo_driver);
fail_2:
	platform_device_unregister(&HardwareInfo_device);	
ok_result:

    return ret;
}

static void __exit HardwareInfo_mod_exit(void)
{
    platform_driver_unregister(&HardwareInfo_driver);
	platform_device_unregister(&HardwareInfo_device);
}

/*****************************************************************************/
module_init(HardwareInfo_mod_init);
module_exit(HardwareInfo_mod_exit);
/*****************************************************************************/

MODULE_AUTHOR("<hello@world.com>");
MODULE_DESCRIPTION("MTK Hareware Info driver");
MODULE_LICENSE("GPL");



