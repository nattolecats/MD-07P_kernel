/*
 * Copyright (C) 2015 transsion, Inc.
 *
 * Author: peixuan qiu <peixuan.qiu@reallytek.com>
 * this module is intended for supporting module_power function separately
 * 
 */
#ifndef MODULE_POWER_H_
#define MODULE_POWER_H_

#include <linux/ioctl.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
//#include <aee.h>
#include <asm/atomic.h>
#include <asm/uaccess.h>
//#include <linux/switch.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/irq.h>


/********************control macro**************************************/
//debug
#define TAG "<module_power>"
#define MODULE_POWER_NAME "module_power"
#define MODULE_POWER_INPUT_NAME	"module_powerdata"
#define HDBG_FUNC()\
	printk(TAG"%s\n", __func__)


#define HDBG_LOG(fmt, arg...)	printk(TAG fmt, ##arg)




 
struct module_power_context {
#if defined(CONFIG_OF)
	struct device_node *irq_node;
#endif
};

#endif
