/*
 * Copyright (C) 2015 transsion, Inc.
 *
 * Author: peixuan qiu <peixuan.qiu@reallytek.com>
 * this module is intended for supporting module_power function separately
 * 
 */
#include "module_power.h"

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#endif
#include <linux/fb.h>
#include <linux/delay.h>

static struct module_power_context *module_power_data;

int vdd_io_host_en_pin = 0;
int vdd_io_host_en_state = 0;

int vcc_3v3_en_pin = 0;
int vcc_3v3_en_state = 0;


int scan_power_control_pin = 0;
int scan_power_control_state = 0;

int rfid2_power_control_pin = 0;
int rfid2_power_control_state = 0;

int scan_trigger_aim_on_pin = 0;
int scan_trigger_aim_on_state = 0;

int dlledb_en_pin = 0;
int dlledb_en_state = 0;

int key_led_en_pin = 0;
int key_led_en_state = 0;

int rfid_5v_en_pin = 0;
int rfid_5v_en_state = 0;

int dc_en_pin = 0;
int dc_en_state = 0;
#if 0
int vcc_io_illvm_en_pin = 0;
int vcc_io_illvm_en_state = 0;

int uhf_en_3v3_pin = 0;
int uhf_en_3v3_state = 0;

#endif
/*****************************************/
#ifdef CONFIG_OF
struct platform_device *module_powerpdev;
#endif

/*------------------------------------------------------------------------*/
static int module_power_suspend(struct platform_device *dev, pm_message_t state)
{
	HDBG_FUNC();
	return 0;
}

static int module_power_resume(struct platform_device *dev)
{
	HDBG_FUNC();
	return 0;
}

void set_scan_trigger_aim_on(int value)
{
    HDBG_LOG("set_scan_trigger_aim_on value:%d \n",value);
    if(value == 1)
    {
        gpio_direction_output(scan_trigger_aim_on_pin,1);
        gpio_set_value(scan_trigger_aim_on_pin,1);  
        scan_trigger_aim_on_state = 1;              
        }else{
        gpio_direction_output(scan_trigger_aim_on_pin,0);
        gpio_set_value(scan_trigger_aim_on_pin,0);  
        scan_trigger_aim_on_state = 0;              
    }
}
EXPORT_SYMBOL(set_scan_trigger_aim_on);
//   /sys/devices/platform/module_power/scan_trigger_aim_on
static ssize_t module_power_scan_trigger_aim_on_store(struct device *dev,
                            struct device_attribute *attr,
                            const char *buf, size_t count)
{
    HDBG_LOG("%s: %d enter string:%s\n", __func__, __LINE__,buf);
        
        if(buf[0] == '0')
        {
                set_scan_trigger_aim_on(0);
        }
        else if(buf[0] == '1')
        {
                set_scan_trigger_aim_on(1);
        }
        else{
                set_scan_trigger_aim_on(0);
        }
        
        return count;
}
static ssize_t module_power_scan_trigger_aim_on_show(struct device *dev,
                                struct device_attribute *attr, char *buf)
{
        ssize_t count = 0;

        count = sprintf(buf, "%d\n", scan_trigger_aim_on_state);
        return count;
}

void set_rfid2_power_control(int value)
{
	printk("rfid2_power_control value:%d \n",value);
	if(value == 1)
	{
		gpio_direction_output(rfid2_power_control_pin,1);
		gpio_set_value(rfid2_power_control_pin,1);	
		rfid2_power_control_state = 1;		
	}else{
		gpio_direction_output(rfid2_power_control_pin,0);
		gpio_set_value(rfid2_power_control_pin,0);	
		rfid2_power_control_state = 0;		
	}
}
EXPORT_SYMBOL(set_rfid2_power_control);
//   /sys/devices/platform/module_power/rfid2_power_control
static ssize_t module_power_rfid2_power_control_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t count)
{
    printk("%s: %d enter string:%s\n", __func__, __LINE__,buf);
	if(buf[0] == '0')
	{
		set_rfid2_power_control(0);
		//rfid2_power_control_state = 0;
	}
	else if(buf[0] == '1')
	{
		set_rfid2_power_control(1);
		//rfid2_power_control_state = 1;
	}
	else{
		set_rfid2_power_control(0);
		//rfid2_power_control_state = 0;
	}
	
	return count;
}
static ssize_t module_power_rfid2_power_control_show(struct device *dev,
                                struct device_attribute *attr, char *buf)
{
        ssize_t count = 0;

