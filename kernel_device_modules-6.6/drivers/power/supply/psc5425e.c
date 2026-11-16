// SPDX-License-Identifier: GPL-2.0

/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/types.h>
#include <linux/init.h>		/* For init/exit macros */
#include <linux/module.h>	/* For MODULE_ marcros  */
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#endif
//#include <mt-plat/mtk_boot.h>
//#include <mt-plat/upmu_common.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include "psc5425e.h"
#include "charger_class.h"
#include <linux/power_supply.h>
#include <linux/regulator/driver.h>
#include "mtk_charger.h"
#include <linux/pm_wakeup.h>


#define PSC5425E_TIMER_DEBUG
#ifdef PSC5425E_TIMER_DEBUG 
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/module.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/kthread.h>
//#include <mt-plat/upmu_common.h>
#include <linux/power_supply.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/machine.h>
#include <linux/iio/consumer.h>

wait_queue_head_t  psc5425e_otg_wait_que;
struct hrtimer psc5425e_otg_kthread_timer;
static int otg_set_tmr_flag =1;
int psc5425e_otg_status_flag =0;
#endif
struct wakeup_source *psc5425e_otg_wk_lock;





/**********************************************************
 *
 *   [I2C Slave Setting]
 *
 *********************************************************/

#define GETARRAYNUM(array) (ARRAY_SIZE(array))

/*psc5425e REG06 VREG[5:0]*/
const unsigned int VBAT_CV_VTH[] = {
	3856000, 3888000, 3920000, 3952000,
	3984000, 4016000, 4048000, 4080000,
	4112000, 4144000, 4176000, 4208000,
	4240000, 4272000, 4304000, 4336000,
	4368000, 4400000, 4432000, 4464000,
	4496000, 4528000, 4560000, 4592000,
	4624000

};

/*psc5425e REG04 ICHG[6:0]*/
const unsigned int CS_VTH[] = {
	697000, 836000, 1114000, 1225000,
	1534000, 1674000, 1953000, 2091000
};

/*psc5425e REG00 IINLIM[5:0]*/
const unsigned int INPUT_CS_VTH[] = {
	10000, 20000, 30000, 40000,
	50000, 60000, 70000, 80000,
	90000, 100000, 110000, 120000,
	130000, 140000, 150000, 160000,
	170000, 180000, 190000, 200000,
	210000, 220000, 230000, 250000,
	260000, 270000, 280000, 290000,
	300000, 310000, 320000
};


const unsigned int VCDT_HV_VTH[] = {
	4200000, 4250000, 4300000, 4350000,
	4400000, 4450000, 4500000, 4550000,
	4600000, 6000000, 6500000, 7000000,
	7500000, 8500000, 9500000, 10500000

};


const unsigned int VINDPM_REG[] = {
	3900, 4000, 4100, 4200, 4300, 4400,
	4500, 4600, 4700, 4800, 4900, 5000,
	5100, 5200, 5300, 5400, 5500, 5600,
	5700, 5800, 5900, 6000, 6100, 6200,
	6300, 6400
};

/* psc5425e REG0A BOOST_LIM[2:0], mA */
const unsigned int BOOST_CURRENT_LIMIT[] = {
	500, 1200
};

struct psc5425e_info {
	struct charger_device *chg_dev;
	struct charger_properties chg_props;
	struct device *dev;
	const char *chg_dev_name;
	const char *eint_name;
	int irq;
	struct regulator_dev *otg_rdev;

	struct power_supply_desc psy_desc;
	struct power_supply_config psy_cfg;
	struct power_supply *psy;
};

static struct psc5425e_info *st_info;

static DEFINE_MUTEX(g_input_current_mutex);
static struct i2c_client *new_client;
static const struct i2c_device_id psc5425e_i2c_id[] = { {"psc5425e", 0}, {} };


/**********************************************************
 *
 *   [Global Variable]
 *
 *********************************************************/
unsigned char psc5425e_reg[psc5425e_REG_NUM] = { 0 };

static DEFINE_MUTEX(psc5425e_i2c_access);
static DEFINE_MUTEX(psc5425e_access_lock);

int g_psc5425e_hw_exist;

/**********************************************************
 *
 *   [I2C Function For Read/Write psc5425e]
 *
 *********************************************************/
#ifdef CONFIG_MTK_I2C_EXTENSION
unsigned int psc5425e_read_byte(unsigned char cmd,
			       unsigned char *returnData)
{
	char cmd_buf[1] = { 0x00 };
	char readData = 0;
	int ret = 0;

	mutex_lock(&psc5425e_i2c_access);

	/* new_client->addr = ((new_client->addr) & I2C_MASK_FLAG) |
	 * I2C_WR_FLAG;
	 */
	new_client->ext_flag =
		((new_client->ext_flag) & I2C_MASK_FLAG) | I2C_WR_FLAG |
		I2C_DIRECTION_FLAG;

	cmd_buf[0] = cmd;
	ret = i2c_master_send(new_client, &cmd_buf[0], (1 << 8 | 1));
	if (ret < 0) {
		/* new_client->addr = new_client->addr & I2C_MASK_FLAG; */
		new_client->ext_flag = 0;
		mutex_unlock(&psc5425e_i2c_access);

		return 0;
	}

	readData = cmd_buf[0];
	*returnData = readData;

	/* new_client->addr = new_client->addr & I2C_MASK_FLAG; */
	new_client->ext_flag = 0;
	mutex_unlock(&psc5425e_i2c_access);

	return 1;
}

unsigned int psc5425e_write_byte(unsigned char cmd,
				unsigned char writeData)
{
	char write_data[2] = { 0 };
	int ret = 0;

	mutex_lock(&psc5425e_i2c_access);

	write_data[0] = cmd;
	write_data[1] = writeData;

	new_client->ext_flag = ((new_client->ext_flag) & I2C_MASK_FLAG) |
			       I2C_DIRECTION_FLAG;

	ret = i2c_master_send(new_client, write_data, 2);
	if (ret < 0) {
		new_client->ext_flag = 0;
		mutex_unlock(&psc5425e_i2c_access);
		return 0;
	}

	new_client->ext_flag = 0;
	mutex_unlock(&psc5425e_i2c_access);
	return 1;
}
#else
unsigned int psc5425e_read_byte(unsigned char cmd,
			       unsigned char *returnData)
{
	unsigned char xfers = 2;
	int ret, retries = 1;

	mutex_lock(&psc5425e_i2c_access);

	do {
		struct i2c_msg msgs[2] = {
			{
				.addr = new_client->addr,
				.flags = 0,
				.len = 1,
				.buf = &cmd,
			},
			{

				.addr = new_client->addr,
				.flags = I2C_M_RD,
				.len = 1,
				.buf = returnData,
			}
		};

		/*
		 * Avoid sending the segment addr to not upset non-compliant
		 * DDC monitors.
		 */
		ret = i2c_transfer(new_client->adapter, msgs, xfers);

		if (ret == -ENXIO) {
			pr_info("skipping non-existent adapter %s\n",
				new_client->adapter->name);
			break;
		}
	} while (ret != xfers && --retries);

	mutex_unlock(&psc5425e_i2c_access);

	return ret == xfers ? 1 : -1;
}

