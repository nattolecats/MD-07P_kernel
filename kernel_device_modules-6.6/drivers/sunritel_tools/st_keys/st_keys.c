/*
 * st_key
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/delay.h>
#include <linux/pinctrl/consumer.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/of_address.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/platform_device.h>

//extern void aw9523b_gpio_set(int channel, int gpio_idx,int dirction, int state);
//extern void aw9523b2_gpio_set(int channel, int gpio_idx,int dirction, int state);

#define ST_KEYS_NUM 1

bool st_keys_state[ST_KEYS_NUM];
struct input_dev *st_keys_input_dev;

struct st_keys_data {
	int device_id;
	int gpio_num;
	int irq_num;
	const char *name;
	int key_code;
};

struct st_keys_data keys_data[ST_KEYS_NUM] = {
{0,0,0,0},
//{0,0,0,0},
//{0,0,0,0},
};

static const char *st_keys_name[ST_KEYS_NUM] = {
	"key_one-gpio",
//	"key_two-gpio",
//	"key_three-gpio",
};

static int st_keys_code[ST_KEYS_NUM] = {KEY_NUMERIC_9};

//static struct st_keys_data *stkeys_data;
//static struct workqueue_struct *st_keys_wq;
//static void st_keys_eint_work(struct work_struct * work);
static int stkey_code = 0;
static int stkey_state = 0;
//struct work_struct report;
static void st_keys_down_detect(int irq)
{
	irq_set_irq_type(irq, IRQF_TRIGGER_FALLING);
	//enable_irq_wake(irq);
	enable_irq(irq);
}
static void st_keys_up_detect(int irq)
{
	irq_set_irq_type(irq, IRQF_TRIGGER_RISING);
	//enable_irq_wake(irq);
	enable_irq(irq);
}
#if 0
static void st_keys_eint_work(struct work_struct * work)
{
	printk("st_keys_eint_work stkey_code:%d stkey_state:%d\n",stkey_code,stkey_state);
	if(stkey_code == 62)	//key_f4
	{
		if(stkey_state == 1)
		{
			printk("p672 led on\n");
			//aw9523b_gpio_set(1,6,1,0);		//0v	led	on
			//aw9523b2_gpio_set(1,6,1,0);		//0v	led	on
		}else{
			printk("p672 led off\n");
			//aw9523b_gpio_set(1,6,1,1);		//0v	led	off
			//aw9523b2_gpio_set(1,6,1,1);		//0v	led	off
		}
	}	
}
#endif
static irqreturn_t st_keys_irq_func(int irq, void *dev_id)
{
	struct st_keys_data *data = (struct st_keys_data *)dev_id;
	
	//queue_work(st_keys_wq, &report);	
	
	disable_irq_nosync(irq);
	st_keys_state[data->device_id] = !st_keys_state[data->device_id];
	printk("---st_key(gpio:%d,key_code:%d) detect, %s---\n", data->gpio_num,data->key_code,st_keys_state[data->device_id] ? "down" : "up");	
	stkey_code = data->key_code;
	if (st_keys_state[data->device_id]){
		stkey_state = 1;
		input_report_key(st_keys_input_dev, data->key_code, 1);
		input_sync(st_keys_input_dev);
		st_keys_up_detect(irq);
	}else {
		stkey_state = 0;
		input_report_key(st_keys_input_dev, data->key_code, 0);
		input_sync(st_keys_input_dev);		
		st_keys_down_detect(irq);
	}	
	return IRQ_HANDLED;	
	//return 0;
}



static int st_keys_irq_register(struct platform_device *pdev)
{
	int ret = 0;
	int i;
	struct device_node *np = pdev->dev.of_node; 
	int st_keys_gpio;
	int st_keys_irq_num;

	//printk("---st_keys_irq_register---start---\n");	
	for (i = 0;i < ST_KEYS_NUM;i++){
	
		st_keys_gpio = of_get_named_gpio(np, st_keys_name[i], 0);
		if (st_keys_gpio < 0) {
			printk("%s: get key_one gpio failed\r\n", __func__);
			return -EINVAL;
		}
		ret = gpio_request(st_keys_gpio,st_keys_name[i]);
		if (ret) {
			printk("%s: devm_gpio_request key_one gpio failed\r\n", __func__);
			return -EBUSY;
		}
		printk("st key one gpio_num=%d\n", st_keys_gpio);
		gpio_direction_input(st_keys_gpio);

		//gpio_set_debounce(st_keys_gpio,50 * 1000);//add debounce	
		//gpio_set_debounce_timeout(st_keys_gpio,50 * 1000);
		
		st_keys_irq_num = gpio_to_irq(st_keys_gpio);
		if (st_keys_irq_num < 0) {
			ret = st_keys_irq_num;
			printk("%s gpio to irq failed\r\n", __func__);
			goto err;
		}
		printk("st key one irq num=%d\n", st_keys_irq_num);
		
		keys_data[i].device_id = i;
		keys_data[i].gpio_num = st_keys_gpio;
		keys_data[i].irq_num = st_keys_irq_num;
		keys_data[i].name = st_keys_name[i];
		keys_data[i].key_code = st_keys_code[i];
		ret = request_irq(keys_data[i].irq_num, st_keys_irq_func, IRQF_TRIGGER_FALLING, keys_data[i].name, (void *)&keys_data[i]);
		if (ret < 0) {
			printk("%s register irq failed\r\n", __func__);
			goto err;
		}	
	}
	//printk("---st_keys_irq_register---end---\n");
	return 0;
err:
	gpio_free(st_keys_gpio);
	return ret;
}

static int st_keys_probe(struct platform_device *pdev)
{
	int err = 0;
	printk("st_keys_probe start \n");
	st_keys_input_dev = input_allocate_device();
    if (!st_keys_input_dev){
		printk("st_keys_input_dev error \n");
        return -ENOMEM;
	}
			
	st_keys_input_dev->name = "st_keys";
    st_keys_input_dev->id.bustype = BUS_HOST;
    st_keys_input_dev->id.vendor = 0x0001;
    st_keys_input_dev->id.product = 0x0001;
    st_keys_input_dev->id.version = 0x0001;  
    __set_bit(EV_KEY, st_keys_input_dev->evbit); 
	__set_bit((unsigned long)st_keys_code[0], st_keys_input_dev->keybit);
//	__set_bit(st_keys_code[1], st_keys_input_dev->keybit);
//	__set_bit(st_keys_code[2], st_keys_input_dev->keybit);
	err = input_register_device(st_keys_input_dev); 
    if (err){
		printk("%s regist input device failed!!!\n",__func__);
		input_free_device(st_keys_input_dev);
		return err;
	}

	st_keys_irq_register(pdev);
	
/*	st_keys_wq = create_singlethread_workqueue("st_keys_wq");
	if (!st_keys_wq) {    
		printk(KERN_ERR "failed to create st_keys_wq workqueue!\n");
		err =  -ENOMEM;
	}  */	