        count = sprintf(buf, "%d\n", rfid2_power_control_state);
        return count;
}

void set_rfid_5v_en(int value)
{
	printk("rfid_5v_en value:%d \n",value);
	if(value == 1)
	{
		gpio_direction_output(rfid_5v_en_pin,1);
		gpio_set_value(rfid_5v_en_pin,1);	
		rfid_5v_en_state = 1;		
	}else{
		gpio_direction_output(rfid_5v_en_pin,0);
		gpio_set_value(rfid_5v_en_pin,0);	
		rfid_5v_en_state = 0;		
	}
}
EXPORT_SYMBOL(set_rfid_5v_en);
//   /sys/devices/platform/module_power/rfid_5v_en
static ssize_t module_power_rfid_5v_en_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t count)
{
    printk("%s: %d enter string:%s\n", __func__, __LINE__,buf);
	if(buf[0] == '0')
	{
		set_rfid_5v_en(0);
		//rfid_5v_en_state = 0;
	}
	else if(buf[0] == '1')
	{
		set_rfid_5v_en(1);
		//rfid_5v_en_state = 1;
	}
	else{
		set_rfid_5v_en(0);
		//rfid_5v_en_state = 0;
	}
	
	return count;
}
static ssize_t module_power_rfid_5v_en_show(struct device *dev,
                                struct device_attribute *attr, char *buf)
{
        ssize_t count = 0;

        count = sprintf(buf, "%d\n", rfid_5v_en_state);
        return count;
}
#if 0
void set_uhf_en_3v3(int value)
{
	printk("uhf_en_3v3 value:%d \n",value);
	if(value == 1)
	{
		gpio_direction_output(uhf_en_3v3_pin,1);
		gpio_set_value(uhf_en_3v3_pin,1);	
		uhf_en_3v3_state = 1;		
	}else{
		gpio_direction_output(uhf_en_3v3_pin,0);
		gpio_set_value(uhf_en_3v3_pin,0);	
		uhf_en_3v3_state = 0;		
	}
}
EXPORT_SYMBOL(set_uhf_en_3v3);
//   /sys/devices/platform/module_power/uhf_en_3v3
static ssize_t module_power_uhf_en_3v3_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t count)
{
    printk("%s: %d enter string:%s\n", __func__, __LINE__,buf);
	if(buf[0] == '0')
	{
		set_uhf_en_3v3(0);
		//rfid_en_3v3_state = 0;
	}
	else if(buf[0] == '1')
	{
		set_uhf_en_3v3(1);
		//rfid_en_3v3_state = 1;
	}
	else{
		set_uhf_en_3v3(0);
		//rfid_en_3v3_state = 0;
	}
	
	return count;
}
static ssize_t module_power_uhf_en_3v3_show(struct device *dev,
                                struct device_attribute *attr, char *buf)
{
        ssize_t count = 0;

        count = sprintf(buf, "%d\n", uhf_en_3v3_state);
        return count;
}

void set_vcc_io_illvm_en(int value)
{
	printk("set_vcc_io_illvm_en value:%d \n",value);
	if(value == 1)
	{
		gpio_direction_output(vcc_io_illvm_en_pin,1);
		gpio_set_value(vcc_io_illvm_en_pin,1);	
		vcc_io_illvm_en_state = 1;		
	}else{
		gpio_direction_output(vcc_io_illvm_en_pin,0);
		gpio_set_value(vcc_io_illvm_en_pin,0);	
		vcc_io_illvm_en_state = 0;		
	}
}
EXPORT_SYMBOL(set_vcc_io_illvm_en);
#endif

void set_scan_power_control(int value)
{
	printk("set_scan_power_control value:%d \n",value);
	if(value == 1)
	{
		gpio_direction_output(scan_power_control_pin,1);
		gpio_set_value(scan_power_control_pin,1);	
		scan_power_control_state = 1;		
	}else{
		gpio_direction_output(scan_power_control_pin,0);
		gpio_set_value(scan_power_control_pin,0);	
		scan_power_control_state = 0;		
	}
}
EXPORT_SYMBOL(set_scan_power_control);