unsigned int psc5425e_write_byte(unsigned char cmd,
				unsigned char writeData)
{
	unsigned char xfers = 1;
	int ret, retries = 1;
	unsigned char buf[8];

	mutex_lock(&psc5425e_i2c_access);

	buf[0] = cmd;
	memcpy(&buf[1], &writeData, 1);

	do {
		struct i2c_msg msgs[1] = {
			{
				.addr = new_client->addr,
				.flags = 0,
				.len = 1 + 1,
				.buf = buf,
			},
		};

		/*
		 * Avoid sending the segment addr to not upset non-compliant
		 * DDC monitors.
		 */
		ret = i2c_transfer(new_client->adapter, msgs, xfers);

		if (ret == -ENXIO) {
			pr_info("skipping non-existent adapter %s\n",
				new_client->adapter->name);
			break;
		}
	} while (ret != xfers && --retries);

	mutex_unlock(&psc5425e_i2c_access);

	return ret == xfers ? 1 : -1;
}
#endif







/**********************************************************
 *
 *   [Internal Function]
 *
 *********************************************************/
static int psc5425e_dump_register(struct charger_device *chg_dev)
{

	unsigned char i = 0;
	unsigned int ret = 0;

	pr_info("[psc5425e] ");
	for (i = 0; i < psc5425e_REG_NUM; i++) {
		ret = psc5425e_read_byte(i, &psc5425e_reg[i]);
		if (ret == 0) {
			pr_info("[psc5425e] i2c transfor error\n");
			return 1;
		}
		pr_info("[0x%x]=0x%x ", i, psc5425e_reg[i]);
	}
	pr_debug("\n");
	return 0;
}





static int psc5425e_driver_probe(struct i2c_client *client);
#if 0
static unsigned int charging_value_to_parameter(const unsigned int
		*parameter, const unsigned int array_size,
		const unsigned int val)
{
	if (val < array_size)
		return parameter[val];

	pr_info("Can't find the parameter\n");
	return parameter[0];

}
#endif
static unsigned int charging_parameter_to_value(const unsigned int
		*parameter, const unsigned int array_size,
		const unsigned int val)
{
	unsigned int i;

	pr_debug_ratelimited("array_size = %d\n", array_size);

	for (i = 0; i < array_size; i++) {
		if (val == *(parameter + i))
			return i;
	}

	pr_info("NO register value match\n");
	/* TODO: ASSERT(0);    // not find the value */
	return 0;
}

static unsigned int bmt_find_closest_level(const unsigned int *pList,
		unsigned int number,
		unsigned int level)
{
	unsigned int i;
	unsigned int max_value_in_last_element;

	if (pList[0] < pList[1])
		max_value_in_last_element = 1;
	else
		max_value_in_last_element = 0;

	if (max_value_in_last_element == 1) {
		for (i = (number - 1); i != 0;
		     i--) {	/* max value in the last element */
			if (pList[i] <= level) {
				pr_debug_ratelimited("zzf_%d<=%d, i=%d\n",
					pList[i], level, i);
				return pList[i];
			}
		}

		pr_info("Can't find closest level\n");
		return pList[0];
		/* return CHARGE_CURRENT_0_00_MA; */
	} else {
		/* max value in the first element */
		for (i = 0; i < number; i++) {
			if (pList[i] <= level)
				return pList[i];
		}

		pr_info("Can't find closest level\n");
		return pList[number - 1];
		/* return CHARGE_CURRENT_0_00_MA; */
	}
}



/**********************************************************
 *
 *   [Read / Write Function]
 *
 *********************************************************/
unsigned int psc5425e_read_interface(unsigned char RegNum,
				    unsigned char *val, unsigned char MASK,
				    unsigned char SHIFT)
{
	unsigned char psc5425e_reg = 0;
	unsigned int ret = 0;

	ret = psc5425e_read_byte(RegNum, &psc5425e_reg);

	pr_debug_ratelimited("[%s] Reg[%x]=0x%x\n", __func__,
			     RegNum, psc5425e_reg);

	psc5425e_reg &= (MASK << SHIFT);
	*val = (psc5425e_reg >> SHIFT);

	pr_debug_ratelimited("[%s] val=0x%x\n", __func__, *val);

	return ret;
}

unsigned int psc5425e_config_interface(unsigned char RegNum,
				      unsigned char val, unsigned char MASK,
				      unsigned char SHIFT)
{
	unsigned char psc5425e_reg = 0;
	unsigned char psc5425e_reg_ori = 0;
	unsigned int ret = 0;

	mutex_lock(&psc5425e_access_lock);
	ret = psc5425e_read_byte(RegNum, &psc5425e_reg);

	psc5425e_reg_ori = psc5425e_reg;
	psc5425e_reg &= ~(MASK << SHIFT);
	psc5425e_reg |= (val << SHIFT);

	ret = psc5425e_write_byte(RegNum, psc5425e_reg);
	mutex_unlock(&psc5425e_access_lock);
	pr_debug_ratelimited("[%s] write Reg[%x]=0x%x from 0x%x\n", __func__,
			     RegNum,
			     psc5425e_reg, psc5425e_reg_ori);

	/* Check */
	/* psc5425e_read_byte(RegNum, &psc5425e_reg); */
	/* pr_info("[%s] Check Reg[%x]=0x%x\n", __func__,*/
	/* RegNum, psc5425e_reg); */

	return ret;
}

/* write one register directly */
unsigned int psc5425e_reg_config_interface(unsigned char RegNum,
		unsigned char val)
{
	unsigned int ret = 0;

	ret = psc5425e_write_byte(RegNum, val);

	return ret;
}

