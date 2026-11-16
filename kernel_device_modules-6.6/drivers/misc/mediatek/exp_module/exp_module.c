/*
 * MTK exp_module Control Driver for Kernel 6.6
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
#define DRIVER_NAME "mtk_exp_module"

struct mtk_exp_module_data {
    struct gpio_desc *uart3v3_gpio;
    struct gpio_desc *uarts_gpio;
	struct gpio_desc *uartoe_gpio;
	struct gpio_desc *kzgpio1_gpio;
	struct gpio_desc *kzgpio2_gpio;
	struct gpio_desc *rtken_gpio;
};

// static struct platform_device *g_pdev;
static void gas_power_en(struct mtk_exp_module_data *data)
{
	gpiod_set_value(data->uart3v3_gpio, 1);
	msleep(10);
	gpiod_set_value(data->uarts_gpio, 1);
	msleep(10);
	gpiod_set_value(data->kzgpio1_gpio, 1);
	// gpiod_set_value(data->kzgpio2_gpio, 1);
}
static void gas_power_off(struct mtk_exp_module_data *data)
{
	gpiod_set_value(data->kzgpio1_gpio, 0);
	// gpiod_set_value(data->kzgpio2_gpio, 0);
	msleep(10);
	gpiod_set_value(data->uarts_gpio, 0);
	msleep(10);
	gpiod_set_value(data->uart3v3_gpio, 0);
}
static void rtk_power_en(struct mtk_exp_module_data *data)
{
	gpiod_set_value(data->uart3v3_gpio, 1);
	msleep(10);
	gpiod_set_value(data->rtken_gpio, 1);
}
static void rtk_power_off(struct mtk_exp_module_data *data)
{
	gpiod_set_value(data->rtken_gpio, 0);
	msleep(10);
	gpiod_set_value(data->uart3v3_gpio, 0);
}
static unsigned int exp_module_gas_flag = 0;
static ssize_t exp_module_gas_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{

	return sprintf(buf, "%d\n", exp_module_gas_flag);
} 

static ssize_t exp_module_gas_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct mtk_exp_module_data *data = dev_get_drvdata(dev);

    if (!data) {
        printk(KERN_ERR "Failed to get driver data\n");
        return -EINVAL;
    }
	if(buf[0] == '1'){
    gas_power_en(data);
	printk("%s %d exp_module_gas_flag %d\n", __func__,__LINE__,exp_module_gas_flag);
	exp_module_gas_flag = 1;
	}
	else if(buf[0] == '0'){
        gas_power_off(data);
		printk("%s %d exp_module_gas_flag %d\n", __func__,__LINE__,exp_module_gas_flag);
	    exp_module_gas_flag = 0;
	}
	return count;
		
}
static DEVICE_ATTR(exp_module_gas, 0664, exp_module_gas_show,exp_module_gas_store);
static unsigned int exp_module_rtk_flag = 0;
static ssize_t exp_module_rtk_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{

	return sprintf(buf, "%d\n", exp_module_rtk_flag);
} 

static ssize_t exp_module_rtk_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct mtk_exp_module_data *data = dev_get_drvdata(dev);

    if (!data) {
        printk(KERN_ERR "Failed to get driver data\n");
        return -EINVAL;
    }
	if(buf[0] == '1'){
    rtk_power_en(data);
	printk("%s %d exp_module_rtk_flag %d\n", __func__,__LINE__,exp_module_rtk_flag);
	exp_module_rtk_flag = 1;
	}
	else if(buf[0] == '0'){
        rtk_power_off(data);
		printk("%s %d exp_module_rtk_flag %d\n", __func__,__LINE__,exp_module_rtk_flag);
	    exp_module_rtk_flag = 0;
	}
	return count;
		
}
static DEVICE_ATTR(exp_module_rtk, 0664, exp_module_rtk_show,exp_module_rtk_store);
static int mtk_exp_module_probe(struct platform_device *pdev)
{
    struct device *dev = &pdev->dev;
    struct mtk_exp_module_data *data;

    // g_pdev = pdev;

    data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
    if (!data)
        return -ENOMEM;
    data->uart3v3_gpio = devm_gpiod_get(dev, "uart3v3", GPIOD_OUT_HIGH);
    if (IS_ERR(data->uart3v3_gpio)) {
        dev_err(dev, "Failed to get uart3v3 GPIO\n");
        return PTR_ERR(data->uart3v3_gpio);
    }
    data->uarts_gpio = devm_gpiod_get(dev, "uarts", GPIOD_OUT_HIGH);
    if (IS_ERR(data->uarts_gpio)) {
        dev_err(dev, "Failed to get uarts GPIO\n");
        return PTR_ERR(data->uarts_gpio);
    }
    data->uartoe_gpio = devm_gpiod_get(dev, "uartoe", GPIOD_OUT_HIGH);
    if (IS_ERR(data->uartoe_gpio)) {
        dev_err(dev, "Failed to get uartoe GPIO\n");
        return PTR_ERR(data->uartoe_gpio);
    }
    data->kzgpio1_gpio = devm_gpiod_get(dev, "kzgpio1", GPIOD_OUT_HIGH);
    if (IS_ERR(data->kzgpio1_gpio)) {
        dev_err(dev, "Failed to get kzgpio1 GPIO\n");
        return PTR_ERR(data->kzgpio1_gpio);
    }
	data->kzgpio2_gpio = devm_gpiod_get(dev, "kzgpio2", GPIOD_OUT_HIGH);
    if (IS_ERR(data->kzgpio2_gpio)) {
        dev_err(dev, "Failed to get kzgpio2 GPIO\n");
        return PTR_ERR(data->kzgpio2_gpio);
    }
	data->rtken_gpio = devm_gpiod_get(dev, "rtken", GPIOD_OUT_HIGH);
    if (IS_ERR(data->rtken_gpio)) {
        dev_err(dev, "Failed to get rtken_gpio GPIO\n");
        return PTR_ERR(data->rtken_gpio);
    }
	gpiod_set_value(data->uart3v3_gpio, 0);
	gpiod_set_value(data->uarts_gpio, 0);
	gpiod_set_value(data->uartoe_gpio, 0);
	gpiod_set_value(data->kzgpio1_gpio, 0);
	gpiod_set_value(data->kzgpio2_gpio, 1);
	gpiod_set_value(data->rtken_gpio, 0);
    platform_set_drvdata(pdev, data);
	//sys/bus/platform/devices/mtk_exp_module/
    device_create_file(dev, &dev_attr_exp_module_gas);
	device_create_file(dev, &dev_attr_exp_module_rtk);
    dev_info(dev, "MTK GPIO driver probed successfully\n");
    return 0;
}

static int mtk_exp_module_remove(struct platform_device *pdev)
{
    dev_info(&pdev->dev, "MTK GPIO driver removed\n");
    return 0;
}

static const struct of_device_id mtk_exp_module_of_match[] = {
    { .compatible = "mediatek,mtk_exp_module" },
    { /* end of list */ }
};
MODULE_DEVICE_TABLE(of, mtk_exp_module_of_match);

static struct platform_driver mtk_exp_module_driver = {
    .probe = mtk_exp_module_probe,
    .remove = mtk_exp_module_remove,
    .driver = {
        .name = DRIVER_NAME,
        .of_match_table = mtk_exp_module_of_match,
    },
};

module_platform_driver(mtk_exp_module_driver);

MODULE_AUTHOR("zlw");
MODULE_DESCRIPTION("MTK exp_module Control Driver for Kernel 6.6");
MODULE_LICENSE("GPL");