void set_vdd_io_host_en(int value)
{
	printk("set_vdd_io_host_en value:%d \n",value);
	if(value == 1)
	{
		gpio_direction_output(vdd_io_host_en_pin,1);
		gpio_set_value(vdd_io_host_en_pin,1);	
		vdd_io_host_en_state = 1;		
	}else{
		gpio_direction_output(vdd_io_host_en_pin,0);
		gpio_set_value(vdd_io_host_en_pin,0);	
		vdd_io_host_en_state = 0;		
	}
}
EXPORT_SYMBOL(set_vdd_io_host_en);

void set_vcc_3v3_en(int value)
{
	printk("vcc_3v3_en_pin value:%d \n",value);
	if(value == 1)
	{
		gpio_direction_output(vcc_3v3_en_pin,1);
		gpio_set_value(vcc_3v3_en_pin,1);	
		vcc_3v3_en_state = 1;		
	}else{
		gpio_direction_output(vcc_3v3_en_pin,0);
		gpio_set_value(vcc_3v3_en_pin,0);	
		vcc_3v3_en_state = 0;		
	}
}
EXPORT_SYMBOL(set_vcc_3v3_en);

void set_dlledb_en(int value)
{
	printk("dlledb_en_pin value:%d \n",value);
	if(value == 1)
	{
		gpio_direction_output(dlledb_en_pin,1);
		gpio_set_value(dlledb_en_pin,1);	
		dlledb_en_state = 1;		
	}else{
		gpio_direction_output(dlledb_en_pin,0);
		gpio_set_value(dlledb_en_pin,0);	
		dlledb_en_state = 0;		
	}
}
EXPORT_SYMBOL(set_dlledb_en);

void set_key_led_en(int value)
{
	printk("key_led_en_pin value:%d \n",value);
	if(value == 1)
	{
		gpio_direction_output(key_led_en_pin,1);
		gpio_set_value(key_led_en_pin,1);	
		key_led_en_state = 1;		
	}else{
		gpio_direction_output(key_led_en_pin,0);
		gpio_set_value(key_led_en_pin,0);	
		key_led_en_state = 0;		
	}
}
EXPORT_SYMBOL(set_key_led_en);

void set_dc_en(int value)
{
	printk("dc_en_pin value:%d \n",value);
	if(value == 1)
	{
		gpio_direction_output(dc_en_pin,1);
		gpio_set_value(dc_en_pin,1);	
		dc_en_state = 1;		
	}else{
		gpio_direction_output(dc_en_pin,0);
		gpio_set_value(dc_en_pin,0);	
		dc_en_state = 0;		
	}
}
EXPORT_SYMBOL(set_dc_en);
#if 0
//extern int nlscan_power_override;
int nlscan_scanner_power_on(int on)
{
	//printk("%s on:%d, power_override:%d\n", __func__, on, nlscan_power_override);
 printk("%s on:%d\n", __func__, on);
	//if(!nlscan_power_override){
	if (on) {
				printk("nlscan scanner on\n");
				set_vdd_io_host_en(1);
				set_scan_power_control(1);
				set_vcc_io_illvm_en(1);
				// 扫码模组上电
	} else {
				printk("nlscan scanner off\n");
				set_vdd_io_host_en(0);
				set_scan_power_control(0);
				set_vcc_io_illvm_en(0);
				// 扫码模组下电
		}
//	}
 
 return 0;
}
EXPORT_SYMBOL(nlscan_scanner_power_on);
#endif
//   /sys/devices/platform/module_power/scan_power_control
static ssize_t module_power_scan_power_control_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t count)
{
    printk("%s: %d enter string:%s\n", __func__, __LINE__,buf);
	if(buf[0] == '0')
	{
		set_scan_power_control(0);
		//scan_power_control_state = 0;
	}
	else if(buf[0] == '1')
	{
		set_scan_power_control(1);
		//scan_power_control_state = 1;
	}
	else{
		set_scan_power_control(0);
		//scan_power_control_state = 0;
	}
	
	return count;
}
static ssize_t module_power_scan_power_control_show(struct device *dev,
                                struct device_attribute *attr, char *buf)
{
        ssize_t count = 0;

        count = sprintf(buf, "%d\n", scan_power_control_state);
        return count;
}