/**********************************************************
 *
 *   [Internal Function]
 *
 *********************************************************/
/* CON0---------------------------------------------------- */
void psc5425e_set_en_hiz(unsigned int val)
{
	unsigned int ret = 0;

	ret = psc5425e_config_interface((unsigned char) (psc5425e_CON0),
				       (unsigned char) (val),
				       (unsigned char) (CON0_EN_HIZ_MASK),
				       (unsigned char) (CON0_EN_HIZ_SHIFT)
				      );
	if(ret<0)
		printk("%s error!\n",__func__);
}

void psc5425e_set_iinlim(unsigned int val)
{
	unsigned int ret = 0;

	ret = psc5425e_config_interface((unsigned char) (psc5425e_CON0),
				       (unsigned char) (val),
				       (unsigned char) (CON0_IINLIM_MASK),
				       (unsigned char) (CON0_IINLIM_SHIFT)
				      );
		if(ret<0)
		printk("%s error!\n",__func__);
}

void psc5425e_set_stat_ctrl(unsigned int val)
{
	unsigned int ret = 0;

	ret = psc5425e_config_interface((unsigned char) (psc5425e_CON0),
				   (unsigned char) (val),
				   (unsigned char) (CON0_STAT_IMON_CTRL_MASK),
				   (unsigned char) (CON0_STAT_IMON_CTRL_SHIFT)
				   );
	if(ret<0)
		printk("%s error!\n",__func__);
}

/* CON1---------------------------------------------------- */

void psc5425e_set_reg_rst(unsigned int val)
{
	unsigned int ret = 0;

	ret = psc5425e_config_interface((unsigned char) (psc5425e_CON11),
				       (unsigned char) (val),
				       (unsigned char) (CON11_REG_RST_MASK),
				       (unsigned char) (CON11_REG_RST_SHIFT)
				      );
	if(ret<0)
		printk("%s error!\n",__func__);
}

void psc5425e_set_pfm(unsigned int val)
{
	unsigned int ret = 0;

	ret = psc5425e_config_interface((unsigned char) (psc5425e_CON1),
				       (unsigned char) (val),
				       (unsigned char) (CON1_PFM_MASK),
				       (unsigned char) (CON1_PFM_SHIFT)
				      );
	if(ret<0)
		printk("%s error!\n",__func__);
}

void psc5425e_set_wdt_rst(unsigned int val)
{
	unsigned int ret = 0;

	ret = psc5425e_config_interface((unsigned char) (psc5425e_CON0),
				       (unsigned char) (val),
				       (unsigned char) (CON1_WDT_RST_MASK),
				       (unsigned char) (CON1_WDT_RST_SHIFT)
				      );
	if(ret<0)
		printk("%s error!\n",__func__);
}

void psc5425e_set_otg_config(unsigned int val)
{
	unsigned int ret = 0;

	ret = psc5425e_config_interface((unsigned char) (psc5425e_CON1),
				       (unsigned char) (val),
				       (unsigned char) (CON1_OTG_CONFIG_MASK),
				       (unsigned char) (CON1_OTG_CONFIG_SHIFT)
				      );
	if(ret<0)
		printk("%s error!\n",__func__);
}

unsigned int psc5425e_get_otg_config(void)
{
	unsigned int ret = 0;
	unsigned char val = 0;

	ret = psc5425e_read_interface((unsigned char) (psc5425e_CON1),
				     (&val),
				     (unsigned char) (CON1_OTG_CONFIG_MASK),
				     (unsigned char) (CON1_OTG_CONFIG_SHIFT)
				    );
	if(ret<0)
		printk("%s error!\n",__func__);
	return val;
}

void psc5425e_set_chg_config(unsigned int val)
{
	unsigned int ret = 0;

	ret = psc5425e_config_interface((unsigned char) (psc5425e_CON1),
				       (unsigned char) (val),
				       (unsigned char) (CON1_CHG_CONFIG_MASK),
				       (unsigned char) (CON1_CHG_CONFIG_SHIFT)
				      );
	if(ret<0)
		printk("%s error!\n",__func__);
}


void psc5425e_set_sys_min(unsigned int val)
{
	unsigned int ret = 0;

	ret = psc5425e_config_interface((unsigned char) (psc5425e_CON1),
				       (unsigned char) (val),
				       (unsigned char) (CON1_SYS_MIN_MASK),
				       (unsigned char) (CON1_SYS_MIN_SHIFT)
				      );
	if(ret<0)
		printk("%s error!\n",__func__);
}
/*
void psc5425e_set_batlowv(unsigned int val)
{
	unsigned int ret = 0;

	ret = psc5425e_config_interface((unsigned char) (psc5425e_CON1),
				       (unsigned char) (val),
				       (unsigned char) (CON1_MIN_VBAT_SEL_MASK),
				       (unsigned char) (CON1_MIN_VBAT_SEL_SHIFT)
				      );
}
*/


/* CON2---------------------------------------------------- */
void psc5425e_set_rdson(unsigned int val)
{
	unsigned int ret = 0;

	ret = psc5425e_config_interface((unsigned char) (psc5425e_CON2),
				       (unsigned char) (val),
				       (unsigned char) (CON2_Q1_FULLON_MASK),
				       (unsigned char) (CON2_Q1_FULLON_SHIFT)
				      );
	if(ret<0)
		printk("%s error!\n",__func__);
}

void psc5425e_set_boost_lim(unsigned int val)
{
	unsigned int ret = 0;

	ret = psc5425e_config_interface((unsigned char) (psc5425e_CON2),
				       (unsigned char) (val),
				       (unsigned char) (CON2_BOOST_LIM_MASK),
				       (unsigned char) (CON2_BOOST_LIM_SHIFT)
				      );
	if(ret<0)
		printk("%s error!\n",__func__);
}

void psc5425e_set_ichg(unsigned int val)
{
	unsigned int ret = 0;

	ret = psc5425e_config_interface((unsigned char) (psc5425e_CON2),
				       (unsigned char) (val),
				       (unsigned char) (CON2_ICHG_MASK),
				       (unsigned char) (CON2_ICHG_SHIFT)
				      );
	if(ret<0)
		printk("%s error!\n",__func__);
}

