// drivers/misc/pogo_irq_driver.c
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/kernel.h>
#include <linux/gpio/consumer.h>
struct pogo_irq_data {
    int irq;
    int gpio;
    struct work_struct work; 
};

int pogo_chr_flags = 0 ;
EXPORT_SYMBOL(pogo_chr_flags);
static irqreturn_t pogo_irq_handler(int irq, void *dev_id)
{
    struct pogo_irq_data *data = dev_id;
    if (gpio_get_value(data->gpio) == 0)
	{
     irq_set_irq_type(data->irq, IRQ_TYPE_LEVEL_HIGH);
     pogo_chr_flags = 1; 
	 printk("zlw IRQ_TYPE_LEVEL_HIGH %s\n",__func__);
	}
	else
	{
	irq_set_irq_type(data->irq, IRQ_TYPE_LEVEL_LOW);
    pogo_chr_flags = 0;	
	printk("zlw IRQ_TYPE_LEVEL_LOW %s\n",__func__);
	}

    disable_irq_nosync(data->irq);
    schedule_work(&data->work);
    printk("zlw %d %s\n", __LINE__,__func__);
    return IRQ_HANDLED;
}

static void pogo_work_handler(struct work_struct *work)
{
    struct pogo_irq_data *data = container_of(work, struct pogo_irq_data, work);

    printk("zlw Interrupt triggered on GPIO %d\n", data->gpio);
    enable_irq(data->irq);
}


static int pogo_irq_probe(struct platform_device *pdev)
{
    struct device *dev = &pdev->dev;
    struct pogo_irq_data *data;
    int ret;
    printk("%s starts\n",__func__);
    data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
    if (!data)
        return -ENOMEM;

    printk("%d %s\n", __LINE__,__func__);
	data->gpio = of_get_named_gpio(dev->of_node, "pogopin-gpios", 0);
    if (data->gpio < 0) {
    dev_err(dev, "Failed to get GPIO %d \n",__LINE__);
        return data->gpio;
    }
    gpio_direction_input(data->gpio);
    data->irq = gpio_to_irq(data->gpio);
    if (data->irq < 0) {
        dev_err(dev, "Failed to map GPIO to IRQ\n");
        return data->irq;
    }
	printk("%s gpio %d irq %d\n",__func__,data->gpio,data->irq);
    INIT_WORK(&data->work, pogo_work_handler);

    ret = devm_request_irq(dev, data->irq, pogo_irq_handler,
                          IRQF_TRIGGER_NONE | IRQF_ONESHOT,
                          "pogo_irq_device", data);
    if (ret) {
        dev_err(dev, "Failed to request IRQ\n");
        return ret;
    }
    printk("%s,end\n",__func__);
    platform_set_drvdata(pdev, data);
    return 0;
}

static const struct of_device_id pogo_irq_of_match[] = {
    { .compatible = "mediatek,pogopin" },
    {},
};
MODULE_DEVICE_TABLE(of, pogo_irq_of_match);

static struct platform_driver pogo_irq_driver = {
    .probe = pogo_irq_probe,
    .driver = {
        .name = "pogo_irq_driver",
        .of_match_table = pogo_irq_of_match,
    },
};

module_platform_driver(pogo_irq_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("zlw");
MODULE_DESCRIPTION("MTK Kernel 6.6 POGO Interrupt Driver");