//   /sys/devices/platform/module_power/vdd_io_host_en
static ssize_t module_power_vdd_io_host_en_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t count)
{
    printk("%s: %d enter string:%s\n", __func__, __LINE__,buf);
	
	if(buf[0] == '0')
	{
		set_vdd_io_host_en(0);
		//vdd_io_host_en_state = 0;
	}
	else if(buf[0] == '1')
	{
		set_vdd_io_host_en(1);
		//vdd_io_host_en_state = 1;
	}
	else{
		set_vdd_io_host_en(0);
		//vdd_io_host_en_state = 0;
	}
	
	return count;
}
static ssize_t module_power_vdd_io_host_en_show(struct device *dev,
                                struct device_attribute *attr, char *buf)
{
        ssize_t count = 0;

        count = sprintf(buf, "%d\n", vdd_io_host_en_state);
        return count;
}

//   /sys/devices/platform/module_power/vcc_3v3_en
static ssize_t module_power_vcc_3v3_en_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t count)
{
    printk("%s: %d enter string:%s\n", __func__, __LINE__,buf);
	
	if(buf[0] == '0')
	{
		set_vcc_3v3_en(0);
		//vdd_io_host_en_state = 0;
	}
	else if(buf[0] == '1')
	{
		set_vcc_3v3_en(1);
		//vdd_io_host_en_state = 1;
	}
	else{
		set_vcc_3v3_en(0);
		//vdd_io_host_en_state = 0;
	}
	
	return count;
}
static ssize_t module_power_vcc_3v3_en_show(struct device *dev,
                                struct device_attribute *attr, char *buf)
{
        ssize_t count = 0;

        count = sprintf(buf, "%d\n", vcc_3v3_en_state);
        return count;
}

//   /sys/devices/platform/module_power/dlledb_en
static ssize_t module_power_dlledb_en_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t count)
{
    printk("%s: %d enter string:%s\n", __func__, __LINE__,buf);
	
	if(buf[0] == '0')
	{
		set_dlledb_en(0);
		//dlledb_en_state = 0;
	}
	//else if(buf[0] == '1')
	//{
	//	set_dlledb_en(1);
		//dlledb_en_state = 1;
	//}
	else{
		set_dlledb_en(1);
		//dlledb_en_state = 0;
	}
	
	return count;
}
static ssize_t module_power_dlledb_en_show(struct device *dev,
                                struct device_attribute *attr, char *buf)
{
        ssize_t count = 0;

        count = sprintf(buf, "%d\n", dlledb_en_state);
        return count;
}

//   /sys/devices/platform/module_power/key_led_en
static ssize_t module_power_key_led_en_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t count)
{
    printk("%s: %d enter string:%s\n", __func__, __LINE__,buf);
	
	if(buf[0] == '0')
	{
		set_key_led_en(0);
		//key_led_en_state = 0;
	}
	//else if(buf[0] == '1')
	//{
	//	set_key_led_en(1);
		//key_led_en_state = 1;
	//}
	else{
		set_key_led_en(1);
		//key_led_en_state = 0;
	}
	
	return count;
}
static ssize_t module_power_key_led_en_show(struct device *dev,
                                struct device_attribute *attr, char *buf)
{
        ssize_t count = 0;

        count = sprintf(buf, "%d\n", key_led_en_state);
        return count;
}

//   /sys/devices/platform/module_power/dc_en
static ssize_t module_power_dc_en_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t count)
{
    printk("%s: %d enter string:%s\n", __func__, __LINE__,buf);
	
	if(buf[0] == '0')
	{
		set_dc_en(0);
		//dc_en_state = 0;
	}
	else if(buf[0] == '1')
	{
		set_dc_en(1);
		//dc_en_state = 1;
	}
	else{
		set_dc_en(0);
		//dc_en_state = 0;
	}
	
	return count;
}
static ssize_t module_power_dc_en_show(struct device *dev,
                                struct device_attribute *attr, char *buf)
{
        ssize_t count = 0;