#ifdef FIXME //this function does not exist on psc5425e
void psc5425e_set_force_20pct(unsigned int val)
{
	unsigned int ret = 0;

	ret = psc5425e_config_interface((unsigned char) (psc5425e_CON2),
				       (unsigned char) (val),
				       (unsigned char) (CON2_FORCE_20PCT_MASK),
				       (unsigned char) (CON2_FORCE_20PCT_SHIFT)
				      );
	if(ret<0)
		printk("%s error!\n",__func__);
}
#endif
/* CON3---------------------------------------------------- */

void psc5425e_set_iprechg(unsigned int val)
{
	unsigned int ret = 0;

	ret = psc5425e_config_interface((unsigned char) (psc5425e_CON3),
				       (unsigned char) (val),
				       (unsigned char) (CON3_IPRECHG_MASK),
				       (unsigned char) (CON3_IPRECHG_SHIFT)
				      );
	if(ret<0)
		printk("%s error!\n",__func__);
}

void psc5425e_set_iterm(unsigned int val)
{
	unsigned int ret = 0;

	ret = psc5425e_config_interface((unsigned char) (psc5425e_CON3),
				       (unsigned char) (val),
				       (unsigned char) (CON3_ITERM_MASK),
				       (unsigned char) (CON3_ITERM_SHIFT)
				      );

	if(ret<0)
		printk("%s error!\n",__func__);
}

/* CON4---------------------------------------------------- */

void psc5425e_set_vreg(unsigned int val)
{
	unsigned int ret = 0;

	ret = psc5425e_config_interface((unsigned char) (psc5425e_CON4),
				       (unsigned char) (val),
				       (unsigned char) (CON4_VREG_MASK),
				       (unsigned char) (CON4_VREG_SHIFT)
				      );
	if(ret<0)
		printk("%s error!\n",__func__);
}

void psc5425e_set_topoff_timer(unsigned int val)
{
	unsigned int ret = 0;

	ret = psc5425e_config_interface((unsigned char) (psc5425e_CON4),
				       (unsigned char) (val),
				       (unsigned char) (CON4_TOPOFF_TIMER_MASK),
				       (unsigned char) (CON4_TOPOFF_TIMER_SHIFT)
				      );

	if(ret<0)
		printk("%s error!\n",__func__);
}


void psc5425e_set_vrechg(unsigned int val)
{
	unsigned int ret = 0;

	ret = psc5425e_config_interface((unsigned char) (psc5425e_CON4),
				       (unsigned char) (val),
				       (unsigned char) (CON4_VRECHG_MASK),
				       (unsigned char) (CON4_VRECHG_SHIFT)
				      );
	if(ret<0)
		printk("%s error!\n",__func__);
}

/* CON5---------------------------------------------------- */

void psc5425e_set_en_term(unsigned int val)
{
	unsigned int ret = 0;

	ret = psc5425e_config_interface((unsigned char) (psc5425e_CON5),
				       (unsigned char) (val),
				       (unsigned char) (CON5_EN_TERM_MASK),
				       (unsigned char) (CON5_EN_TERM_SHIFT)
				      );
	if(ret<0)
		printk("%s error!\n",__func__);
}



void psc5425e_set_watchdog(unsigned int val)
{
	unsigned int ret = 0;

	ret = psc5425e_config_interface((unsigned char) (psc5425e_CON5),
				       (unsigned char) (val),
				       (unsigned char) (CON5_WATCHDOG_MASK),
				       (unsigned char) (CON5_WATCHDOG_SHIFT)
				      );
	if(ret<0)
		printk("%s error!\n",__func__);
}

void psc5425e_set_en_timer(unsigned int val)
{
	unsigned int ret = 0;

	ret = psc5425e_config_interface((unsigned char) (psc5425e_CON5),
				       (unsigned char) (val),
				       (unsigned char) (CON5_EN_TIMER_MASK),
				       (unsigned char) (CON5_EN_TIMER_SHIFT)
				      );

	if(ret<0)
		printk("%s error!\n",__func__);
}

void psc5425e_set_chg_timer(unsigned int val)
{
	unsigned int ret = 0;

	ret = psc5425e_config_interface((unsigned char) (psc5425e_CON5),
				       (unsigned char) (val),
				       (unsigned char) (CON5_CHG_TIMER_MASK),
				       (unsigned char) (CON5_CHG_TIMER_SHIFT)
				      );
	if(ret<0)
		printk("%s error!\n",__func__);
}

/* CON6---------------------------------------------------- */

void psc5425e_set_treg(unsigned int val)
{
#ifdef FIXME
	unsigned int ret = 0;

	ret = psc5425e_config_interface((unsigned char) (psc5425e_CON6),
				       (unsigned char) (val),
				       (unsigned char) (CON6_BOOSTV_MASK),
				       (unsigned char) (CON6_BOOSTV_SHIFT)
				      );
	if(ret<0)
		printk("%s error!\n",__func__);
#endif
}

void psc5425e_set_vindpm(unsigned int val)
{
	unsigned int ret = 0;

	ret = psc5425e_config_interface((unsigned char) (psc5425e_CON6),
				       (unsigned char) (val),
				       (unsigned char) (CON6_VINDPM_MASK),
				       (unsigned char) (CON6_VINDPM_SHIFT)
				      );

	if(ret<0)
		printk("%s error!\n",__func__);
}


void psc5425e_set_ovp(unsigned int val)
{
	unsigned int ret = 0;

	ret = psc5425e_config_interface((unsigned char) (psc5425e_CON6),
				       (unsigned char) (val),
				       (unsigned char) (CON6_OVP_MASK),
				       (unsigned char) (CON6_OVP_SHIFT)
				      );

	if(ret<0)
		printk("%s error!\n",__func__);
}

void psc5425e_set_boostv(unsigned int val)
{

	unsigned int ret = 0;

	ret = psc5425e_config_interface((unsigned char) (psc5425e_CON6),
				       (unsigned char) (val),
				       (unsigned char) (CON6_BOOSTV_MASK),
				       (unsigned char) (CON6_BOOSTV_SHIFT)
				      );
	if(ret<0)
		printk("%s error!\n",__func__);
}



/* CON7---------------------------------------------------- */

void psc5425e_set_tmr2x_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = psc5425e_config_interface((unsigned char) (psc5425e_CON7),
					(unsigned char) (val),
					(unsigned char) (CON7_TMR2X_EN_MASK),
					(unsigned char) (CON7_TMR2X_EN_SHIFT)
					);
	if(ret<0)
		printk("%s error!\n",__func__);
}