//	INIT_WORK(&report, st_keys_eint_work); 
	printk("st_keys_probe end \n");
	return 0;
}

static int st_keys_remove(struct platform_device *pdev)
{
	return 0;
}

static int __maybe_unused st_keys_suspend(struct device *dev)
{
	int i;
	for (i = 0;i < ST_KEYS_NUM;i++){
		enable_irq_wake(keys_data[i].irq_num);
	}
	return 0;
}

static int __maybe_unused st_keys_resume(struct device *dev)
{
	int i;
	for (i = 0;i < ST_KEYS_NUM;i++){
		disable_irq_wake(keys_data[i].irq_num);
	}	
    return 0;
}

static SIMPLE_DEV_PM_OPS(st_keys_pm_ops, st_keys_suspend, st_keys_resume);

static const struct of_device_id st_keys_of_match[] = {
	{.compatible = "sunritel,st_keys"},
};
static struct platform_driver st_keys_drv = {
	.probe = st_keys_probe,
	.remove = st_keys_remove,
	.driver = {
		.name = "st_keys",
		.owner = THIS_MODULE,
		.pm = &st_keys_pm_ops,
		.of_match_table = st_keys_of_match,
	},
};

static int __init st_keys_init(void)
{
	int ret;
	printk("st_keys_init start\n");
	ret = platform_driver_register(&st_keys_drv);
	if (ret) {
		printk("register driver failed (%d)\n", ret);
		return ret;
	}	
printk("st_keys_init end\n");	
	return 0;
}
static void __exit st_keys_exit(void)
{
	platform_driver_unregister(&st_keys_drv);
}

module_init(st_keys_init);
module_exit(st_keys_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("sunritel_smile");
MODULE_DESCRIPTION("sunritel keys ctrl Driver");