        count = sprintf(buf, "%d\n", dc_en_state);
        return count;
}
static DEVICE_ATTR(vdd_io_host_en, 0664, module_power_vdd_io_host_en_show, module_power_vdd_io_host_en_store);
static DEVICE_ATTR(vcc_3v3_en, 0664, module_power_vcc_3v3_en_show, module_power_vcc_3v3_en_store);
static DEVICE_ATTR(dlledb_en, 0664, module_power_dlledb_en_show, module_power_dlledb_en_store);
static DEVICE_ATTR(key_led_en, 0664, module_power_key_led_en_show, module_power_key_led_en_store);
static DEVICE_ATTR(dc_en, 0664, module_power_dc_en_show, module_power_dc_en_store);
static DEVICE_ATTR(scan_power_control, 0664, module_power_scan_power_control_show, module_power_scan_power_control_store);
static DEVICE_ATTR(rfid2_power_control, 0664, module_power_rfid2_power_control_show, module_power_rfid2_power_control_store);
static DEVICE_ATTR(rfid_5v_en, 0664, module_power_rfid_5v_en_show, module_power_rfid_5v_en_store);
//static DEVICE_ATTR(uhf_en_3v3, 0664, module_power_uhf_en_3v3_show, module_power_uhf_en_3v3_store);
static DEVICE_ATTR(scan_trigger_aim_on, 0664, module_power_scan_trigger_aim_on_show, module_power_scan_trigger_aim_on_store);

static struct attribute *module_power_attributes[] = {
	&dev_attr_vdd_io_host_en.attr,
	&dev_attr_vcc_3v3_en.attr,
	&dev_attr_dlledb_en.attr,
	&dev_attr_key_led_en.attr,
	&dev_attr_scan_power_control.attr,
	&dev_attr_rfid2_power_control.attr,
	&dev_attr_rfid_5v_en.attr,
//	&dev_attr_uhf_en_3v3.attr,
	&dev_attr_scan_trigger_aim_on.attr,
	&dev_attr_dc_en.attr,
	NULL
};

static const struct attribute_group module_power_attr_group = {
	.attrs = module_power_attributes,
};