void psc5425e_set_batfet_disable(unsigned int val)
{
	unsigned int ret = 0;

	ret = psc5425e_config_interface((unsigned char) (psc5425e_CON7),
				(unsigned char) (val),
				(unsigned char) (CON7_BATFET_Disable_MASK),
				(unsigned char) (CON7_BATFET_Disable_SHIFT)
				);
	if(ret<0)
		printk("%s error!\n",__func__);
}


void psc5425e_set_batfet_delay(unsigned int val)
{
	unsigned int ret = 0;

	ret = psc5425e_config_interface((unsigned char) (psc5425e_CON7),
				       (unsigned char) (val),
				       (unsigned char) (CON7_BATFET_DLY_MASK),
				       (unsigned char) (CON7_BATFET_DLY_SHIFT)
				      );
	if(ret<0)
		printk("%s error!\n",__func__);
}

void psc5425e_set_batfet_reset_enable(unsigned int val)
{
	unsigned int ret = 0;

	ret = psc5425e_config_interface((unsigned char) (psc5425e_CON7),
				(unsigned char) (val),
				(unsigned char) (CON7_BATFET_RST_EN_MASK),
				(unsigned char) (CON7_BATFET_RST_EN_SHIFT)
				);
	if(ret<0)
		printk("%s error!\n",__func__);
}


/* CON8---------------------------------------------------- */

unsigned int psc5425e_get_system_status(void)
{
	unsigned int ret = 0;
	unsigned char val = 0;

	ret = psc5425e_read_interface((unsigned char) (psc5425e_CON8),
				     (&val), (unsigned char) (0xFF),
				     (unsigned char) (0x0)
				    );
	if(ret<0)
		printk("%s error!\n",__func__);
	return val;
}

unsigned int psc5425e_get_vbus_stat(void)
{
	unsigned int ret = 0;
	unsigned char val = 0;

	ret = psc5425e_read_interface((unsigned char) (psc5425e_CON8),
				     (&val),
				     (unsigned char) (CON8_VBUS_STAT_MASK),
				     (unsigned char) (CON8_VBUS_STAT_SHIFT)
				    );
	if(ret<0)
		printk("%s error!\n",__func__);
	return val;
}

unsigned int psc5425e_get_chrg_stat(void)
{
	unsigned int ret = 0;
	unsigned char val = 0;

	ret = psc5425e_read_interface((unsigned char) (psc5425e_CON0),
				     (&val),
				     (unsigned char) (0x03),
				     (unsigned char) (0x04)
				    );
	if(ret<0)
		printk("%s error!\n",__func__);
	return val;
}
EXPORT_SYMBOL(psc5425e_get_chrg_stat);

unsigned int psc5425e_get_vsys_stat(void)
{
	unsigned int ret = 0;
	unsigned char val = 0;

	ret = psc5425e_read_interface((unsigned char) (psc5425e_CON8),
				     (&val),
				     (unsigned char) (CON8_VSYS_STAT_MASK),
				     (unsigned char) (CON8_VSYS_STAT_SHIFT)
				    );
	if(ret<0)
		printk("%s error!\n",__func__);
	return val;
}

unsigned int psc5425e_get_pg_stat(void)
{
	unsigned int ret = 0;
	unsigned char val = 0;

	ret = psc5425e_read_interface((unsigned char) (psc5425e_CON8),
				     (&val),
				     (unsigned char) (CON8_PG_STAT_MASK),
				     (unsigned char) (CON8_PG_STAT_SHIFT)
				    );
	if(ret<0)
		printk("%s error!\n",__func__);
	return val;
}

/*CON10----------------------------------------------------------*/

void psc5425e_set_int_mask(unsigned int val)
{
	unsigned int ret = 0;

	ret = psc5425e_config_interface((unsigned char) (psc5425e_CON10),
				       (unsigned char) (val),
				       (unsigned char) (CON10_INT_MASK_MASK),
				       (unsigned char) (CON10_INT_MASK_SHIFT)
				      );
	if(ret<0)
		printk("%s error!\n",__func__);
}



/**********************************************************
 *
 *   [Internal Function]
 *
 *********************************************************/
static void psc5425e_hw_component_detect(void)
{
	unsigned int ret = 0;
	unsigned char val = 0;
	unsigned int retry;
	for (retry = 0;retry < 3;retry++){
		ret = psc5425e_read_interface(0x03, &val, 0x7, 0x5);
		printk("[%s] ret=%d,val=%d\n", __func__,ret,val);
		if (val == 0x7)
			break;
		
		mdelay(10);
	}

//	val = val & 0xf0;
	if (val == 0x7)
		g_psc5425e_hw_exist = 1;
	else
		g_psc5425e_hw_exist = 0;

	printk("[%s] exist=%d, Reg[0x03]=0x%x\n", __func__,
		g_psc5425e_hw_exist, val);
}


static int psc5425e_enable_charging(struct charger_device *chg_dev,   //ok
				   bool en)
{
	int status = 0;

	pr_info("enable state : %d\n", en);
	if (en) {

		psc5425e_config_interface(0x01,0,0x01,0x02);   //enable CHG
		printk("enable_charging");
		
	} else {
		
		psc5425e_config_interface(0x01,1,0x01,0x02);  //disable  CHG
		printk("disable_charging");

	}

	return status;
}

static int psc5425e_get_current(struct charger_device *chg_dev,  //not use 
			       u32 *ichg)
{
	unsigned int ret_val = 0;
#ifdef FIXME
	unsigned char ret_force_20pct = 0;

	/* Get current level */
	psc5425e_read_interface(psc5425e_CON2, &ret_val, CON2_ICHG_MASK,
			       CON2_ICHG_SHIFT);

	/* Get Force 20% option */
	psc5425e_read_interface(psc5425e_CON2, &ret_force_20pct,
			       CON2_FORCE_20PCT_MASK,
			       CON2_FORCE_20PCT_SHIFT);

	/* Parsing */
	ret_val = (ret_val * 64) + 512;

#endif
	return ret_val;
}

