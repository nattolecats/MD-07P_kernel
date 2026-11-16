
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/i2c.h>
#include <linux/irq.h>

#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>

#include "ocp2131.h"

#define I2C_ID_NAME "ocp2131"
/*static struct i2c_client *ocp2131_i2c_client;*/
static struct i2c_client *ocp2131_i2c_client;

/*****************************************************************************
 * Function Prototype
 *****************************************************************************/
static int ocp2131_probe(struct i2c_client *client);
static void ocp2131_remove(struct i2c_client *client);
/*****************************************************************************
 * Data Structure
 *****************************************************************************/

/*ocp2131 REG0 4.0v --- 6.0v, resolution:0.1v*/
const unsigned int LCM_AVDD_POSITIVE[] = {
	0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,
	0x0A,0x0B,0x0C,0x0D,0x0E,0x0F,0x10,0x11,0x12,0x13,0x14
};
/*ocp2131 REG1 -4.0v --- -6.0v, resolution:0.1v*/
const unsigned int LCM_AVEE_NEGATIVE[] = {
	0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,
	0x0A,0x0B,0x0C,0x0D,0x0E,0x0F,0x10,0x11,0x12,0x13,0x14
};

static const struct of_device_id lcm_of_match[] = {
		{.compatible = "mediatek,ocp2131"},
		{},
};


struct ocp2131_dev {
	struct i2c_client *client;

};


/*static int ocp2131_write_bytes(unsigned char addr, unsigned char value)*/
int ocp2131_write_bytes(unsigned char addr, unsigned char value)
{
	int ret = 0;
	struct i2c_client *client = ocp2131_i2c_client;
	char write_data[2] = { 0 };

	write_data[0] = addr;
	write_data[1] = value;
	ret = i2c_master_send(client, write_data, 2);
	if (ret < 0)
		printk("ocp2131 write data fail !!\n");
	return ret;
}

void ocp2131_set_avdd_voltage(unsigned int voltagelevel)
{
	int ret = 0;
	int avdd_reg = 0x00;
	printk("ocp2131_set_avdd_voltage start voltagelevel = %d LCM_AVDD_POSITIVE = %d\n",voltagelevel,LCM_AVDD_POSITIVE[voltagelevel]);
	
	ret = ocp2131_write_bytes(avdd_reg, LCM_AVDD_POSITIVE[voltagelevel]);
	if (ret < 0)
		printk("R69339----OCP2131----avdd_reg=%0x--i2c write error----\n", avdd_reg);
	else
		printk("R69339----OCP2131----avdd_reg=%0x--i2c write success----\n", avdd_reg);	
	
	printk("ocp2131_set_avdd_voltage end\n");
}

void ocp2131_set_avee_voltage(unsigned int voltagelevel)
{
	int ret = 0;
	int avee_reg = 0x01;
	printk("ocp2131_set_avee_voltage start voltagelevel = %d LCM_AVEE_NEGATIVE = %d\n",voltagelevel,LCM_AVEE_NEGATIVE[voltagelevel]);
	
	ret = ocp2131_write_bytes(avee_reg, LCM_AVEE_NEGATIVE[voltagelevel]);
	if (ret < 0)
		printk("R69339----OCP2131----avee_reg=%0x--i2c write error----\n", avee_reg);
	else
		printk("R69339----OCP2131----avee_reg=%0x--i2c write success----\n", avee_reg);	
	
	printk("ocp2131_set_avee_voltage end\n");	
}

EXPORT_SYMBOL(ocp2131_set_avdd_voltage);
EXPORT_SYMBOL(ocp2131_set_avee_voltage);

static int ocp2131_probe(struct i2c_client *client)
{
	printk("ocp2131_iic_probe\n");
	printk("TPS: info==>name=%s addr=0x%x\n", client->name, client->addr);
	ocp2131_i2c_client = client;
	return 0;
}

static void ocp2131_remove(struct i2c_client *client)
{
	printk("ocp2131_remove\n");
	ocp2131_i2c_client = NULL;
	i2c_unregister_device(client);

}

static const struct i2c_device_id ocp2131_id[] = {
	{I2C_ID_NAME, 0},
	{}
};


static struct i2c_driver ocp2131_iic_driver = {
	.id_table = ocp2131_id,
	.probe = ocp2131_probe,
	.remove = ocp2131_remove,
	/* .detect               = mt6605_detect, */
	.driver = {
		   .owner = THIS_MODULE,
		   .name = "ocp2131",
			.of_match_table = lcm_of_match,
		   },
};




static int __init ocp2131_iic_init(void)
{
	printk("ocp2131_iic_init\n");
	i2c_add_driver(&ocp2131_iic_driver);
	printk("ocp2131_iic_init success\n");
	return 0;
}

static void __exit ocp2131_iic_exit(void)
{
	printk("ocp2131_iic_exit\n");
	i2c_del_driver(&ocp2131_iic_driver);
}


module_init(ocp2131_iic_init);
module_exit(ocp2131_iic_exit);

MODULE_AUTHOR("Mike Liu");
MODULE_DESCRIPTION("MTK OCP2131 I2C Driver");
MODULE_LICENSE("GPL");