/*--------------------------------------------------------------------------*/
static int module_power_probe(struct platform_device *pdev)
{
	int err = 0;	
	struct device *dev = &pdev->dev;
	HDBG_FUNC();

	module_power_data = kzalloc(sizeof(*module_power_data), GFP_KERNEL);
	if (!module_power_data) {
		err = -ENOMEM;
		goto err_kzalloc;
	}

	vdd_io_host_en_pin = of_get_named_gpio(dev->of_node,"vdd-io-host-en",0);
	if(vdd_io_host_en_pin < 0){
		pr_info("%s: failed no vdd_io_host_en_pin:%d \n", __func__,vdd_io_host_en_pin);
		return -ENODATA;
	}
	gpio_request(vdd_io_host_en_pin,"vdd_io_host_en");

	set_vdd_io_host_en(1);
	
	vcc_3v3_en_pin = of_get_named_gpio(dev->of_node,"vcc-3v3-en",0);
	if(vcc_3v3_en_pin < 0){
		pr_info("%s: failed no vcc_3v3_en_pin:%d \n", __func__,vcc_3v3_en_pin);
		return -ENODATA;
	}
	gpio_request(vcc_3v3_en_pin,"vcc-3v3-en");

	set_vcc_3v3_en(1);
	
	dlledb_en_pin = of_get_named_gpio(dev->of_node,"dlledb_en",0);
	if(dlledb_en_pin < 0){
		pr_info("%s: failed no dlledb_en_pin:%d \n", __func__,dlledb_en_pin);
		return -ENODATA;
	}
	gpio_request(dlledb_en_pin,"dlledb_en");
	
	key_led_en_pin = of_get_named_gpio(dev->of_node,"key_led_en",0);
	if(key_led_en_pin < 0){
		pr_info("%s: failed no key_led_en_pin:%d \n", __func__,key_led_en_pin);
		return -ENODATA;
	}
	gpio_request(key_led_en_pin,"key_led_en");
	
	dc_en_pin = of_get_named_gpio(dev->of_node,"dc_en",0);
	if(dc_en_pin < 0){
		pr_info("%s: failed no dc_en_pin:%d \n", __func__,dc_en_pin);
		return -ENODATA;
	}
	gpio_request(dc_en_pin,"dc_en");

	//set_vcc_3v3_en(1);
#if 0	
	vcc_io_illvm_en_pin = of_get_named_gpio(dev->of_node,"vcc-io-illvm-en",0);
	if(vcc_io_illvm_en_pin < 0){
		pr_info("%s: failed no vcc_io_illvm_en_pin:%d \n", __func__,vcc_io_illvm_en_pin);
		return -ENODATA;
	}
	gpio_request(vcc_io_illvm_en_pin,"vcc-io-illvm-en");	
#endif	
	scan_power_control_pin = of_get_named_gpio(dev->of_node,"scan_power_control",0);
	if(scan_power_control_pin < 0){
		pr_info("%s: failed no scan_power_control_pin:%d \n", __func__,scan_power_control_pin);
		return -ENODATA;
	}
	gpio_request(scan_power_control_pin,"scan_power_control");
	
	//set_scan_power_control(1);
	
	rfid2_power_control_pin = of_get_named_gpio(dev->of_node,"rfid2_power_control",0);
	if(rfid2_power_control_pin < 0){
		pr_info("%s: failed no rfid2_power_control_pin:%d \n", __func__,rfid2_power_control_pin);
		return -ENODATA;
	}
	gpio_request(rfid2_power_control_pin,"rfid2_power_control");
	
	//set_rfid2_power_control(1);	
#if 0
	uhf_en_3v3_pin = of_get_named_gpio(dev->of_node,"uhf_en_3v3",0);
	if(uhf_en_3v3_pin < 0){
		pr_info("%s: failed no uhf_en_3v3_pin:%d \n", __func__,uhf_en_3v3_pin);
		return -ENODATA;
	}
	gpio_request(uhf_en_3v3_pin,"uhf_en_3v3");
	
	//set_uhf_en_3v3(1);	
#endif	
    scan_trigger_aim_on_pin = of_get_named_gpio(dev->of_node,"scan_trigger_aim_on",0);
    if(scan_trigger_aim_on_pin < 0){
        pr_info("%s: failed no scan_trigger_aim_on_pin:%d \n", __func__,scan_trigger_aim_on_pin);
        return -ENODATA;
    }
    gpio_request(scan_trigger_aim_on_pin,"scan_trigger_aim_on");
       
   // set_scan_trigger_aim_on(1);  	
	
	rfid_5v_en_pin = of_get_named_gpio(dev->of_node,"rfid_5v_en",0);
	if(rfid_5v_en_pin < 0){
		pr_info("%s: failed no rfid_5v_en_pin:%d \n", __func__,rfid_5v_en_pin);
		return -ENODATA;
	}
	gpio_request(rfid_5v_en_pin,"rfid_5v_en");
	
	err = sysfs_create_group(&dev->kobj, &module_power_attr_group);
	if (err < 0)
		printk("module_power attr group create failed\n");
    else
		printk("module_power attr group create success!\n");		

	HDBG_LOG("probe done!!!\n");
	return err;


err_kzalloc:
	return err;
}

/*-------------------------------------------------------------------------*/
/* should never be called */
static int module_power_remove(struct platform_device *pdev)
{
	HDBG_FUNC();

	return 0;
}

/*------------------------------------------------------------------------*/
static const struct of_device_id of_match[] = {
	{ .compatible = "mediatek,module_power", },
	{},
};
/*------------------------------------------------------------------------*/

static struct platform_driver module_power_driver = {
	.probe = module_power_probe,
	.remove = module_power_remove,
	.suspend    = module_power_suspend,
	.resume     = module_power_resume,
	.driver = {
		.name = MODULE_POWER_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = of_match,
#endif
	},
};
/*------------------------------------------------------------------------*/

static int __init module_power_init(void)
{
	int err = 0;
	HDBG_FUNC();
	
	err = platform_driver_register(&module_power_driver);
	if (err) {
		HDBG_LOG("register platform driver failed!!!\n");
		goto err_platform_driver_register;
	}
	
	return err;
	
err_platform_driver_register:
#if !defined(CONFIG_OF)
	platform_device_unregister(&module_power_device);
err_platform_device_register:
#endif

	return err;
}

/* should never be called */
static void __exit module_power_exit(void)
{	
	HDBG_FUNC();
}

//late_initcall(module_power_init);
module_init(module_power_init);
module_exit(module_power_exit);

MODULE_AUTHOR("haifeng.zou@sunritel.com");
MODULE_DESCRIPTION("module_power support for wind");
MODULE_LICENSE("GPL");