static int psc5425e_set_current(struct charger_device *chg_dev,    //ok
			       u32 current_value)
{
	unsigned int status = true;
	unsigned int set_chr_current;
	unsigned int array_size;
	unsigned int register_value;

	printk("&&&& charge_current_value = %d\n", current_value);
	//current_value /= 10;     //500000
	array_size = GETARRAYNUM(CS_VTH);
	set_chr_current = bmt_find_closest_level(CS_VTH, array_size,
			  current_value);
	register_value = charging_parameter_to_value(CS_VTH, array_size,
			 set_chr_current);
	//register_value=0x07;
	psc5425e_config_interface(0x04,register_value,0x07,0x04);
	printk("psc5425e_set_current current_value=%d\n register_value=%d ",current_value,register_value);

	return status;
}

static int psc5425e_get_input_current(struct charger_device *chg_dev,   //not CG
				     u32 *aicr)
{
	int ret = 0;
#ifdef FIXME
	unsigned char val = 0;

	psc5425e_read_interface(psc5425e_CON1, &val, CON0_IINLIM_MASK,
			       CON0_IINLIM_SHIFT);
	ret = (int)val;
	*aicr = INPUT_CS_VTH[val];
#endif
	return ret;
}


static int psc5425e_set_input_current(struct charger_device *chg_dev,    //ok
				     u32 current_value)
{
	unsigned int status = true;
	unsigned int register_value;


	if (current_value <= 500000)
	{
		register_value=0x01;  //limit  500ma 
	}
	else {
		register_value=0x03; //input  no limit
	}
	
	psc5425e_config_interface(0x01,register_value,0x03,0x06);
	printk("psc5425e_set_input_current current_value=%d\n register_value=%d ",current_value,register_value);

	return status;
}

static int psc5425e_set_cv_voltage(struct charger_device *chg_dev,  //ok
				  u32 cv)
{
	unsigned int status = true;
	unsigned short register_value = 0;


	
	if (cv <= 4100000 ){
		
		register_value=0x0;  //OREG  4.1v
	}
	else if (cv > 4100000  &&  cv <= 4200000 ){
		
		register_value=0x02;  //OREG  4.2v
	}
	else if (cv > 4200000  &&  cv <= 4350000 ){
		
		register_value=0x24;  //OREG  4.35v
	}
	else if (cv > 4350000 ){
		
		register_value=0x2d;  //OREG  4.4v
	}
	
	psc5425e_config_interface(0x02,register_value,0x3f,0x02);
	printk("psc5425e_set_current cv=%d\n register_value=%d ",cv,register_value);

	return status;
}

static int psc5425e_reset_watch_dog_timer(struct charger_device
		*chg_dev)
{
	unsigned int status = true;

	pr_info("charging_reset_watch_dog_timer\n");

	psc5425e_set_wdt_rst(0x1);	/* Kick watchdog */
	psc5425e_set_watchdog(0x3);	/* WDT 160s */

	return status;
}



static int psc5425e_get_charging_status(struct charger_device *chg_dev,
				       bool *is_done)
{
	unsigned int status = true;
	unsigned int ret_val;

	ret_val = psc5425e_get_chrg_stat();

	if (ret_val == 0x2)
		*is_done = true;
	else
		*is_done = false;

	return status;
}


static int psc5425e_enable_otg(struct charger_device *chg_dev, bool en)
{
	int ret = 0;

	printk("%s en = %d\n", __func__, en);
	if (en) {
		psc5425e_config_interface(0x01,01,0x01,0x00); //enable OTG 
		printk("enable OTG  ");
	} else {
		psc5425e_config_interface(0x01,0,0x01,0x00); //disable OTG 
		printk("disable OTG  ");
	}
	return ret;
}




static unsigned int charging_hw_init(void)  //ok
{
	unsigned int status = 0;


	psc5425e_config_interface(0x01,0xf8,0xff,0x00); //0x01 0xf8 -> 0xf0  bit3 disable terminl charge detect
	psc5425e_config_interface(0x02,0x08,0xff,0x00); //OREG 4.2V
	psc5425e_config_interface(0x04,0x73,0xff,0x00);
	psc5425e_config_interface(0x05,0x23,0xff,0x00);
	
	pr_info("%s: hw_init down!\n", __func__);
	return status;
}

static int psc5425e_parse_dt(struct psc5425e_info *info,
			    struct device *dev)
{
	struct device_node *np = dev->of_node;
	//int psc5425e_en_pin = 0;

	pr_info("%s\n", __func__);
	if (!np) {
		pr_info("%s: no of node\n", __func__);
		return -ENODEV;
	}

	if (of_property_read_string(np, "charger_name",
				    &info->chg_dev_name) < 0) {
		info->chg_dev_name = "primary_chg";
		pr_info("%s: no charger name\n", __func__);
	}

	if (of_property_read_string(np, "alias_name",
				    &(info->chg_props.alias_name)) < 0) {
		info->chg_props.alias_name = "psc5425e";
		pr_info("%s: no alias name\n", __func__);
	}

	return 0;
}

static int psc5425e_do_event(struct charger_device *chg_dev, u32 event,
			    u32 args)
{
	struct psc5425e_info *info = NULL;

	if (chg_dev == NULL)
		return -EINVAL;
	pr_info("%s: event = %d\n", __func__, event);

	info = (struct psc5425e_info *)dev_get_drvdata(&chg_dev->dev);
	if (info == NULL)
		return -EINVAL;

	switch (event) {
	case EVENT_FULL:
		charger_dev_notify(chg_dev, CHARGER_DEV_NOTIFY_EOC);
		power_supply_changed(info->psy);
		break;
	case EVENT_RECHARGE:
		charger_dev_notify(chg_dev, CHARGER_DEV_NOTIFY_RECHG);
		power_supply_changed(info->psy);
		break;
	default:
		break;
	}
	return 0;
}

static struct charger_ops psc5425e_chg_ops = {
#ifdef FIXME
	.enable_hz = psc5425e_enable_hz,
#endif

	/* Normal charging */
	.dump_registers = psc5425e_dump_register,
	.enable = psc5425e_enable_charging,
	.get_charging_current = psc5425e_get_current,
	.set_charging_current = psc5425e_set_current,
	.get_input_current = psc5425e_get_input_current,
	.set_input_current = psc5425e_set_input_current,
	.set_constant_voltage = psc5425e_set_cv_voltage,
	.kick_wdt = psc5425e_reset_watch_dog_timer,
	.is_charging_done = psc5425e_get_charging_status,


	/* OTG */
	.enable_otg = psc5425e_enable_otg,
	//.set_boost_current_limit = psc5425e_set_boost_current_limit,
	.event = psc5425e_do_event,
};

