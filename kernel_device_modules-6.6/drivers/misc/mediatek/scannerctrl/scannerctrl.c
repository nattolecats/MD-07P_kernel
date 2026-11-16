/*
 * MTK scan Control Driver for Kernel 6.6
 *
 * Copyright (C) 2023 Your Company
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/gpio/consumer.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#define DRIVER_NAME "mtk_scan"

struct mtk_scan_data {
    struct gpio_desc *rst_gpio;
    struct gpio_desc *int_gpio;
	struct gpio_desc *scan_3v3_gpio;
	struct gpio_desc *scan_5v_gpio;
	struct gpio_desc *buzz_gpio;
};

static struct platform_device *g_pdev;
#if 0
static void set_gpio_value(struct mtk_scan_data *data, int value)
{
    gpiod_set_value(data->rst_gpio, value);
     printk(KERN_INFO "rst_gpio set to %d\n", value);
    gpiod_set_value(data->scan_3v3_gpio, value);
    printk(KERN_INFO "scan_3v3_gpio set to %d\n", value);
	gpiod_set_value(data->scan_5v_gpio, value);
    printk(KERN_INFO "scan_5v_gpio set to %d\n", value);
	gpiod_set_value(data->int_gpio, value);
    printk(KERN_INFO "int_gpio set to %d\n", value);
	// gpiod_set_value(data->buzz_gpio, value);
    // printk(KERN_INFO "buzz_gpio set to %d\n", value);
}

static void get_gpio_value(struct mtk_scan_data *data)
{
    printk(KERN_INFO "rst_gpio value is %d\n", gpiod_get_value(data->rst_gpio));
    printk(KERN_INFO "int_gpio value is %d\n", gpiod_get_value(data->int_gpio));
	printk(KERN_INFO "scan_3v3_gpio value is %d\n", gpiod_get_value(data->scan_3v3_gpio));
	printk(KERN_INFO "scan_5v_gpio value is %d\n", gpiod_get_value(data->scan_5v_gpio));
	// printk(KERN_INFO "buzz_gpio value is %d\n", gpiod_get_value(data->buzz_gpio));
}
#endif
static void scan_power_en(struct mtk_scan_data *data)
{
	// gpiod_set_value(data->scan_5v_gpio, 1);
	// gpiod_set_value(data->scan_3v3_gpio, 1);
	gpiod_set_value(data->int_gpio, 0);
	msleep(10);
}
static void scan_power_off(struct mtk_scan_data *data)
{
	// gpiod_set_value(data->scan_5v_gpio, 0);
	// gpiod_set_value(data->scan_3v3_gpio, 0);
	gpiod_set_value(data->int_gpio, 1);
	msleep(10);
}
static unsigned int scan_cam_flag = 0;
static unsigned int buzz_flag = 0;
static void buzz_power_en(struct mtk_scan_data *data)
{
	int buzz_count = 0;
	for (buzz_count = 0; buzz_count < 20; buzz_count++)
	{
	gpiod_set_value(data->buzz_gpio, 1);
	usleep_range(180 , 185);
	gpiod_set_value(data->buzz_gpio, 0);
	usleep_range(180 , 185);
	}
	msleep(100);
	for (buzz_count = 0; buzz_count < 20; buzz_count++)
	{
	gpiod_set_value(data->buzz_gpio, 1);
	usleep_range(370 , 375);
	gpiod_set_value(data->buzz_gpio, 0);
	usleep_range(370 , 375);
	}
	msleep(100);
	for (buzz_count = 0; buzz_count < 20; buzz_count++)
	{
	gpiod_set_value(data->buzz_gpio, 1);
	usleep_range(370 , 375);
	gpiod_set_value(data->buzz_gpio, 0);
	usleep_range(370 , 375);
	}
}
static void buzz_power_off(struct mtk_scan_data *data)
{
	gpiod_set_value(data->buzz_gpio, 0);
}

static ssize_t scan_cam_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{

	return sprintf(buf, "%d\n", scan_cam_flag);
} 

static ssize_t scan_cam_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct mtk_scan_data *data = dev_get_drvdata(dev);

    if (!data) {
        printk(KERN_ERR "Failed to get driver data\n");
        return -EINVAL;
    }
	if(buf[0] == '1'){
    scan_power_en(data);
	printk("%s %d scan_cam_flag %d\n", __func__,__LINE__,scan_cam_flag);
	scan_cam_flag = 1;
	}
	else if(buf[0] == '0'){
        scan_power_off(data);
		printk("%s %d scan_cam_flag %d\n", __func__,__LINE__,scan_cam_flag);
	    scan_cam_flag = 0;
	}
	return count;
		
}
static DEVICE_ATTR(scan_cam, 0664, scan_cam_show,scan_cam_store);
static ssize_t buzz_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{

	return sprintf(buf, "%d\n", buzz_flag);
} 

static ssize_t buzz_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct mtk_scan_data *data = dev_get_drvdata(dev);

    if (!data) {
        printk(KERN_ERR "Failed to get driver data\n");
        return -EINVAL;
    }
	if(buf[0] == '1'){
	buzz_power_en(data);
	printk("%s %d buzz_flag %d\n", __func__,__LINE__,buzz_flag);
	buzz_flag = 1;
	}
	else if(buf[0] == '0'){
		buzz_power_off(data);
		printk("%s %d buzz_flag %d\n", __func__,__LINE__,buzz_flag);
	    buzz_flag = 0;
	}
	return count;
		
}
static DEVICE_ATTR(buzz, 0664, buzz_show,buzz_store);
static int mtk_scan_probe(struct platform_device *pdev)
{
    struct device *dev = &pdev->dev;
    struct mtk_scan_data *data;

    g_pdev = pdev;

    data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
    if (!data)
        return -ENOMEM;
    data->rst_gpio = devm_gpiod_get(dev, "rst", GPIOD_OUT_HIGH);
    if (IS_ERR(data->rst_gpio)) {
        dev_err(dev, "Failed to get rst GPIO\n");
        return PTR_ERR(data->rst_gpio);
    }
    data->int_gpio = devm_gpiod_get(dev, "int", GPIOD_OUT_HIGH);
    if (IS_ERR(data->int_gpio)) {
        dev_err(dev, "Failed to get int GPIO\n");
        return PTR_ERR(data->int_gpio);
    }
    data->scan_3v3_gpio = devm_gpiod_get(dev, "scan_3v3", GPIOD_OUT_HIGH);
    if (IS_ERR(data->scan_3v3_gpio)) {
        dev_err(dev, "Failed to get scan_3v3 GPIO\n");
        return PTR_ERR(data->scan_3v3_gpio);
    }
    data->scan_5v_gpio = devm_gpiod_get(dev, "scan_5v", GPIOD_OUT_HIGH);
    if (IS_ERR(data->scan_5v_gpio)) {
        dev_err(dev, "Failed to get scan_5v GPIO\n");
        return PTR_ERR(data->scan_5v_gpio);
    }
	data->buzz_gpio = devm_gpiod_get(dev, "buzz", GPIOD_OUT_HIGH);
    if (IS_ERR(data->buzz_gpio)) {
        dev_err(dev, "Failed to get buzz_gpio GPIO\n");
        return PTR_ERR(data->buzz_gpio);
    }
	gpiod_set_value(data->int_gpio, 1);
	gpiod_set_value(data->rst_gpio, 0);
	gpiod_set_value(data->scan_5v_gpio, 1);
	gpiod_set_value(data->scan_3v3_gpio, 1);
	gpiod_set_value(data->buzz_gpio, 0);
    platform_set_drvdata(pdev, data);
    device_create_file(dev, &dev_attr_scan_cam);
	device_create_file(dev, &dev_attr_buzz);
    dev_info(dev, "MTK GPIO driver probed successfully\n");
    return 0;
}

static int mtk_scan_remove(struct platform_device *pdev)
{
    dev_info(&pdev->dev, "MTK GPIO driver removed\n");
    return 0;
}

static const struct of_device_id mtk_scan_of_match[] = {
    { .compatible = "mediatek,mtk-scan" },
    { /* end of list */ }
};
MODULE_DEVICE_TABLE(of, mtk_scan_of_match);

static struct platform_driver mtk_scan_driver = {
    .probe = mtk_scan_probe,
    .remove = mtk_scan_remove,
    .driver = {
        .name = DRIVER_NAME,
        .of_match_table = mtk_scan_of_match,
    },
};

module_platform_driver(mtk_scan_driver);

MODULE_AUTHOR("zlw");
MODULE_DESCRIPTION("MTK scan Control Driver for Kernel 6.6");
MODULE_LICENSE("GPL");