static enum power_supply_property psc5425e_psy_properties[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_USB_TYPE,
};

static enum power_supply_usb_type psc5425e_usb_types[] = {
	POWER_SUPPLY_USB_TYPE_UNKNOWN,
	POWER_SUPPLY_USB_TYPE_SDP,
	POWER_SUPPLY_USB_TYPE_DCP,
	POWER_SUPPLY_USB_TYPE_CDP,
};

static int psy_psc5425e_get_property(struct power_supply *psy,
	enum power_supply_property psp, union power_supply_propval *val)
{
	struct psc5425e_info *info = NULL;
	struct power_supply *chg_type_psy = NULL;
	unsigned int ret_val;

	info = (struct psc5425e_info *)power_supply_get_drvdata(psy);
	if (info == NULL) {
		pr_info("%s get info fail\n", __func__);
		return -EINVAL;
	}

	chg_type_psy = devm_power_supply_get_by_phandle(info->dev, "charger");
	if (IS_ERR_OR_NULL(chg_type_psy)) {
		pr_info("%s get chg_type_psy fail\n", __func__);
		return -EINVAL;
	}

	pr_info("%s psp:%d\n", __func__, psp);
	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		ret_val = psc5425e_get_chrg_stat();
		if (ret_val == 0)
			val->intval = POWER_SUPPLY_STATUS_NOT_CHARGING;
		else if (ret_val == 1)
			val->intval = POWER_SUPPLY_STATUS_CHARGING;
		else if (ret_val == 2)
			val->intval = POWER_SUPPLY_STATUS_FULL;
		else
			return -EINVAL;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		power_supply_get_property(chg_type_psy, POWER_SUPPLY_PROP_ONLINE, val);
		break;
	case POWER_SUPPLY_PROP_USB_TYPE:
		power_supply_get_property(chg_type_psy, POWER_SUPPLY_PROP_USB_TYPE, val);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static char *psc5425e_supplied_to[] = {
	"battery"
};

#ifdef PSC5425E_TIMER_DEBUG 
void _wake_up_psc5425e_otg(void)
{
	otg_set_tmr_flag =1;
	wake_up(&psc5425e_otg_wait_que);
}

static enum hrtimer_restart otg_rst_timer_func(struct hrtimer *timer)
{
	_wake_up_psc5425e_otg();
	return HRTIMER_NORESTART;
}

void psc5425e_otg_init_timer(void)
{
	ktime_t ktime = ktime_set(10, 0);
	hrtimer_init(&psc5425e_otg_kthread_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	psc5425e_otg_kthread_timer.function = otg_rst_timer_func;
	hrtimer_start(&psc5425e_otg_kthread_timer, ktime, HRTIMER_MODE_REL);
}

void psc5425e_otg_start_timer(void)
{

	ktime_t ktime = ktime_set(10, 0);
	hrtimer_start(&psc5425e_otg_kthread_timer, ktime, HRTIMER_MODE_REL);
}

int psc5425e_otg_routine_thread(void *arg)
{
	while (1) {
		wait_event(psc5425e_otg_wait_que, (otg_set_tmr_flag == 1));
		otg_set_tmr_flag =0;
		if(psc5425e_otg_status_flag ==1)
		{
			psc5425e_dump_register(st_info->chg_dev);
			psc5425e_otg_start_timer();		
		}
	}
	
	return 0;
}
#endif

static int psc5425e_enable_vbus(struct regulator_dev *rdev)
{
	
	//printk("---%s---\n",__func__);
#ifdef PSC5425E_TIMER_DEBUG 
	psc5425e_otg_status_flag = 1;
	psc5425e_otg_start_timer();
#endif
	__pm_stay_awake(psc5425e_otg_wk_lock);
	psc5425e_set_otg_config(1); //enable OTG 
	printk("psc5425e enable OTG");


	return 0;
}

static int psc5425e_disable_vbus(struct regulator_dev *rdev)
{
	//printk("---%s---\n",__func__);	
	__pm_relax(psc5425e_otg_wk_lock);
	psc5425e_set_otg_config(0); //disable OTG 
	psc5425e_otg_status_flag = 0;
	printk("psc5425e disable OTG");

	return 0;
}

static int psc5425e_is_enabled_vbus(struct regulator_dev *rdev)
{
	int status;
	
	status = psc5425e_get_otg_config();
	//printk("---%s--%d-\n",__func__,status);
	return status;
}

static const struct regulator_ops psc5425e_vbus_ops = {
	.enable = psc5425e_enable_vbus,
	.disable = psc5425e_disable_vbus,
	.is_enabled = psc5425e_is_enabled_vbus,
};

static const struct regulator_desc psc5425e_otg_rdesc = {
	.of_match = "usb-otg-vbus",
	.name = "usb-otg-vbus",
	.ops = &psc5425e_vbus_ops,
	.owner = THIS_MODULE,
	.type = REGULATOR_VOLTAGE,
	.fixed_uV = 5000000,
	.n_voltages = 1,
};


static int psc5425e_driver_probe(struct i2c_client *client)
{
	int ret = 0;
	struct psc5425e_info *info = NULL;
	struct regulator_config config = { };

	pr_info("[%s]\n", __func__);

	info = devm_kzalloc(&client->dev, sizeof(struct psc5425e_info),
			    GFP_KERNEL);
	if (!info)
		return -ENOMEM;
	client->addr = 0x6a;
	new_client = client;
	info->dev = &client->dev;

	ret = psc5425e_parse_dt(info, &client->dev);
	if (ret < 0)
		return ret;

	psc5425e_hw_component_detect();
	if(g_psc5425e_hw_exist == 0){
		return -1;
	}
	charging_hw_init();

	/* Register charger device */
	info->chg_dev = charger_device_register(info->chg_dev_name,
						&client->dev, info,
						&psc5425e_chg_ops,
						&info->chg_props);
	if (IS_ERR_OR_NULL(info->chg_dev)) {
		pr_info("%s: register charger device  failed\n", __func__);
		ret = PTR_ERR(info->chg_dev);
		return ret;
	}

	/* otg regulator */
	config.dev = info->dev;
	config.driver_data = info;
	info->otg_rdev = devm_regulator_register(info->dev,
						&psc5425e_otg_rdesc, &config);
	if (IS_ERR(info->otg_rdev)) {
		ret = PTR_ERR(info->otg_rdev);
		pr_info("%s: register otg regulator failed (%d)\n", __func__, ret);
		return ret;
	}

	info->psy_desc.name = "charger_psc5425e";
	info->psy_desc.type = POWER_SUPPLY_TYPE_UNKNOWN;
	info->psy_desc.properties = psc5425e_psy_properties;
	info->psy_desc.num_properties = ARRAY_SIZE(psc5425e_psy_properties);
	info->psy_desc.get_property = psy_psc5425e_get_property;
	info->psy_desc.usb_types = psc5425e_usb_types,
	info->psy_desc.num_usb_types = ARRAY_SIZE(psc5425e_usb_types),

	info->psy_cfg.drv_data = info;
	info->psy_cfg.of_node = client->dev.of_node;
	info->psy_cfg.supplied_to = psc5425e_supplied_to;
	info->psy_cfg.num_supplicants = ARRAY_SIZE(psc5425e_supplied_to);

	info->psy = power_supply_register(info->dev, &info->psy_desc, &info->psy_cfg);
//	if (IS_ERR(info->psy))
//		chr_err("register psy fail:%d\n", PTR_ERR(info->psy));

#ifdef PSC5425E_TIMER_DEBUG //add by esky_liml_2018_02_23
	init_waitqueue_head(&psc5425e_otg_wait_que);
	psc5425e_otg_init_timer();
	kthread_run(psc5425e_otg_routine_thread, 0, "psc5425e_otg_thread");
#endif
	st_info = info;
	psc5425e_dump_register(info->chg_dev);

	
	psc5425e_otg_wk_lock = wakeup_source_register(NULL, "psc5425e_otg_wk_wakelock");

	return 0;
}

/**********************************************************
 *
 *   [platform_driver API]
 *
 *********************************************************/
unsigned char g_reg_value_psc5425e;
static ssize_t psc5425e_access_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	pr_info("[%s] 0x%x\n", __func__, g_reg_value_psc5425e);
	return sprintf(buf, "%u\n", g_reg_value_psc5425e);
}

static ssize_t psc5425e_access_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t size)
{
	int ret = 0;
	char *pvalue = NULL, *addr, *val;
	unsigned int reg_value = 0;
	unsigned int reg_address = 0;

	pr_info("[%s]\n", __func__);

	if (buf != NULL && size != 0) {
		pr_info("[%s] buf is %s and size is %zu\n", __func__, buf,
			size);

		pvalue = (char *)buf;
		if (size > 3) {
			addr = strsep(&pvalue, " ");
			ret = kstrtou32(addr, 16,
				(unsigned int *)&reg_address);
		} else
			ret = kstrtou32(pvalue, 16,
				(unsigned int *)&reg_address);

		if (size > 3) {
			val = strsep(&pvalue, " ");
			ret = kstrtou32(val, 16, (unsigned int *)&reg_value);
			pr_info(
			"[%s] write psc5425e reg 0x%x with value 0x%x !\n",
			__func__,
			(unsigned int) reg_address, reg_value);
			ret = psc5425e_config_interface(reg_address,
				reg_value, 0xFF, 0x0);
		} else {
			ret = psc5425e_read_interface(reg_address,
					     &g_reg_value_psc5425e, 0xFF, 0x0);
			pr_info(
			"[%s] read psc5425e reg 0x%x with value 0x%x !\n",
			__func__,
			(unsigned int) reg_address, g_reg_value_psc5425e);
			pr_info(
			"[%s] use \"cat psc5425e_access\" to get value\n",
			__func__);
		}
	}
	if(ret<0)
		printk("%s error!\n",__func__);
	return size;
}

static DEVICE_ATTR_RW(psc5425e_access);

static int psc5425e_user_space_probe(struct platform_device *dev)
{
	int ret_device_file = 0;

	pr_info("******** %s!! ********\n", __func__);

	ret_device_file = device_create_file(&(dev->dev),
					     &dev_attr_psc5425e_access);
	if(ret_device_file<0)
	printk("%s error!\n",__func__);

	return 0;
}

struct platform_device psc5425e_user_space_device = {
	.name = "psc5425e-user",
	.id = -1,
};

static struct platform_driver psc5425e_user_space_driver = {
	.probe = psc5425e_user_space_probe,
	.driver = {
		.name = "psc5425e-user",
	},
};

#ifdef CONFIG_OF
static const struct of_device_id psc5425e_of_match[] = {
	{.compatible = "mediatek,psc5425e"},
	{},
};
#else
static struct i2c_board_info i2c_psc5425e __initdata = {
	I2C_BOARD_INFO("psc5425e", (psc5425e_SLAVE_ADDR_WRITE >> 1))
};
#endif

static struct i2c_driver psc5425e_driver = {
	.driver = {
		.name = "psc5425e",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = psc5425e_of_match,
#endif
	},
	.probe = psc5425e_driver_probe,
	.id_table = psc5425e_i2c_id,
};

static int __init psc5425e_init(void)
{
	int ret = 0;

	/* i2c registration using DTS instead of boardinfo*/
#ifdef CONFIG_OF
	pr_info("[%s] init start with i2c DTS", __func__);
#else
	pr_info("[%s] init start. ch=%d\n", __func__, psc5425e_BUSNUM);
	i2c_register_board_info(psc5425e_BUSNUM, &i2c_psc5425e, 1);
#endif
	if (i2c_add_driver(&psc5425e_driver) != 0) {
		pr_info(
			"[%s] failed to register psc5425e i2c driver.\n",
			__func__);
	} else {
		pr_info(
			"[%s] Success to register psc5425e i2c driver.\n",
			__func__);
	}

	/* psc5425e user space access interface */
	ret = platform_device_register(&psc5425e_user_space_device);
	if (ret) {
		pr_info("****[%s] Unable to device register(%d)\n", __func__,
			ret);
		return ret;
	}
	ret = platform_driver_register(&psc5425e_user_space_driver);
	if (ret) {
		pr_info("****[%s] Unable to register driver (%d)\n", __func__,
			ret);
		return ret;
	}

	return 0;
}

static void __exit psc5425e_exit(void)
{
	i2c_del_driver(&psc5425e_driver);
}
//module_init(psc5425e_init);
late_initcall(psc5425e_init);
module_exit(psc5425e_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("I2C psc5425e Driver");
MODULE_AUTHOR("will cai <will.cai@mediatek.com>");
