/*
 * Copyright (C) 2022 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/types.h>
#include <linux/init.h>		/* For init/exit macros */
#include <linux/module.h>	/* For MODULE_ marcros  */
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/irq.h>
#include <linux/kthread.h>
#include <linux/reboot.h>
#include <linux/power_supply.h>
#include <linux/phy/phy.h>
#include <linux/errno.h>
#include "charger_class.h"
//#include <mt-plat/mtk_boot.h>
//#include <mt-plat/upmu_common.h>
//#include <mt-plat/charger_type.h>
#ifdef CONFIG_RT_REGMAP
#include <mt-plat/rt-regmap.h>
#endif
//#include <mtk_musb.h>
//#include <charger_class.h>
#include "mtk_charger.h"
//#include <mt-plat/charger_type.h>
//#include "mtk_charger_intf.h"
#include "sd76030.h"
//#define CONFIG_MTK_EXTERNAL_CHARGER_TYPE_DETECT

#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/machine.h>
#include <linux/iio/consumer.h>


#define sd76030_info(fmt, args...) pr_err("[bmt-sd76030] %s: "fmt, __func__, ## args)

enum charger_type
{
	CHARGER_UNKNOWN = POWER_SUPPLY_USB_TYPE_UNKNOWN,
	STANDARD_HOST = POWER_SUPPLY_USB_TYPE_SDP,		/* USB : 450mA */
	CHARGING_HOST = POWER_SUPPLY_USB_TYPE_CDP,		/* USB : 1.5A */
	NONSTANDARD_CHARGER = POWER_SUPPLY_USB_TYPE_DCP,	/* AC : 450mA~1A */
	STANDARD_CHARGER = POWER_SUPPLY_USB_TYPE_DCP,		/* AC : ~1A */
};

enum sd76030_port_stat
{
	SD76030_PORTSTAT_NO_INPUT = 0,
	SD76030_PORTSTAT_SDP,
	SD76030_PORTSTAT_CDP,
	SD76030_PORTSTAT_DCP,
	SD76030_PORTSTAT_HVDCP,
	SD76030_PORTSTAT_UNKNOWN,
	SD76030_PORTSTAT_NON_STANDARD,
	SD76030_PORTSTAT_OTG,
	SD76030_PORTSTAT_MAX,
};

struct sd76030_chip
{
	struct charger_device *chg_dev;
	struct charger_properties chg_props;
	struct device *dev;
	struct i2c_client * client;
	struct mutex i2c_rw_lock;
	struct mutex adc_lock;
	struct mutex irq_lock;
	const char *chg_dev_name;
	struct delayed_work sd76030_work;
#ifdef CONFIG_MTK_EXTERNAL_CHARGER_TYPE_DETECT
	struct mutex bc12_lock;
	struct delayed_work psy_dwork;
	atomic_t vbus_gd;
	bool attach;
	enum sd76030_port_stat port;
	enum charger_type chg_type;
	struct power_supply *psy;
	wait_queue_head_t bc12_en_req;
	atomic_t bc12_en_req_cnt;
	struct task_struct *bc12_en_kthread;
	struct delayed_work sd76030_bc12_check_work;
#endif /* CONFIG_MTK_EXTERNAL_CHARGER_TYPE_DETECT */
	struct power_supply_desc psy_desc;
	struct power_supply * sd76030_psy;
	int32_t component_id;
	int32_t irq_gpio;
	int32_t irq;
	int32_t ce_gpio;
	uint32_t chg_state;

	uint32_t ilimit_ma;
	uint32_t vlimit_mv;
	uint32_t cc_ma;
	uint32_t cv_mv;
	uint32_t pre_ma;
	uint32_t eoc_ma;
	uint32_t rechg_mv;
	uint32_t vsysmin_mv;
	uint32_t target_hv;
	int32_t  vbat0_flag;
	
	struct regulator_dev *otg_rdev;
};

struct sd76030_chip *g_sd76030_chip;

struct sd76030_irq_info
{
	const char	*irq_name;
	int32_t	(*irq_func)(struct sd76030_chip *chip,uint8_t rt_stat);
	uint8_t 	irq_mask;
	uint8_t	irq_shift;
};

struct sd76030_irq_handle
{
	uint8_t reg;
	uint8_t value;
	uint8_t pre_value;
	struct sd76030_irq_info irq_info[5];
};

const static char* vbus_type[] =
{
	"no input",
	"sdp",
	"cdp",
	"dcp",
	"hvdcp",
	"unknown",
	"non-standard",
	"otg"
};

const static char* charge_state[] =
{
	"not charging",
	"pre-charge",
	"fast charging",
	"charge termination"
};

const static char* adc_channel[] =
{
	"vbat",
	"vsys",
	"tbat",
	"vbus",
	"ibat",
	"ibus",
};

static int32_t sd76030_read_byte(struct sd76030_chip *chip, uint8_t reg, uint8_t *data)
{
	int32_t ret = 0;
	int32_t i = 0;
	
	mutex_lock(&chip->i2c_rw_lock);
	for(i=0; i<4; i++)
	{
		ret = i2c_smbus_read_byte_data(chip->client, reg);
		if(ret >= 0)
			break;
		else
			dev_err(chip->dev, "%s: reg[0x%02X] err %d, %d times\n",__func__, reg, ret, i);
	}
	mutex_unlock(&chip->i2c_rw_lock);
	if(i >= 4)
	{
		*data = 0;
		return -1;
	}
	*data = (uint8_t)ret;

	return 0;
}

static int32_t sd76030_write_byte(struct sd76030_chip *chip, uint8_t reg, uint8_t val)
{
	int32_t ret = 0;
	int32_t i = 0;
	
	mutex_lock(&chip->i2c_rw_lock);
	for(i=0; i<4; i++)
	{
		ret = i2c_smbus_write_byte_data(chip->client, reg, val);
		if(ret >= 0)
			break;
		else
			dev_err(chip->dev, "%s: reg[0x%02X] err %d, %d times\n",__func__, reg, ret, i);
	}
	mutex_unlock(&chip->i2c_rw_lock);
	return (i>=4) ? -1 : 0;
}

static int32_t sd76030_update(struct sd76030_chip *chip, uint8_t RegNum, uint8_t val, uint8_t MASK,uint8_t SHIFT)
{
	uint8_t data = 0;
	int32_t ret = 0;

	ret = sd76030_read_byte(chip,RegNum, &data);

	if(ret < 0)
		return ret;

	if((data & (MASK << SHIFT)) != (val << SHIFT))
	{
		data &= ~(MASK << SHIFT);
		data |= (val << SHIFT);
		return sd76030_write_byte(chip,RegNum, data);
	}
	return 0;
}
/**********************************************************
 *
 *   [Internal Function]
 *
 *********************************************************/
 
static int sd76030_enable_hz(struct charger_device *chg_dev, bool en)
{
	struct sd76030_chip *chip = dev_get_drvdata(&chg_dev->dev);

	return sd76030_update(chip,SD76030_R00,!!en,CON0_EN_HIZ_MASK,CON0_EN_HIZ_SHIFT);
}
static int32_t sd76030_set_en_hiz(struct sd76030_chip *chip, int32_t enable)
{
	return sd76030_update(chip,SD76030_R00,!!enable,CON0_EN_HIZ_MASK,CON0_EN_HIZ_SHIFT);
}
static int32_t sd76030_get_en_hiz(struct sd76030_chip *chip)
{
	uint8_t data_reg = 0;
	int32_t ret = 0;

	ret = sd76030_read_byte(chip,SD76030_R00,&data_reg);
	if(ret < 0)
		return ret;

	data_reg = (data_reg & (CON0_EN_HIZ_MASK << CON0_EN_HIZ_SHIFT)) >> CON0_EN_HIZ_SHIFT;	
	return !!data_reg;
}

static int32_t sd76030_set_input_curr_lim(struct sd76030_chip *chip, uint32_t ilimit_ma)
{
	uint8_t  data_reg = 0;
	uint32_t input_current = 0; 

	if (ilimit_ma < INPUT_CURRT_MIN)
		input_current = INPUT_CURRT_MIN;
	else if (ilimit_ma > INPUT_CURRT_MAX)
		input_current = INPUT_CURRT_MAX;
	else
		input_current = ilimit_ma;

	data_reg = (input_current - INPUT_CURRT_MIN) / INPUT_CURRT_STEP;

	return sd76030_update(chip,SD76030_R00,data_reg,CON0_IINLIM_MASK,CON0_IINLIM_SHIFT);
}
static int32_t sd76030_get_input_curr_lim(struct sd76030_chip *chip)
{
	uint8_t data_reg = 0;
	int32_t ret = 0;

	ret = sd76030_read_byte(chip,SD76030_R00,&data_reg);
	if(ret < 0)
		return ret;

	data_reg = (data_reg & (CON0_IINLIM_MASK << CON0_IINLIM_SHIFT)) >> CON0_IINLIM_SHIFT;

	return (data_reg * INPUT_CURRT_STEP + INPUT_CURRT_MIN);
}

static int32_t sd76030_get_iterm(struct sd76030_chip *chip)
{
	uint8_t data_reg = 0;
	int32_t ret = 0;

	ret = sd76030_read_byte(chip,SD76030_R03,&data_reg);
	if(ret < 0)
		return ret;

	data_reg = (data_reg & (CON3_ITERM_MASK<< CON3_ITERM_SHIFT)) >> CON3_ITERM_SHIFT;

	return (data_reg * EOC_CURRT_STEP + EOC_CURRT_MIN);
}
static int32_t sd76030_set_iterm(struct sd76030_chip *chip, uint32_t iterm)
{
	uint8_t data_reg = 0;
	uint32_t data_temp = 0;

	if (iterm < EOC_CURRT_MIN)
		data_temp = EOC_CURRT_MIN;
	else if (iterm > EOC_CURRT_MAX)
		data_temp = EOC_CURRT_MAX;
	else
		data_temp = iterm;

	data_reg = (data_temp - EOC_CURRT_MIN)/ EOC_CURRT_STEP;

	return sd76030_update(chip,SD76030_R03,data_reg,CON3_ITERM_MASK,CON3_ITERM_SHIFT);
}

static int32_t sd76030_set_wd_reset(struct sd76030_chip *chip)
{
	return sd76030_update(chip,SD76030_R01,1,CON1_WD_MASK,CON1_WD_SHIFT);
}

static int32_t sd76030_set_otg_en(struct sd76030_chip *chip, int32_t enable)
{

	return sd76030_update(chip,SD76030_R01,!!enable,CON1_OTG_CONFIG_MASK,CON1_OTG_CONFIG_SHIFT);
}
static int32_t sd76030_get_otg_config(struct sd76030_chip *chip)
{
	uint8_t data_reg = 0;
	int32_t ret = 0;

	ret = sd76030_read_byte(chip,SD76030_R01,&data_reg);
	if(ret < 0)
		return ret;

	data_reg = (data_reg & (CON1_OTG_CONFIG_MASK << CON1_OTG_CONFIG_SHIFT)) >> CON1_OTG_CONFIG_SHIFT;

	return !!data_reg;
}

static int32_t sd76030_set_charger_en(struct sd76030_chip *chip, int32_t enable)
{
	return sd76030_update(chip,SD76030_R01,!!enable,CON1_CHG_CONFIG_MASK,CON1_CHG_CONFIG_SHIFT);
}
static int32_t sd76030_get_charger_en(struct sd76030_chip *chip)
{
	uint8_t data_reg = 0;
	int32_t ret = 0;

	ret = sd76030_read_byte(chip,SD76030_R01,&data_reg);
	if(ret < 0)
		return ret;

	data_reg = (data_reg & (CON1_CHG_CONFIG_MASK << CON1_CHG_CONFIG_SHIFT)) >> CON1_CHG_CONFIG_SHIFT;

	return !!data_reg;
}

static int32_t sd76030_set_sys_min_volt(struct sd76030_chip *chip, uint32_t vsys_mv)
{
	uint8_t  data_reg = 0;
	uint32_t vsys = 0; 

	if (vsys_mv < CON1_SYS_V_LIMIT_MIN)
		vsys = CON1_SYS_V_LIMIT_MIN;
	else if (vsys_mv > CON1_SYS_V_LIMIT_MAX)
		vsys = CON1_SYS_V_LIMIT_MAX;
	else
		vsys = vsys_mv;

	data_reg = (vsys - CON1_SYS_V_LIMIT_MIN) / CON1_SYS_V_LIMIT_STEP;

	return sd76030_update(chip,SD76030_R01,data_reg,CON1_SYS_V_LIMIT_MASK,CON1_SYS_V_LIMIT_SHIFT);
}

static int32_t sd76030_set_boost_ilim(struct sd76030_chip *chip, uint32_t boost_ilimit)
{
	uint8_t data_reg = 0;
	
	if(boost_ilimit <= 500)			
		data_reg = 0x00;
	else 
		data_reg = 0x01;

	return sd76030_update(chip,SD76030_R02,data_reg,CON2_BOOST_ILIM_MASK,CON2_BOOST_ILIM_SHIFT);
}
static int32_t sd76030_get_boost_ilim(struct sd76030_chip *chip)
{
	uint8_t data_reg = 0;
	int32_t ret = 0;

	ret = sd76030_read_byte(chip,SD76030_R02,&data_reg);
	if(ret < 0)
		return ret;

	data_reg = (data_reg & (CON2_BOOST_ILIM_MASK << CON2_BOOST_ILIM_SHIFT)) >> CON2_BOOST_ILIM_SHIFT;

	return (1 == data_reg) ? 1200 : 500;
}
static int32_t sd76030_set_ichg_current(struct sd76030_chip *chip, uint32_t ichg)
{
	uint8_t data_reg = 0;
	uint32_t data_temp = 0; 

	if (ichg < ICHG_CURR_MIN)
		data_temp = ICHG_CURR_MIN;
	else if (ichg > ICHG_CURR_MAX)
		data_temp = ICHG_CURR_MAX;
	else
		data_temp = ichg;

	data_reg = (data_temp - ICHG_CURR_MIN)/ ICHG_CURR_STEP;

	return sd76030_update(chip,SD76030_R02,data_reg,CON2_ICHG_MASK,CON2_ICHG_SHIFT);
}
static int32_t sd76030_get_ichg_current(struct sd76030_chip *chip)
{
	uint8_t data_reg = 0;
	int32_t ret = 0;

	ret = sd76030_read_byte(chip,SD76030_R02,&data_reg);
	if(ret < 0)
		return ret;

	data_reg = (data_reg & (CON2_ICHG_MASK << CON2_ICHG_SHIFT)) >> CON2_ICHG_SHIFT;

	return (data_reg * ICHG_CURR_STEP + ICHG_CURR_MIN);
}
// /* CON3---------------------------------------------------- */
static int32_t sd76030_set_iprechg_current(struct sd76030_chip *chip, uint32_t iprechg_curr)
{
	uint8_t data_reg = 0;
	uint32_t data_temp = 0; 

	if (iprechg_curr < IPRECHG_CURRT_MIN)
		data_temp = IPRECHG_CURRT_MIN;
	else if (iprechg_curr > IPRECHG_CURRT_MAX)
		data_temp = IPRECHG_CURRT_MAX;
	else
		data_temp = iprechg_curr;

	data_reg = (data_temp - IPRECHG_CURRT_MIN)/ IPRECHG_CURRT_STEP;

	return sd76030_update(chip,SD76030_R03,data_reg,CON3_IPRECHG_MASK,CON3_IPRECHG_SHIFT);
}
// /* CON4---------------------------------------------------- */
static int32_t sd76030_set_vreg_volt(struct sd76030_chip *chip, uint32_t vreg_chg_vol)
{
	uint8_t data_reg = 0;
	uint32_t data_temp = 0;

	if (vreg_chg_vol < VREG_VOL_MIN)
		data_temp = VREG_VOL_MIN;
	else if (vreg_chg_vol > VREG_VOL_MAX)
		data_temp = VREG_VOL_MAX;
	else
		data_temp = vreg_chg_vol;

	data_reg = (data_temp - VREG_VOL_MIN ) / (VREG_VOL_STEP >> 1);
	sd76030_update(chip, SD76030_R0D, data_reg & 1, COND_CV_SP_MASK, COND_CV_SPP_SHIFT);
	return sd76030_update(chip, SD76030_R04, data_reg >> 1, CON4_VREG_MASK, CON4_VREG_SHIFT);
}

static int32_t sd76030_get_vreg_volt(struct sd76030_chip *chip)
{
	uint8_t data_reg = 0;
	uint8_t data_reg1 = 0;
	int32_t ret = 0;

	ret = sd76030_read_byte(chip, SD76030_R04, &data_reg);
	if (ret < 0)
		return ret;
	ret = sd76030_read_byte(chip, SD76030_R0D, &data_reg1);
	if (ret < 0)
		return ret;

	data_reg = (data_reg & (CON4_VREG_MASK << CON4_VREG_SHIFT)) >> CON4_VREG_SHIFT;
	data_reg1 = (data_reg1 >> COND_CV_SPP_SHIFT) & COND_CV_SP_MASK;

	return (data_reg * VREG_VOL_STEP + VREG_VOL_MIN) + (data_reg1 * VREG_VOL_STEP / 2);
}

static int32_t sd76030_set_rechg_volt(struct sd76030_chip *chip, uint32_t rechg_volt)
{
	uint8_t data_reg = 0;
	
	if(rechg_volt >= 200)			
		data_reg = 0x01;
	else
		data_reg = 0x00;

	return sd76030_update(chip,SD76030_R04,data_reg,CON4_VRECHG_MASK,CON4_VRECHG_SHIFT);
}
// /* CON5---------------------------------------------------- */
static int32_t sd76030_set_en_term_chg(struct sd76030_chip *chip, int32_t enable)
{
	return sd76030_update(chip,SD76030_R05,!!enable,CON5_EN_TERM_CHG_MASK,CON5_EN_TERM_CHG_SHIFT);
}
static int32_t sd76030_set_wd_timer(struct sd76030_chip *chip, uint32_t second)
{
	uint8_t data_reg = 0;

	if(second < 40)		//second < 40s, disable wdt
		data_reg = 0x00;
	else if(second < 80)	//40 <= second < 80, set 40s 
		data_reg = 0x01;
	else if(second < 160)//80 <= second < 160, set 80s
		data_reg = 0x02;
	else 
		data_reg = 0x03;	//second >= 160, set 160s

	return sd76030_update(chip,SD76030_R05,data_reg,CON5_WTG_TIM_SET_MASK,CON5_WTG_TIM_SET_SHIFT);
}
static int32_t sd76030_set_en_chg_timer(struct sd76030_chip *chip, int32_t enable)
{
	return sd76030_update(chip,SD76030_R05,!!enable,CON5_EN_TIMER_MASK,CON5_EN_TIMER_SHIFT);
}
static int32_t sd76030_get_en_chg_timer(struct sd76030_chip *chip)
{
	uint8_t data_reg = 0;
	int32_t ret = 0;

	ret = sd76030_read_byte(chip,SD76030_R05,&data_reg);
	if(ret < 0)
		return ret;

	data_reg = (data_reg & (CON5_EN_TIMER_MASK << CON5_EN_TIMER_SHIFT)) >> CON5_EN_TIMER_SHIFT;

	return !!data_reg;
} 
// /* CON6---------------------------------------------------- */
static int32_t sd76030_get_vlimit(struct sd76030_chip *chip)
{
	uint8_t data_reg = 0;
	int32_t ret = 0;
	uint32_t vlimit = 0;

	ret = sd76030_read_byte(chip,SD76030_R10,&data_reg);
	if(ret < 0)
		return ret;

	if(data_reg & (CON10_HV_VINDPM_MASK << CON10_HV_VINDPM_SHIFT))
	{
		data_reg = (data_reg & (CON10_VLIMIT_MASK << CON10_VLIMIT_SHIFT)) >> CON10_VLIMIT_SHIFT;
		vlimit =  (data_reg * VILIMIT_VOL_STEP+ VILIMIT_VOL_MIN);
	}
	else
	{
		data_reg = 0;
		ret = sd76030_read_byte(chip,SD76030_R06,&data_reg);
		if(ret < 0)
			return ret;

		data_reg = (data_reg & (CON6_VINDPM_MASK << CON6_VINDPM_SHIFT)) >> CON6_VINDPM_SHIFT;
		vlimit = (data_reg * VINDPM_VOL_STEP+ VINDPM_VOL_MIN);
	}
	return vlimit;
}
static int32_t sd76030_set_vlimit(struct sd76030_chip *chip, uint32_t vlimit_mv)
{
	uint8_t data_reg 	= 0;
	uint32_t data_temp = 0; 
	int32_t ret = 0;
	if (vlimit_mv < VILIMIT_VOL_MIN)
		data_temp = VILIMIT_VOL_MIN;
	else if (vlimit_mv > VILIMIT_VOL_MAX)
		data_temp = VILIMIT_VOL_MAX;
	else
		data_temp = vlimit_mv;

	data_reg = (data_temp - VILIMIT_VOL_MIN)/ VILIMIT_VOL_STEP;

	//enable HV_VINDPM
	ret = sd76030_update(chip,SD76030_R10,1,CON10_HV_VINDPM_MASK,CON10_HV_VINDPM_SHIFT);
	if(ret < 0)
		return ret;

	return sd76030_update(chip,SD76030_R10,data_reg,CON10_VLIMIT_MASK,CON10_VLIMIT_SHIFT);
}
// /* CON8---------------------------------------------------- */
static int32_t sd76030_get_charger_status(struct sd76030_chip *chip)
{
	uint8_t data_reg = 0;
	int32_t ret = 0;

	ret = sd76030_read_byte(chip,SD76030_R08,&data_reg);
	if(ret < 0)
		return ret;
	data_reg = (data_reg & (CON8_CHRG_STAT_MASK << CON8_CHRG_STAT_SHIFT)) >> CON8_CHRG_STAT_SHIFT;
	
	return data_reg;
}
static int32_t sd76030_get_adc_value(struct sd76030_chip *chip,enum sd76030_adc_channel adc)
{
	uint8_t val = 0, pg_good = 0;
	int32_t ret = 0, wait_cnt = 0, adc_value = 0, ilimit = 0;
	uint8_t data = 0;

	mutex_lock(&chip->adc_lock);
#if 1
	// one shot ac conversion
	ret = sd76030_write_byte(chip,SD76030_R11,0x00);
	if(ret < 0)
		goto error;
	//start adc conversion
	ret = sd76030_update(chip,SD76030_R11,1,CON11_START_ADC_MASK,CON11_START_ADC_SHIFT);
	if(ret < 0)
		goto error;
	do
	{
		usleep_range(10000,20000);
		wait_cnt++;
		ret = sd76030_read_byte(chip,SD76030_R11,&val);
		if(ret < 0)
			goto error;
	}while((val & (CON11_START_ADC_MASK << CON11_START_ADC_SHIFT)) && wait_cnt < 100);
//	sd76030_info("wait cnt %d\n",wait_cnt);
#else
	//start 1s continuous conversion
	ret = sd76030_update(chip,SD76030_R11,1,CON11_CONV_RATE_MASK,CON11_CONV_RATE_SHIFT);
	if(ret < 0)
		goto error;
#endif
	ret = sd76030_read_byte(chip,SD76030_R08,&val);
	if(ret < 0)
		goto error;
	
	pg_good = (val & (CON8_PG_STAT_MASK << CON8_PG_STAT_SHIFT)) >> CON8_PG_STAT_SHIFT;

	if(wait_cnt < 100)
	{
		ret = sd76030_read_byte(chip,SD76030_ADC_CHANNEL_START + adc,&val);
		if(ret < 0)
			goto error;
		switch(adc)
		{
			case vbat_channel: 	
			case vsys_channel:	
				data = val;
				data = data &~(1 << 7);
				adc_value = 2304 + data * 20;
				break;

			case tbat_channel:
				adc_value = 21000 + 465 * val;
				break;

			case vbus_channel:
				data = val;
				data = data &~(1 << 7);
				adc_value = (0 == data) ? 0 : (2600 + data * 100);
				break;

			case ibat_channel:
				adc_value = val * 25;
				break;

			case ibus_channel:
				if(1 == pg_good)
				{
					ilimit = sd76030_get_input_curr_lim(chip);
					if(ilimit < 0)
						goto error;
					adc_value = val * ilimit / 127;
				}
				else
					adc_value = 0;
				break;

			default:
				adc_value = 0;
				break;
		}
	}
	else
		goto error;
	sd76030_info("pg_good : %d -- %s[0x%02X]: Regval=0x%02X, Adcval=%d\n",pg_good,adc_channel[adc],SD76030_ADC_CHANNEL_START + adc,val,adc_value);
	mutex_unlock(&chip->adc_lock);
	return adc_value;
error:
	mutex_unlock(&chip->adc_lock);
	return -1;
}

static int32_t sd76030_dump_msg(struct sd76030_chip * chip)
{
	uint8_t i = 0;
	uint8_t data[SD76030_REG_NUM] = { 0 };
	int32_t ret = 0;
	
	for (i = 0; i < SD76030_REG_NUM; i++) 
	{
		ret = sd76030_read_byte(chip,i, &data[i]);
		if (ret < 0) 
		{
			sd76030_info("i2c transfor error\n");
			return -1;
		}
	}

	sd76030_info("[0x00]=0x%02X, [0x01]=0x%02X, [0x02]=0x%02X, [0x03]=0x%02X, [0x04]=0x%02X, [0x05]=0x%02X, [0x06]=0x%02X, [0x07]=0x%02X\n",
			 data[0x00],    data[0x01],    data[0x02],    data[0x03],    data[0x04],    data[0x05],    data[0x06],    data[0x07]);

	sd76030_info("[0x08]=0x%02X, [0x09]=0x%02X, [0x0A]=0x%02X, [0x0B]=0x%02X, [0x0C]=0x%02X, [0x0D]=0x%02X, [0x0E]=0x%02X, [0x0F]=0x%02X\n",
			data[0x08],	 data[0x09],	data[0x0A],	data[0x0B],	data[0x0C],	data[0x0D],	data[0x0E],	data[0x0F]);

	sd76030_info("[0x10]=0x%02X\n",data[0x10]);

	//charge parameter
	sd76030_info("vchg:%d, ichg:%d, ilimit:%d, iterm %d, vlimit %d, sftimer %d, iboost %d\n",
		sd76030_get_vreg_volt(chip),
		sd76030_get_ichg_current(chip),
		sd76030_get_input_curr_lim(chip),
		sd76030_get_iterm(chip),
		sd76030_get_vlimit(chip),
		sd76030_get_en_chg_timer(chip),
		sd76030_get_boost_ilim(chip)
	);
	//fault and state
	sd76030_info("hiz:%d, chg_en:%d, chg_state:%s, vbus_type:%s, pg:%d, thm:%d, vsys_state:%d\n",
		sd76030_get_en_hiz(chip),
		sd76030_get_charger_en(chip),
		charge_state[(data[SD76030_R08] & (CON8_CHRG_STAT_MASK << CON8_CHRG_STAT_SHIFT)) >> CON8_CHRG_STAT_SHIFT],
		vbus_type[(data[SD76030_R08] & (CON8_VBUS_STAT_MASK << CON8_VBUS_STAT_SHIFT)) >> CON8_VBUS_STAT_SHIFT],
		(data[SD76030_R08] & (CON8_PG_STAT_MASK << CON8_PG_STAT_SHIFT)) >> CON8_PG_STAT_SHIFT,
		(data[SD76030_R08] & (CON8_THM_STAT_MASK << CON8_THM_STAT_SHIFT)) >> CON8_THM_STAT_SHIFT,
		(data[SD76030_R08] & (CON8_VSYS_STAT_MASK << CON8_VSYS_STAT_SHIFT)) >> CON8_VSYS_STAT_SHIFT
	);

	sd76030_info("fault:%d, vbus_gd:%d, vdpm:%d, idpm:%d, acov:%d\n",
		data[SD76030_R09],
		(data[SD76030_R0A] & (CONA_VBUS_GD_MASK << CONA_VBUS_GD_SHIFT)) >> CONA_VBUS_GD_SHIFT,
		(data[SD76030_R0A] & (CONA_VINDPM_STAT_MASK << CONA_VINDPM_STAT_SHIFT)) >> CONA_VINDPM_STAT_SHIFT,
		(data[SD76030_R0A] & (CONA_IDPM_STAT_MASK << CONA_IDPM_STAT_SHIFT)) >> CONA_IDPM_STAT_SHIFT,
		(data[SD76030_R0A] & (CONA_ACOV_STAT_MASK << CONA_ACOV_STAT_SHIFT)) >> CONA_ACOV_STAT_SHIFT
	);

	for (i = vbat_channel; i < max_channel; i++) 
		sd76030_get_adc_value(chip,i);	 	
	 	
	return 0;
}
static int32_t sd76030_set_dpdm(struct sd76030_chip *chip,uint8_t dp_val, uint8_t dm_val)
{
	uint8_t data_reg = 0;
	data_reg  = (dp_val & CONC_DP_VOLT_MASK) << CONC_DP_VOLT_SHIFT;
	data_reg |= (dm_val & CONC_DM_VOLT_MASK) << CONC_DM_VOLT_SHIFT;
	return sd76030_update(chip,SD76030_R0C,data_reg,CONC_DP_DM_MASK,CONC_DP_DM_SHIFT);
}
#ifdef CONFIG_MTK_EXTERNAL_CHARGER_TYPE_DETECT
#if 0
static int32_t sd76030_charging_set_hvdcp20(struct sd76030_chip *chip,uint32_t vbus_target)
{
	int32_t ret = 0;
	//5V,and D+ = 0.6 to wake up adapter
	ret = sd76030_set_dpdm(chip,CONC_DP_DM_VOL_0P6,CONC_DP_DM_VOL_0P0);
	if(ret < 0)
		return ret;
	//must wait adapter
	msleep(1250);

	if(9 == vbus_target)
		ret = sd76030_set_dpdm(chip,CONC_DP_DM_VOL_3P3,CONC_DP_DM_VOL_0P6);
	else if(12 == vbus_target)
		ret = sd76030_set_dpdm(chip,CONC_DP_DM_VOL_0P6,CONC_DP_DM_VOL_0P6);

 	return (ret < 0) ? ret : 0;
}
#endif
static int32_t sd76030_chg_type_det(struct sd76030_chip *chip,uint8_t en)
{
	int32_t ret = 0;
	uint8_t val = 0;
	int32_t wait_cnt = 0;
	uint8_t chg_type = 0;

	if(0 == en)
		return 0;

	ret = sd76030_update(chip,SD76030_R07,1,FORCE_IINDET_MASK,FORCE_IINDET_SHIFT);
	if(ret < 0)
		return ret;
	do
	{
		msleep(100);
		wait_cnt++;
		ret = sd76030_read_byte(chip,SD76030_R07,&val);
		if(ret < 0)
			return ret;
		
	}while((val & (FORCE_IINDET_MASK << FORCE_IINDET_SHIFT)) && wait_cnt < 30);
	sd76030_info("wait cnt %d\n",wait_cnt);
	if(wait_cnt < 30)
	{
		ret = sd76030_read_byte(chip,SD76030_R08,&val);
		if(ret < 0)
			return ret;

		chg_type = (val & (CON8_VBUS_STAT_MASK << CON8_VBUS_STAT_SHIFT)) >> CON8_VBUS_STAT_SHIFT;
		sd76030_info("chg_type %s\n",vbus_type[chg_type]);
	}
	else
		return -1;
	
	return 0;
}

static void sd76030_enable_bc12(struct sd76030_chip *chip, bool en)
{
	sd76030_info("en = %d\n", en);
	atomic_set(&chip->bc12_en_req_cnt, en);
	wake_up(&chip->bc12_en_req);
}

static int32_t sd76030_bc12_preprocess(struct sd76030_chip *chip)
{	
	sd76030_enable_bc12(chip, true);
	return 0;
}

static void sd76030_inform_psy_dwork_handler(struct work_struct *work)
{
	//int32_t ret = 0;
	//union power_supply_propval propval = {.intval = 0};

	struct sd76030_chip *chip = container_of(work, struct sd76030_chip,psy_dwork.work);
#if 0
	bool attach = false;
	enum charger_type chg_type = CHARGER_UNKNOWN;

	mutex_lock(&chip->bc12_lock);
	attach = chip->attach;
	chg_type = chip->chg_type;
	mutex_unlock(&chip->bc12_lock);

	sd76030_info("attach = %d, type = %d\n",attach, chg_type);

	/* Get chg type det power supply */
	if (!chip->psy)
		chip->psy = power_supply_get_by_name("charger");
	if (!chip->psy)
	{
		sd76030_info("get power supply fail\n");
		mod_delayed_work(system_wq, &chip->psy_dwork,msecs_to_jiffies(1000));
		return;
	}
	propval.intval = attach;
	ret = power_supply_set_property(chip->psy, POWER_SUPPLY_PROP_ONLINE,
					&propval);
	if (ret < 0)
		sd76030_info("psy online fail(%d)\n",ret);

	propval.intval = chg_type;
	ret = power_supply_set_property(chip->psy,POWER_SUPPLY_PROP_CHARGE_TYPE,&propval);
	if (ret < 0)
		sd76030_info("psy type fail(%d)\n", ret);
#endif
	power_supply_changed(chip->sd76030_psy);
}
enum sd76030_usbsw {
	USBSW_CHG = 0,
	USBSW_USB,
};
#define PHY_MODE_BC11_SET 1
#define PHY_MODE_BC11_CLR 2
extern int32_t phy_set_mode_ext(struct phy *phy, enum phy_mode mode, int32_t submode);
static int32_t sd76030_chg_set_usbsw(struct sd76030_chip *chip,enum sd76030_usbsw usbsw)
{
	struct phy *phy = NULL;
	int32_t ret = 0;
	int32_t mode = (usbsw == USBSW_CHG) ? PHY_MODE_BC11_SET : PHY_MODE_BC11_CLR;

	pr_info("usbsw = %d\n", usbsw);
	phy = phy_get(chip->dev, "usb2-phy");
	if (IS_ERR_OR_NULL(phy)) {
		dev_err(chip->dev, "failed to get usb2-phy\n");
		return -ENODEV;
	}
	ret = phy_set_mode_ext(phy, PHY_MODE_USB_DEVICE, mode);
	if (ret)
		dev_err(chip->dev, "failed to set phy ext mode\n");
	phy_put(chip->dev, phy);
	return ret;
}

static int32_t sd76030_bc12_postprocess(struct sd76030_chip *chip)
{
	int32_t ret = 0;
	bool attach = false, inform_psy = true;
	u8 port = SD76030_PORTSTAT_NO_INPUT;

	attach = atomic_read(&chip->vbus_gd);
	if (chip->attach == attach) 
	{
		sd76030_info("attach(%d) is the same\n", attach);
		inform_psy = !attach;
		goto out;
	}
	chip->attach = attach;
	sd76030_info("attach = %d\n", attach);

	if (!attach) 
	{
		chip->port = SD76030_PORTSTAT_NO_INPUT;
		chip->chg_type = CHARGER_UNKNOWN;
		goto out;
	}

	ret = sd76030_read_byte(chip, SD76030_R08, &port);
	if (ret < 0)
		chip->port = SD76030_PORTSTAT_NO_INPUT;
	else
		chip->port = (port & (CON8_VBUS_STAT_MASK << CON8_VBUS_STAT_SHIFT)) >> CON8_VBUS_STAT_SHIFT;

	sd76030_info("chg_type %s\n",vbus_type[chip->port]);

	switch (chip->port)
	{
		case SD76030_PORTSTAT_NO_INPUT:
			chip->chg_type = CHARGER_UNKNOWN;
			chip->psy_desc.type = POWER_SUPPLY_TYPE_USB;
			break;
		case SD76030_PORTSTAT_SDP:
			chip->chg_type = STANDARD_HOST;
			chip->psy_desc.type = POWER_SUPPLY_TYPE_USB;
			break;
		case SD76030_PORTSTAT_CDP:
			chip->chg_type = CHARGING_HOST;
			chip->psy_desc.type = POWER_SUPPLY_TYPE_USB_CDP;
			break;
		case SD76030_PORTSTAT_DCP:
		case SD76030_PORTSTAT_HVDCP:
			chip->chg_type = STANDARD_CHARGER;
			chip->psy_desc.type = POWER_SUPPLY_TYPE_USB_DCP;
			//if(chip->target_hv)
				//sd76030_charging_set_hvdcp20(chip,chip->target_hv);
			break;
		case SD76030_PORTSTAT_UNKNOWN:
			//chip->chg_type = CHARGER_UNKNOWN;
			//chip->psy_desc.type = POWER_SUPPLY_TYPE_USB;
			chip->chg_type = NONSTANDARD_CHARGER;
			chip->psy_desc.type = POWER_SUPPLY_TYPE_USB;
			break;
		case SD76030_PORTSTAT_NON_STANDARD:
		case SD76030_PORTSTAT_OTG:
		default:
			chip->chg_type = NONSTANDARD_CHARGER;
			chip->psy_desc.type = POWER_SUPPLY_TYPE_USB_DCP;
			break;
	}
out:
	if(chip->chg_type != STANDARD_CHARGER)
		sd76030_chg_set_usbsw(chip, USBSW_USB);	

	if (inform_psy)
	{
		cancel_delayed_work_sync(&chip->psy_dwork);
		schedule_delayed_work(&chip->psy_dwork, 0);
	}
	return 0;
}

static bool is_usb_rdy(struct device *dev)
{
	bool ready = true;
	struct device_node *node;

	node = of_parse_phandle(dev->of_node, "usb", 0);
	if (node) {
		ready = !of_property_read_bool(node, "cdp-block");
		pr_info("usb ready = %d\n", ready);
	} else
		dev_warn(dev, "usb node missing or invalid\n");
	return ready;
}

static int32_t sd76030_bc12_en_kthread(void *data)
{
	int32_t ret = 0, i = 0;
	struct sd76030_chip *chip = data;
	const int32_t max_wait_cnt = 200;

	sd76030_info("\n");

wait:
	wait_event(chip->bc12_en_req, atomic_read(&chip->bc12_en_req_cnt) > 0 || kthread_should_stop());

	if (atomic_read(&chip->bc12_en_req_cnt) <= 0 && kthread_should_stop()) 
	{
		sd76030_info( "bye bye\n");
		return 0;
	}
	atomic_dec(&chip->bc12_en_req_cnt);
	
	pm_stay_awake(chip->dev);

	if(atomic_read(&chip->vbus_gd))
	{
		/* Workaround for CDP port */
		for (i = 0; i < max_wait_cnt; i++) 
		{
			if (is_usb_rdy(chip->dev))
				break;
			sd76030_info("CDP block\n");
			if (!atomic_read(&chip->vbus_gd)) 
			{
				sd76030_info("plug out\n");
				break;
			}
			msleep(100);
		}
		if (i == max_wait_cnt)
			sd76030_info("CDP timeout\n");
		else
			sd76030_info("CDP free\n");	
	}

	sd76030_chg_set_usbsw(chip, (atomic_read(&chip->vbus_gd))  ? USBSW_CHG : USBSW_USB);
	ret = sd76030_chg_type_det(chip,atomic_read(&chip->vbus_gd));
	if (ret < 0)
		sd76030_info("fail(%d)\n",ret);
	sd76030_bc12_postprocess(chip);

	pm_relax(chip->dev);
	goto wait;

	return 0;
}
#endif /* CONFIG_MTK_EXTERNAL_CHARGER_TYPE_DETECT */

/*MTK ops*/
static int32_t sd76030_chg_plug_in(struct charger_device* chg_dev)
{
	sd76030_info("\n");
	return 0;
}
static int32_t sd76030_chg_plug_out(struct charger_device* chg_dev)
{
	struct sd76030_chip *chip = dev_get_drvdata(&chg_dev->dev);
	sd76030_info("\n");
	//reset DP DM to hiz
	sd76030_set_dpdm(chip,CONC_DP_DM_VOL_HIZ,CONC_DP_DM_VOL_HIZ);
	return 0;
}

static int32_t sd76030_enable_charging(struct charger_device *chg_dev,bool en)
{
	int32_t ret = 0;     
	struct sd76030_chip *chip = dev_get_drvdata(&chg_dev->dev);

	sd76030_info("enable state : %d\n", en);

	ret = sd76030_set_charger_en(chip,!!en);
	
	return (ret < 0) ? ret : 0;
}
static int32_t sd76030_is_enabled(struct charger_device* chg_dev , bool* en)
{
	int32_t ret = 0;
	struct sd76030_chip *chip = dev_get_drvdata(&chg_dev->dev);
		
	ret = sd76030_get_charger_en(chip);
	if(ret < 0) 
	{
		*en = false;
		return ret;
	}
	sd76030_info("ret = %d\n",ret);
	*en = (ret) ? true : false;

	return 0;
}

static int32_t sd76030_get_current(struct charger_device *chg_dev,uint32_t *ichg)
{
	int32_t ret = 0;
	struct sd76030_chip *chip = dev_get_drvdata(&chg_dev->dev);

	ret = sd76030_get_ichg_current(chip);
	if(ret < 0)
		return ret;

	sd76030_info("ret = %d\n",ret);
	*ichg = ret * 1000;

	return 0;
}

static int32_t sd76030_set_current(struct charger_device *chg_dev,uint32_t current_value)
{
	int32_t ret = 0;
	struct sd76030_chip *chip = dev_get_drvdata(&chg_dev->dev);

	sd76030_info("charge_current_value = %d\n", current_value);
	
	ret = sd76030_set_ichg_current(chip,current_value / 1000);

	return (ret < 0) ? ret : 0;
}

static int32_t sd76030_get_min_charging_current(struct charger_device* chg_dev, uint32_t* val_ua)
{
	*val_ua = 0;
	sd76030_info("return min cc : %d uA\n",*val_ua);
	return 0;
}

static int32_t sd76030_set_constant_voltage(struct charger_device *chg_dev,uint32_t cv)
{
	int32_t ret = 0;
	struct sd76030_chip *chip = dev_get_drvdata(&chg_dev->dev);

	sd76030_info("cv = %d uV\n", cv);

	ret = sd76030_set_vreg_volt(chip,cv / 1000);

	return (ret < 0) ? ret : 0;
}
static int32_t sd76030_get_constant_voltage(struct charger_device* chg_dev, uint32_t* val_uv)
{
	int32_t ret = 0;
	struct sd76030_chip *chip = dev_get_drvdata(&chg_dev->dev);

	ret = sd76030_get_vreg_volt(chip);
	if(ret < 0)
		return ret;

	sd76030_info("ret = %d\n",ret);

	*val_uv = ret * 1000;

	return 0;
}

static int32_t sd76030_get_input_current(struct charger_device *chg_dev,uint32_t *aicr)
{
	int32_t ret = 0;
	struct sd76030_chip *chip = dev_get_drvdata(&chg_dev->dev);

	ret = sd76030_get_input_curr_lim(chip);
	if(ret < 0)
		return ret;

	sd76030_info("ret = %d\n",ret);

	*aicr = ret * 1000;

	return 0;
}

static int32_t sd76030_set_input_current(struct charger_device *chg_dev,uint32_t current_value)
{
	int32_t ret = 0;
	struct sd76030_chip *chip = dev_get_drvdata(&chg_dev->dev);

	sd76030_info("current_value = %d\n", current_value);

	ret = sd76030_set_input_curr_lim(chip,current_value / 1000);

	return (ret < 0) ? ret : 0;
}
static int32_t sd76030_get_min_input_current(struct charger_device* chg_dev, uint32_t* val_ua)
{
	*val_ua = 100 * 1000;
	sd76030_info("return min input current : %d uA\n",*val_ua);
	return 0;
}

static int32_t sd76030_get_eoc_current(struct charger_device* chg_dev, uint32_t* val_ua)
{
	int32_t ret = 0;
	struct sd76030_chip *chip = dev_get_drvdata(&chg_dev->dev);

	ret = sd76030_get_iterm(chip);
	if(ret < 0)
		return ret;

	sd76030_info("ret = %d\n",ret);

	*val_ua = ret * 1000;

	return 0;
}
static int32_t sd76030_set_eoc_current(struct charger_device* chg_dev, uint32_t eoc_ua)
{
	int32_t ret = 0;
	struct sd76030_chip *chip = dev_get_drvdata(&chg_dev->dev);

	sd76030_info("eoc_ua = %d\n", eoc_ua);

	ret = sd76030_set_iterm(chip,eoc_ua / 1000);

	return (ret < 0) ? ret : 0;
}
static int32_t sd76030_reset_watch_dog_timer(struct charger_device*chg_dev)
{
	struct sd76030_chip *chip = dev_get_drvdata(&chg_dev->dev);

	sd76030_info("\n");

	sd76030_set_wd_reset(chip);	/* Kick watchdog */

	sd76030_set_wd_timer(chip,160);	/* WDT 160s */

	return 0;
}
static int32_t sd76030_do_event(struct charger_device *chg_dev, uint32_t event,uint32_t args)
{
	struct sd76030_chip *chip = dev_get_drvdata(&chg_dev->dev);

	sd76030_info("event = %d\n", event);

	switch (event) {
	case EVENT_FULL:
	case EVENT_RECHARGE:
	case EVENT_DISCHARGE:
		power_supply_changed(chip->sd76030_psy);
		break;
	default:
		break;
	}

	return 0;
}

#if 0
static int sd76030_reset_ta(struct charger_device *chg_dev)
{
 	int32_t ret = 0;
 	struct sd76030_chip *chip = dev_get_drvdata(&chg_dev->dev);

 	sd76030_info("\n");

	ret = sd76030_set_vlimit(chip,4600000);
 	if (ret < 0)
 		return ret;

 	ret = sd76030_set_input_curr_lim(chip,100);	/* 100mA */
 	if (ret < 0)
 		return ret;

 	msleep(250);

 	ret = sd76030_set_input_curr_lim(chip,500);	/* 100mA */
 	if (ret < 0)
		return ret;

	return (ret < 0) ? ret : 0;
}
#endif

static int32_t sd76030_charging_set_ta_current_pattern(struct charger_device *chg_dev, bool is_increase)
{
	bool increase = is_increase;
	struct sd76030_chip *chip = dev_get_drvdata(&chg_dev->dev);

	sd76030_info("is_increase %d\n",is_increase);
	//sd76030_set_vreg_volt(chip,4400);	/* Set CV */	
	sd76030_set_ichg_current(chip,800);/* Set charging current 800ma */
	/* Enable Charging */
	sd76030_set_en_hiz(chip,0);
	sd76030_set_charger_en(chip,1);

	if (increase == 1) 
	{
		sd76030_set_input_curr_lim(chip,100);	/* 100mA */
		msleep(85);

		sd76030_set_input_curr_lim(chip,500);	/* 500mA */
		msleep(85);

		sd76030_set_input_curr_lim(chip,100);	/* 100mA */
		msleep(85);

		sd76030_set_input_curr_lim(chip,500);	/* 500mA */
		msleep(85);

		sd76030_set_input_curr_lim(chip,100);	/* 100mA */
		msleep(85);

		sd76030_set_input_curr_lim(chip,500);	/* 500mA */
		msleep(281);

		sd76030_set_input_curr_lim(chip,100);	/* 100mA */
		msleep(85);

		sd76030_set_input_curr_lim(chip,500);/* 500mA */
		msleep(281);

		sd76030_set_input_curr_lim(chip,100);	/* 100mA */
		msleep(85);

		sd76030_set_input_curr_lim(chip,500);	/* 500mA */
		msleep(281);

		sd76030_set_input_curr_lim(chip,100);	/* 100mA */
		msleep(85);

		sd76030_set_input_curr_lim(chip,500);	/* 500mA */
		msleep(485);

		sd76030_set_input_curr_lim(chip,100);	/* 100mA */
		msleep(50);

		sd76030_set_input_curr_lim(chip,500);	/* 500mA */
		msleep(200);
	} else {
		sd76030_set_input_curr_lim(chip,100);	/* 100mA */
		msleep(85);

		sd76030_set_input_curr_lim(chip,500);	/* 500mA */
		msleep(281);

		sd76030_set_input_curr_lim(chip,100);	/* 100mA */
		msleep(85);

		sd76030_set_input_curr_lim(chip,500);	/* 500mA */
		msleep(281);

		sd76030_set_input_curr_lim(chip,100);	/* 100mA */
		msleep(85);

		sd76030_set_input_curr_lim(chip,500);	/* 500mA */
		msleep(281);

		sd76030_set_input_curr_lim(chip,100);	/* 100mA */
		msleep(85);

		sd76030_set_input_curr_lim(chip,500);	/* 500mA */
		msleep(85);

		sd76030_set_input_curr_lim(chip,100);	/* 100mA */
		msleep(85);

		sd76030_set_input_curr_lim(chip,500);	/* 500mA */
		msleep(85);

		sd76030_set_input_curr_lim(chip,100);	/* 100mA */
		msleep(85);

		sd76030_set_input_curr_lim(chip,500);	/* 500mA */
		msleep(485);

		sd76030_set_input_curr_lim(chip,100);	/* 100mA */
		msleep(50);

		sd76030_set_input_curr_lim(chip,500);	/* 500mA */
	}

	return 0;
}
static int32_t sd76030_get_vindpm_voltage(struct charger_device *chg_dev, uint32_t *val_uV)
{
	int32_t ret = 0;
	struct sd76030_chip *chip = dev_get_drvdata(&chg_dev->dev);

	ret = sd76030_get_vlimit(chip);
	if(ret < 0)
		return ret;

	sd76030_info("ret = %d\n",ret);

	*val_uV = ret * 1000;

	return 0;
}

static int32_t sd76030_set_vindpm_voltage(struct charger_device *chg_dev,uint32_t vindpm)
{
	int32_t ret = 0;
	struct sd76030_chip *chip = dev_get_drvdata(&chg_dev->dev);

	sd76030_info("vindpm =%d\n", vindpm);

	ret = sd76030_set_vlimit(chip,vindpm / 1000);

	return (ret < 0) ? ret : 0;
}
static int32_t sd76030_get_vindpm_state(struct charger_device *chg_dev, bool *in_loop)
{
	int32_t ret = 0;
	uint8_t data = 0;
	struct sd76030_chip *chip = dev_get_drvdata(&chg_dev->dev);

	ret = sd76030_read_byte(chip,SD76030_R0A,&data);
	if (ret < 0) 
		return ret;
	
	*in_loop = (data & (CONA_VINDPM_STAT_MASK << CONA_VINDPM_STAT_SHIFT)) >> CONA_VINDPM_STAT_SHIFT;

	sd76030_info("in_loop =%d\n", *in_loop);

	return 0;
}

static int32_t sd76030_get_vbus_state(struct sd76030_chip *chip)
{
	int32_t ret = 0;
	uint8_t data = 0;

	ret = sd76030_read_byte(chip,SD76030_R0A,&data);
	if (ret) 
		return ret;
	
	data = (data & (CONA_VBUS_GD_MASK << CONA_VBUS_GD_SHIFT)) >> CONA_VBUS_GD_SHIFT;
	
	return data;
}

static int32_t sd76030_is_powerpath_enabled(struct charger_device *chg_dev, bool *en)
{
	int32_t ret = 0;
	struct sd76030_chip *chip = dev_get_drvdata(&chg_dev->dev);

	ret = sd76030_get_en_hiz(chip);
	if(ret < 0)
		return ret;

	sd76030_info("ret = %d\n",ret);

	//en hiz = 1,ic is suspend, power path is disabled
	*en = (ret) ? false : true;

	return 0;
}
static int32_t sd76030_enable_powerpath(struct charger_device *chg_dev, bool en)
{
	int32_t ret = 0;
	uint32_t vlimit = (en ? 4500 : VILIMIT_VOL_MAX);
	struct sd76030_chip *chip = dev_get_drvdata(&chg_dev->dev);

	sd76030_info("enable %d\n",en);

	//if want to open power path, disable en hiz
	//ret = sd76030_set_en_hiz(chip,!en);
	ret = sd76030_set_vlimit(chip,vlimit);
	return (ret < 0) ? ret : 0;
}
static int32_t sd76030_get_is_safetytimer_enable(struct charger_device *chg_dev, bool *en)
{
	int32_t ret = 0;
	struct sd76030_chip *chip = dev_get_drvdata(&chg_dev->dev);

	ret = sd76030_get_en_chg_timer(chip);
	if(ret < 0)
		return ret;

	*en = (bool)ret;
	sd76030_info("en =%d\n", *en);
	return 0;
}
static int32_t sd76030_enable_safetytimer(struct charger_device *chg_dev,bool en)
{
	int32_t ret = 0;
	struct sd76030_chip *chip = dev_get_drvdata(&chg_dev->dev);

	sd76030_info("en = %d\n",en);

	ret =	sd76030_set_en_chg_timer(chip,!!en);

	return (ret < 0) ? ret : 0;
}

static int32_t sd76030_enable_termination(struct charger_device *chg_dev, bool en)
{
	int32_t ret = 0;
	struct sd76030_chip *chip = dev_get_drvdata(&chg_dev->dev);

	sd76030_info("en = %d\n",en);

	ret =	sd76030_set_en_term_chg(chip,!!en);

	return (ret < 0) ? ret : 0;
}

static int sd76030_enable_otg(struct charger_device *chg_dev, bool en)
{
	 int32_t ret = 0;
	 struct sd76030_chip *chip = dev_get_drvdata(&chg_dev->dev);
	 sd76030_info("en = %d\n", en);

	 ret = sd76030_set_otg_en(chip,!!en);

	 return (ret < 0)? ret:0;
}

int sd76030_is_vbus_on(void)
{
	int status=0;
		
	status = sd76030_get_otg_config(g_sd76030_chip);
	printk("%s:%d\n",__func__,status);
	return status;
}
EXPORT_SYMBOL(sd76030_is_vbus_on);

int st_set_vbus(bool en){
	int32_t ret = 0;

	ret = sd76030_set_otg_en(g_sd76030_chip,!!en);

	return (ret < 0) ? ret : 0;

}
EXPORT_SYMBOL(st_set_vbus);

static int32_t sd76030_set_boost_current_limit(struct charger_device *chg_dev, uint32_t boot_cur)
{
	int32_t ret = 0;
	struct sd76030_chip *chip = dev_get_drvdata(&chg_dev->dev);

	sd76030_info("boot_cur = %d uA\n", boot_cur);

	ret = sd76030_set_boost_ilim(chip,boot_cur / 1000);

	return (ret < 0) ? ret : 0;
}
static int32_t sd76030_enable_chg_type_det(struct charger_device *chg_dev, bool en)
{
	int32_t ret = 0;
	struct sd76030_chip *chip = dev_get_drvdata(&chg_dev->dev);

	sd76030_info("en = %d\n",en);
	//if use the irq of sd76030,chg type will report in irq function
	if (-1 != chip->irq_gpio) 
		return 0;
	
#ifdef CONFIG_MTK_EXTERNAL_CHARGER_TYPE_DETECT
//#ifdef CONFIG_TCPC_CLASS
	mutex_lock(&chip->bc12_lock);
	atomic_set(&chip->vbus_gd, en);
	ret = (en ? sd76030_bc12_preprocess : sd76030_bc12_postprocess)(chip);
	mutex_unlock(&chip->bc12_lock);
	if (ret < 0)
		sd76030_info("en bc12 fail(%d)\n", ret);
//#endif /* CONFIG_TCPC_CLASS */
#endif /* CONFIG_MTK_EXTERNAL_CHARGER_TYPE_DETECT */
	return (ret < 0) ? ret : 0;
}
static int32_t sd76030_get_charging_status(struct charger_device *chg_dev,bool *is_done)
{
	int32_t ret_val = 0;
	struct sd76030_chip *chip = dev_get_drvdata(&chg_dev->dev);

	ret_val = sd76030_get_charger_status(chip);

	if (ret_val == 0x3)
		*is_done = true;
	else
		*is_done = false;

	sd76030_info("is_done = %d\n", *is_done);

	return 0;
}
static int32_t sd76030_dump_register(struct charger_device *chg_dev)
{
	struct sd76030_chip *chip = dev_get_drvdata(&chg_dev->dev);

	return sd76030_dump_msg(chip);
}
static int32_t to_sd76030_adc(enum adc_channel chan)
{
	int32_t channel = error_channel;
	switch(chan)
	{
		case ADC_CHANNEL_VBUS	:	channel = vbus_channel;	break;
		case ADC_CHANNEL_VSYS	:	channel = vsys_channel;	break;
		case ADC_CHANNEL_VBAT	:	channel = vbat_channel;	break;
		case ADC_CHANNEL_IBUS	:	channel = ibus_channel;	break;
		case ADC_CHANNEL_IBAT	:	channel = ibat_channel;	break;
		case ADC_CHANNEL_TEMP_JC:
		case ADC_CHANNEL_USBID	:
		case ADC_CHANNEL_TS		:
		default:break;
	}
	return channel;
}
static int32_t sd76030_get_adc(struct charger_device *chg_dev, enum adc_channel chan,int32_t *min, int32_t *max)
{
	int32_t ret = 0;
	struct sd76030_chip *chip = dev_get_drvdata(&chg_dev->dev);
	enum sd76030_adc_channel channel = to_sd76030_adc(chan);

	if(error_channel == channel)
		return -ENOTSUPP;

	ret = sd76030_get_adc_value(chip,channel);
	if(ret < 0)
		return ret;

	*min = *max = ret;
	sd76030_info("min = %d max = %d\n", *min,*max);

	return 0;
}
static int32_t sd76030_get_vbus_adc(struct charger_device *chg_dev, uint32_t *vbus)
{
	int32_t ret = 0;
	struct sd76030_chip *chip = dev_get_drvdata(&chg_dev->dev);

	ret = sd76030_get_adc_value(chip,vbus_channel);
	if(ret < 0)
		return ret;
	
	*vbus = ret*1000;
	sd76030_info("vbus = %d uv\n", *vbus);

	return 0;
}
static int32_t sd76030_get_ibus_adc(struct charger_device *chg_dev, uint32_t *ibus)
{
	int32_t ret = 0;
	struct sd76030_chip *chip = dev_get_drvdata(&chg_dev->dev);
	
	ret = sd76030_get_adc_value(chip,ibus_channel);
	if(ret < 0)
		return ret;

	*ibus = ret * 1000;
	sd76030_info("ibus = %d\n", *ibus);

	return 0;
}
static int32_t sd76030_get_ibat_adc(struct charger_device *chg_dev, uint32_t *ibat)
{
	int32_t ret = 0;
	struct sd76030_chip *chip = dev_get_drvdata(&chg_dev->dev);

	ret = sd76030_get_adc_value(chip,ibat_channel);
	if(ret < 0)
		return ret;

	*ibat = ret;
	sd76030_info("ibat = %d\n", *ibat);

	return 0;
}

static struct charger_ops sd76030_chg_ops =
{
	.suspend			= NULL,
	.resume			= NULL,
	/* cable plug in/out */
	.plug_in			= sd76030_chg_plug_in,
	.plug_out			= sd76030_chg_plug_out,

	.enable 			= sd76030_enable_charging,
	.is_enabled			= sd76030_is_enabled,

	.enable_chip 			= sd76030_enable_powerpath,
	.is_chip_enabled 		= sd76030_is_powerpath_enabled,

	.get_charging_current 		= sd76030_get_current,
	.set_charging_current 		= sd76030_set_current,
	.get_min_charging_current	= sd76030_get_min_charging_current,

	.set_constant_voltage 		= sd76030_set_constant_voltage,
	.get_constant_voltage		= sd76030_get_constant_voltage,

	.get_input_current 		= sd76030_get_input_current,
	.set_input_current 		= sd76030_set_input_current,
	.get_min_input_current		= sd76030_get_min_input_current,

	.get_eoc_current		= sd76030_get_eoc_current,
	.set_eoc_current		= sd76030_set_eoc_current,

	.kick_wdt 			= sd76030_reset_watch_dog_timer,
	.event 			= sd76030_do_event,

	/* PE/PE+ */
	.reset_ta 			= NULL,//sd76030_reset_ta,
	.send_ta_current_pattern	= sd76030_charging_set_ta_current_pattern,

	.send_ta20_current_pattern 	= NULL,
	//.set_ta20_reset		= NULL,
	.enable_cable_drop_comp	= NULL,

	.get_mivr			= sd76030_get_vindpm_voltage,
	.set_mivr 			= sd76030_set_vindpm_voltage,
	.get_mivr_state		= sd76030_get_vindpm_state,
	/* Power path */
	.is_powerpath_enabled 		= sd76030_is_powerpath_enabled, 
	.enable_powerpath 		= sd76030_enable_powerpath, 

	.enable_vbus_ovp		= NULL,
	/* Safety timer */
	.is_safety_timer_enabled 	= sd76030_get_is_safetytimer_enable,
	.enable_safety_timer 		= sd76030_enable_safetytimer,

	.enable_termination		= sd76030_enable_termination,
		/* direct charging */
	.enable_direct_charging	= NULL,
	.kick_direct_charging_wdt	= NULL,
	.set_direct_charging_ibusoc	= NULL,
	.set_direct_charging_vbusov	= NULL,
	// /* OTG */
	.enable_otg 			= sd76030_enable_otg,
	.enable_discharge		= NULL,
	.set_boost_current_limit 	= sd76030_set_boost_current_limit,

	.enable_chg_type_det		= sd76030_enable_chg_type_det,
	
	/* run AICL */
	.run_aicl			= NULL,
	/* reset EOC state */
	.reset_eoc_state		= NULL,
	.safety_check			= NULL,

	.is_charging_done 		= sd76030_get_charging_status,
	.set_pe20_efficiency_table	= NULL,
	.dump_registers 		= sd76030_dump_register,
	.enable_hz 				= sd76030_enable_hz,
	.get_adc			= sd76030_get_adc,
	.get_vbus_adc			= sd76030_get_vbus_adc,
	.get_ibus_adc			= sd76030_get_ibus_adc,
	.get_ibat_adc			= sd76030_get_ibat_adc,
	.get_tchg_adc			= NULL,
	.get_zcv			= NULL,
};

static int32_t sd76030_detect_ic(struct sd76030_chip *chip)
{
	uint8_t hw_id = 0;
	int32_t ret = 0;

	ret = sd76030_read_byte(chip,SD76030_R0B,&hw_id);
	if(ret < 0)
		return ret;

	pr_info("hw_id:0x%02X\n",hw_id);
	
	if(0x38 != hw_id)
		return -1;

	return 0;
}

static int32_t sd76030_get_component_id(struct sd76030_chip *chip)
{
	uint8_t chip_id = 0, version_id = 0;
	int32_t ret = 0;

	ret = sd76030_read_byte(chip,SD76030_R0E,&chip_id);
	if(ret < 0)
		return ret;

	ret = sd76030_read_byte(chip,SD76030_R0F,&version_id);
	if(ret < 0)
		return ret;

	chip->component_id = chip_id << 8 | version_id;

	pr_info("chip_id:0x%02X,version_id:0x%02X,component_id:0x%04X\n",chip_id,version_id,chip->component_id);
	
	return 0;
}

static void sd76030_hw_init(struct sd76030_chip *chip)
{
	sd76030_write_byte(chip,SD76030_R00,0x14);	//set ilimit 500ma as default
	sd76030_write_byte(chip,SD76030_R01,0x18);
	sd76030_write_byte(chip,SD76030_R02,0xA2);	//set ichg 2040ma as default
	sd76030_write_byte(chip,SD76030_R03,0x22);	//set pre_chg and iterm current 180ma
	sd76030_write_byte(chip,SD76030_R04,0x78);	//set as default,cv 4208mv
	sd76030_write_byte(chip,SD76030_R05,0x8F);	//enable iterm,disable wdt,enable charge timer,chg timer 10hrs,jeita 20% ichg
	sd76030_write_byte(chip,SD76030_R06,0xE6);	//set vindpm 4500mv as default
	sd76030_write_byte(chip,SD76030_R07,0x4C);	//set as default
	sd76030_write_byte(chip,SD76030_R0A,0x03);	//disable iindpm interrupt
	sd76030_write_byte(chip,SD76030_R0C,0x00);	
	sd76030_write_byte(chip,SD76030_R0D,0x12);	
	sd76030_write_byte(chip,SD76030_R10,0x86);	//enable hv_vindpm

	sd76030_set_en_hiz(chip,0);
	sd76030_set_vlimit(chip,chip->vlimit_mv);	
	sd76030_set_input_curr_lim(chip,chip->ilimit_ma);
	sd76030_set_wd_reset(chip);	
	sd76030_set_sys_min_volt(chip,chip->vsysmin_mv);
	sd76030_set_iprechg_current(chip,chip->pre_ma);	
	sd76030_set_iterm(chip,chip->eoc_ma);	
	sd76030_set_vreg_volt(chip,chip->cv_mv);	
	sd76030_set_ichg_current(chip,chip->cc_ma);
	sd76030_set_rechg_volt(chip,chip->rechg_mv);
	sd76030_set_en_term_chg(chip,1);	
	sd76030_set_wd_timer(chip,0);	
	sd76030_set_en_chg_timer(chip,1);	
	sd76030_set_otg_en(chip,0);
	sd76030_set_charger_en(chip,1);
}

static void sd76030_work_func(struct work_struct *work)
{
	struct sd76030_chip *chip = container_of(work, struct sd76030_chip,sd76030_work.work);
	sd76030_dump_msg(chip);
	schedule_delayed_work(&chip->sd76030_work, msecs_to_jiffies(10000));
}

static int32_t sd76030_parse_dt(struct sd76030_chip *chip,struct device *dev)
{
	int32_t ret = 0;
	struct device_node *np = dev->of_node;

	if (!np) {
		sd76030_info("no of node\n");
		return -ENODEV;
	}

	if (of_property_read_string(np, "charger_name",&chip->chg_dev_name) < 0) 
	{
		chip->chg_dev_name = "primary_chg";
		sd76030_info("no charger name\n");
	}

	if (of_property_read_string(np, "alias_name",&(chip->chg_props.alias_name)) < 0) 
	{
		chip->chg_props.alias_name = "sd76030";
		sd76030_info("no alias name\n");
	}
	//irq gpio
	ret = of_get_named_gpio(np,"bigm,irq_gpio",0);
	if (ret < 0) 
	{
		sd76030_info("no bigm,irq_gpio(%d)\n", ret);
		chip->irq_gpio = -1;
	} 
	else
		chip->irq_gpio = ret;
	//if config ce gpio, output low
	ret = of_get_named_gpio(np,"bigm,ce_gpio",0);
	if (ret < 0) 
	{
		sd76030_info("no bigm,ce_gpio(%d)\n", ret);
		chip->ce_gpio = -1;
	} 
	else
		chip->ce_gpio = ret;

	sd76030_info("irq_gpio = %d, ce_gpio = %d\n", chip->irq_gpio, chip->ce_gpio);

	if (-1 != chip->ce_gpio) 
	{
		ret = devm_gpio_request_one(chip->dev,chip->ce_gpio,GPIOF_DIR_OUT,devm_kasprintf(chip->dev, GFP_KERNEL,"sd76030_ce_gpio.%s", dev_name(chip->dev)));
		if (ret < 0) 
		{
			sd76030_info("ce gpio request fail(%d)\n", ret);
			return ret;
		}
		//enable ic
		gpio_set_value(chip->ce_gpio,0);
	}

	//ilimit
	ret = of_property_read_u32(np, "bigm,ilimit_ma",&chip->ilimit_ma);
	if(ret)
		chip->ilimit_ma = DEFAULT_ILIMIT;
	//vlimit
	ret = of_property_read_u32(np, "bigm,vlimit_mv",&chip->vlimit_mv);
	if(ret)
		chip->vlimit_mv = DEFAULT_VLIMIT;
	//cc
	ret = of_property_read_u32(np, "bigm,cc_ma",&chip->cc_ma);
	if(ret)
		chip->cc_ma = DEFAULT_CC;
	//cv
	ret = of_property_read_u32(np, "bigm,cv_mv",&chip->cv_mv);
	if(ret)
		chip->cv_mv = DEFAULT_CV;
	//pre ma
	ret = of_property_read_u32(np, "bigm,pre_ma",&chip->pre_ma);
	if(ret)
		chip->pre_ma = DEFAULT_IPRECHG;
	//eoc ma
	ret = of_property_read_u32(np, "bigm,eoc_ma",&chip->eoc_ma);
	if(ret)
		chip->eoc_ma = DEFAULT_ITERM;
	//vsys min mv
	ret = of_property_read_u32(np, "bigm,vsysmin_mv",&chip->vsysmin_mv);
	if(ret)
		chip->vsysmin_mv = DEFAULT_VSYS_MIN;
	//recharge voltage
	ret = of_property_read_u32(np, "bigm,rechg_mv",&chip->rechg_mv);
	if(ret)
		chip->rechg_mv = DEFAULT_RECHG;
	//hvdcp target voltage
	ret = of_property_read_u32(np, "bigm,target_hv",&chip->target_hv);
	if(ret)
		chip->target_hv = 0;

	sd76030_info("ilimit:%dma, vlimit:%dmv, cc:%dma, cv:%dmv, pre:%dma, eoc:%dma, vsysmin:%dmv, vrechg:%dmv, target_hv %dv\n",
					chip->ilimit_ma,chip->vlimit_mv,chip->cc_ma,
					chip->cv_mv,chip->pre_ma,chip->eoc_ma,chip->vsysmin_mv,
					chip->rechg_mv,chip->target_hv);
	return 0;
}

static int32_t	sd76030_default_irq(struct sd76030_chip *chip,uint8_t data)
{
	sd76030_info("%d\n",data);
	return 0;
}
static int32_t	sd76030_chg_state_irq(struct sd76030_chip *chip,uint8_t data)
{
	sd76030_info("%d %s\n",data,charge_state[data]);
	chip->chg_state = data;
	return 0;
}
static int32_t	sd76030_pg_state_irq(struct sd76030_chip *chip,uint8_t data)
{
	int32_t ret = 0;

	sd76030_info("pg good %d\n",data);
#ifdef CONFIG_MTK_EXTERNAL_CHARGER_TYPE_DETECT
	mutex_lock(&chip->bc12_lock);
	atomic_set(&chip->vbus_gd,data);
	ret = (data ? sd76030_bc12_preprocess : sd76030_bc12_postprocess)(chip);
	mutex_unlock(&chip->bc12_lock);
#endif /* CONFIG_MTK_EXTERNAL_CHARGER_TYPE_DETECT */
	return ret;
}
static int32_t	sd76030_vbus_good_irq(struct sd76030_chip *chip,uint8_t data)
{
	int32_t ret = 0;
#ifdef CONFIG_MTK_EXTERNAL_CHARGER_TYPE_DETECT
#if 0
	sd76030_info("data %d\n",data);
	mutex_lock(&chip->bc12_lock);
	atomic_set(&chip->vbus_gd,data);
	ret = (data ? sd76030_bc12_preprocess : sd76030_bc12_postprocess)(chip);
	mutex_unlock(&chip->bc12_lock);
#endif
#endif /* CONFIG_MTK_EXTERNAL_CHARGER_TYPE_DETECT */
 	return ret;
}

static int32_t	sd76030_boost_fault_irq(struct sd76030_chip *chip,uint8_t data)
{
	int32_t ret = 0;
	uint8_t val = 0;
	uint8_t retry_times = 0;
	uint8_t check_times = 0;

	if(1 == data)
	{
		for(retry_times = 0; retry_times < 5; retry_times++)
		{
			sd76030_set_otg_en(chip,0);
			sd76030_set_otg_en(chip,1);
			//check result
			for(check_times = 0; check_times < 5; check_times++)
			{
				ret = sd76030_read_byte(chip,SD76030_R09,&val);
				if(ret < 0)
					continue;

				pr_info("R09 = 0x%02X\n",val);
				if(0 == (val & (CON9_BOOST_STAT_MASK << CON9_BOOST_STAT_SHIFT)))
					break;
			}
			pr_info("check_times = %d\n",check_times);
			//failed, retry
			if(check_times >= 5)
				continue;
			else//successful,break
				break;			
		}
	}
	return 0;
}
static struct sd76030_irq_handle irq_handles[] =
{
	{
		SD76030_R08,0,0,
		{
			{
				.irq_name	= "chg state",
				.irq_func	= sd76030_chg_state_irq,
				.irq_mask   = CON8_CHRG_STAT_MASK,
				.irq_shift	= CON8_CHRG_STAT_SHIFT,
			},
			{
				.irq_name	= "pg state",
				.irq_func	= sd76030_pg_state_irq,
				.irq_mask   = CON8_PG_STAT_MASK,
				.irq_shift	= CON8_PG_STAT_SHIFT,
			},
			{
				.irq_name	= "therm state",
				.irq_func	= sd76030_default_irq,
				.irq_mask   = CON8_THM_STAT_MASK,
				.irq_shift	= CON8_THM_STAT_SHIFT,
			},
			{
				.irq_name	= "vsys state",
				.irq_func	= sd76030_default_irq,
				.irq_mask   = CON8_VSYS_STAT_MASK,
				.irq_shift	= CON8_VSYS_STAT_SHIFT,
			},
		},
	},

	{
		SD76030_R09,0,0,
		{
			{
				.irq_name	= "wathc dog fault",
				.irq_func	= sd76030_default_irq,
				.irq_mask   = CON9_WATG_STAT_MASK,
				.irq_shift	= CON9_WATG_STAT_SHIFT,
			},
			{
				.irq_name	= "boost fault",
				.irq_func	= sd76030_boost_fault_irq,
				.irq_mask   = CON9_BOOST_STAT_MASK,
				.irq_shift	= CON9_BOOST_STAT_SHIFT,
			},
			{
				.irq_name	= "chg fault",
				.irq_func	= sd76030_default_irq,
				.irq_mask   = CON9_CHRG_FAULT_MASK,
				.irq_shift	= CON9_CHRG_FAULT_SHIFT,
			},
			{
				.irq_name	= "bat fault",
				.irq_func	= sd76030_default_irq,
				.irq_mask   = CON9_BAT_STAT_MASK,
				.irq_shift	= CON9_BAT_STAT_SHIFT,
			},
			{
				.irq_name	= "ntc fault",
				.irq_func	= sd76030_default_irq,
				.irq_mask   = CON9_NTC_STAT_MASK,
				.irq_shift	= CON9_NTC_STAT_SHIFT,
			},
		},
	},
	{
		SD76030_R0A,0,0,
		{
			 {
			 	.irq_name	= "vbus good",
			 	.irq_func	= sd76030_vbus_good_irq,
			 	.irq_mask   = CONA_VBUS_GD_MASK,
			 	.irq_shift	= CONA_VBUS_GD_SHIFT,
			},
			{
				.irq_name	= "vdpm",
				.irq_func	= sd76030_default_irq,
				.irq_mask   = CONA_VINDPM_STAT_MASK,
				.irq_shift	= CONA_VINDPM_STAT_SHIFT,
			},
			{
				.irq_name	= "idpm",
				.irq_func	= sd76030_default_irq,
				.irq_mask   = CONA_IDPM_STAT_MASK,
				.irq_shift	= CONA_IDPM_STAT_SHIFT,
			},
			{
				.irq_name	= "topoff active",
				.irq_func	= sd76030_default_irq,
				.irq_mask   = CONA_TOPOFF_ACTIVE_MASK,
				.irq_shift	= CONA_TOPOFF_ACTIVE_SHIFT,
			},
			{
				.irq_name	= "acov state",
				.irq_func	= sd76030_default_irq,
				.irq_mask   = CONA_ACOV_STAT_MASK,
				.irq_shift	= CONA_ACOV_STAT_SHIFT,
			},
		},
	},
};

static int32_t sd76030_process_irq(struct sd76030_chip *chip)
{
	int32_t i, j,ret = 0;
	uint8_t pre_state=0 ,now_state=0;
	int32_t handler_count = 0;

	if (-1 == chip->irq_gpio) 
		return 0;

	for (i=0; i<ARRAY_SIZE(irq_handles); i++) 
	{
		ret = sd76030_read_byte(chip,irq_handles[i].reg,&irq_handles[i].value);
		sd76030_info("reg 0x%02X,value now 0x%02X, value pre 0x%02X\n",irq_handles[i].reg,irq_handles[i].value,irq_handles[i].pre_value);

		if (ret < 0) 
		{
			dev_err(chip->dev, "Couldn't read reg 0x%02X ret=%d\n",irq_handles[i].reg,ret);
			continue;
		}

		for (j=0; j<ARRAY_SIZE(irq_handles[i].irq_info); j++) 
		{
			now_state = irq_handles[i].value & (irq_handles[i].irq_info[j].irq_mask << irq_handles[i].irq_info[j].irq_shift);
			pre_state = irq_handles[i].pre_value & (irq_handles[i].irq_info[j].irq_mask << irq_handles[i].irq_info[j].irq_shift); 

			now_state >>= irq_handles[i].irq_info[j].irq_shift;
			pre_state >>= irq_handles[i].irq_info[j].irq_shift;

			if(now_state != pre_state)
			{
				pr_info("%s change %d -> %d\n", irq_handles[i].irq_info[j].irq_name,pre_state,now_state);

				if (irq_handles[i].irq_info[j].irq_func)
				{
					handler_count++;
					ret = irq_handles[i].irq_info[j].irq_func(chip, now_state);
					if (ret < 0)
						dev_err(chip->dev,"Couldn't handle %d irq for reg 0x%02X ret = %d\n",j, irq_handles[i].reg, ret);
				}	
			}
		}

		irq_handles[i].pre_value = irq_handles[i].value;
	}

	return handler_count;
}

static irqreturn_t sd76030_irq_handler(int32_t irq, void *data)
{
	struct sd76030_chip *chip = (struct sd76030_chip *)data;

	sd76030_info("--------------------------\n");

	//wait for register modify
	msleep(50);
	pm_stay_awake(chip->dev);
	mutex_lock(&chip->irq_lock);
	sd76030_process_irq(chip);
	mutex_unlock(&chip->irq_lock);
	pm_relax(chip->dev);

	return IRQ_HANDLED;
}


static int32_t sd76030_register_irq(struct sd76030_chip *chip)
{
	int32_t ret = 0;

	sd76030_info("irq gpio %d\n",chip->irq_gpio);

	if (-1 == chip->irq_gpio) 
		return 0;

	ret = devm_gpio_request_one(chip->dev, chip->irq_gpio, GPIOF_DIR_IN,
										devm_kasprintf(chip->dev, GFP_KERNEL,
										"sd76030_irq_gpio.%s", dev_name(chip->dev)));
	if (ret < 0) 
	{
		sd76030_info("gpio request fail(%d)\n", ret);
		return ret;
	}

	chip->irq = gpio_to_irq(chip->irq_gpio);
	if (chip->irq < 0) 
	{
		sd76030_info("gpio2irq fail(%d)\n", chip->irq);
		return chip->irq;
	}

	sd76030_info("irq = %d\n", chip->irq);

	/* Request threaded IRQ */
	ret = devm_request_threaded_irq(chip->dev, chip->irq, NULL,
					sd76030_irq_handler,
					IRQF_ONESHOT | IRQF_TRIGGER_FALLING, 
					devm_kasprintf(chip->dev, GFP_KERNEL,
					"sd76030_irq.%s", dev_name(chip->dev)),
					chip);

	if (ret < 0) 
	{
		sd76030_info("request threaded irq fail(%d)\n", ret);
		return ret;
	}
	enable_irq_wake(chip->irq);
	return ret;
}

/*****************************************************************************
 * Description: sd76030 show registers
 *****************************************************************************/
static uint8_t attr_reg = 0xFF;
static ssize_t show_registers(struct device *dev,struct device_attribute *attr, char *buf)
{
	size_t count = 0;
	uint8_t i = 0, data = 0;
	int32_t ret  = 0;
	struct sd76030_chip *chip = i2c_get_clientdata(to_i2c_client(dev));
	if(0xFF == attr_reg)
	{
		for (i = 0; i < SD76030_REG_NUM; i++) 
		{
			data = 0;
			ret = sd76030_read_byte(chip,i, &data);
			count += sprintf(&buf[count], "Reg[0x%02X] = 0x%02X %s\n",i,data,(ret < 0) ? "failed":"successfully");
		}

		ret = sd76030_write_byte(chip,SD76030_R11,0x80);//one shot
		if(ret < 0)
			count += sprintf(&buf[count], "write R11 failed\n");
		else
		{
			msleep(1000);
			for (i = SD76030_R11; i <= SD76030_R17; i++) 
			{
				data = 0;
				ret = sd76030_read_byte(chip,i, &data);
				count += sprintf(&buf[count], "Reg[0x%02X] = 0x%02X %s\n",i,data,(ret < 0) ? "failed":"successfully");
			}	
		}
	}
	else
	{
		data = 0;
		ret = sd76030_read_byte(chip,attr_reg, &data);
		count += sprintf(&buf[count], "Reg[0x%02X] = 0x%02X %s\n",attr_reg,data,(ret < 0) ? "failed":"successfully");
	}

	return count;
}

static ssize_t store_registers(struct device *dev,struct device_attribute *attr,const char *buf, size_t size)
{
	int32_t ret = 0;
	char *pvalue = NULL, *addr = NULL, *val = NULL;
	uint32_t reg_value = 0,reg_address = 0;
	struct sd76030_chip *chip = i2c_get_clientdata(to_i2c_client(dev));

	if(NULL == buf || (6 != size && 3 != size))
		goto msg;

	pvalue = (char *)buf;
	addr = strsep(&pvalue, " ");
	ret = kstrtou32(addr, 16,(uint32_t *)&reg_address);
	sd76030_info("st_dbg: store_registers-0- ret=%d\n",ret);
	//read
	if(3 == size)
	{
		//just want read some register
		attr_reg = reg_address;
		sd76030_info("You want read reg 0x%02X, please cat registers!\n",(uint32_t) attr_reg);
	}
	else
	{
		//you can read register immediately��after write
		attr_reg = reg_address;

		val  = strsep(&pvalue, " ");
		ret = kstrtou32(val, 16, (uint32_t *)&reg_value);

		sd76030_info("ret = %d , write sd76030 reg 0x%02X with value 0x%02X !\n",ret,(uint32_t) reg_address, reg_value);

		sd76030_write_byte(chip,reg_address,reg_value);
	}
	return size;
msg:
	sd76030_info("exsample: echo 10 22 > registers\n");
	sd76030_info("means   : write reg 0x10 with value 0x22!\n");
	sd76030_info("exsample: echo 10 > registers\n");
	sd76030_info("means   : cat registers will read 0x10\n");
	sd76030_info("exsample: echo ff > registers\n");
	sd76030_info("means   : cat registers will read all\n");
	return size;
}

/*****************************************************************************
 * Description: sd76030_sys_set_charger_en
 * Parameters: write example: echo 1 > set_charger_en ---enable charger
 *****************************************************************************/
static ssize_t sd76030_sys_set_charger_en(struct device *dev, struct device_attribute *attr, const char *buf, size_t _count)
{
	int32_t val = 0;
	struct sd76030_chip *chip  = i2c_get_clientdata(to_i2c_client(dev));
	
	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	if(val == 1)
	{
		sd76030_set_charger_en(chip,1);
		pr_info("CHARGE EN ON \n");
	}
	else if (val == 0)
	{
		sd76030_set_charger_en(chip,0);
		pr_info("CHARGE EN CLOSE \n");
	}
	else
	{
		sd76030_info("invalid command\n");
		return -EINVAL;
	}
	return _count;
}

static DEVICE_ATTR(registers, 0664, show_registers,store_registers);	/* 664 */
static DEVICE_ATTR(sd76030_charger_en,S_IRUGO | (S_IWUSR|S_IWGRP), NULL, sd76030_sys_set_charger_en);
static struct attribute *sd76030_attributes[] = {
	&dev_attr_registers.attr,
	&dev_attr_sd76030_charger_en.attr,
	NULL,
};

static struct attribute_group sd76030_attribute_group = {
	.attrs = sd76030_attributes,
};

static enum power_supply_property sd76030_props[] = {
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_TYPE,
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE, //cv
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT, //vbus current
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX,	//cc
	POWER_SUPPLY_PROP_USB_TYPE,
	POWER_SUPPLY_PROP_MODEL_NAME,
	POWER_SUPPLY_PROP_MANUFACTURER,
};

static int32_t sd76030_get_prop(struct power_supply *psy,enum power_supply_property psp,union power_supply_propval *val)
{
	int32_t ret = 0;
	struct sd76030_chip *chip = power_supply_get_drvdata(psy);
	val->intval = 0;

	switch (psp) 
	{
		case POWER_SUPPLY_PROP_PRESENT:
		case POWER_SUPPLY_PROP_ONLINE:
			ret = sd76030_get_vbus_state(chip);
			if(ret == 1)
				val->intval = 1;
			else
				val->intval = 0;
			break;

		case POWER_SUPPLY_PROP_TYPE:
			val->intval = psy->desc->type;
			break;

		case POWER_SUPPLY_PROP_STATUS:
			ret = sd76030_get_charger_status(chip);
			if (ret == 3)
				val->intval = POWER_SUPPLY_STATUS_FULL;
			else if (ret == 0)
				val->intval = POWER_SUPPLY_STATUS_NOT_CHARGING;
			else
				val->intval = POWER_SUPPLY_STATUS_CHARGING;
			break;

		case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:	//cv
			ret = sd76030_get_vreg_volt(chip);
			val->intval = (ret < 0) ? 0 : (ret * 1000);
			break;

		case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
			ret = sd76030_get_input_curr_lim(chip);
			val->intval = (ret < 0) ? 0 : (ret * 1000);
			break;

		case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
			ret = sd76030_get_ichg_current(chip);
			val->intval = (ret < 0) ? 0 : (ret * 1000);
			break;
#ifdef CONFIG_MTK_EXTERNAL_CHARGER_TYPE_DETECT
		case POWER_SUPPLY_PROP_USB_TYPE:
			val->intval = chip->chg_type;
			break;
#endif
		case POWER_SUPPLY_PROP_MODEL_NAME:
			val->strval = "sd76030";
			break;

		case POWER_SUPPLY_PROP_MANUFACTURER:
			val->strval = "Big Moment Technology";
			break;
		default:
			pr_err("get prop %d is not supported in usb\n", psp);
			ret = -EINVAL;
			break;
	}

	if (ret < 0) 
	{
		pr_debug("Couldn't get prop %d rc = %d\n", psp, ret);
		return -ENODATA;
	}
	return 0;
}

static int32_t sd76030_set_prop(struct power_supply *psy,enum power_supply_property psp,const union power_supply_propval *val)
{
	int32_t ret = 0;
	struct sd76030_chip *chip = power_supply_get_drvdata(psy);
	
	switch (psp) 
	{
#ifdef CONFIG_MTK_EXTERNAL_CHARGER_TYPE_DETECT		
		case POWER_SUPPLY_PROP_ONLINE:
			chip->attach = val->intval;
			break;
#endif
		case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:		//cv
			ret = sd76030_set_vreg_volt(chip,val->intval / 1000);
			break;

		case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT: //ilimit
			ret = sd76030_set_input_curr_lim(chip,val->intval / 1000);
			break;

		case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX: //cc
			ret = sd76030_set_ichg_current(chip,val->intval / 1000);
			break;

		default:
			pr_err("set prop %d is not supported\n", psp);
			ret = -EINVAL;
			break;
	}

	return ret;
}

static int32_t sd76030_prop_is_writeable(struct power_supply *psy,enum power_supply_property psp)
{
	switch (psp)
	{
		case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
		case POWER_SUPPLY_PROP_ONLINE:
			return 1;
		default:
			break;
	}

	return 0;
}

static char *sd76030_supplied_to[] =
{
	"battery",
	"mtk-master-charger"
};
static enum power_supply_usb_type sd76030_chg_psy_usb_types[] = {
	POWER_SUPPLY_USB_TYPE_UNKNOWN,
	POWER_SUPPLY_USB_TYPE_SDP,
	POWER_SUPPLY_USB_TYPE_CDP,
	POWER_SUPPLY_USB_TYPE_DCP,
};
static const struct power_supply_desc sd76030_psy_desc =
{
	.name = "sd76030",
	.type = POWER_SUPPLY_TYPE_USB,
	.usb_types = sd76030_chg_psy_usb_types,
	.num_usb_types = ARRAY_SIZE(sd76030_chg_psy_usb_types),
	.properties = sd76030_props,
	.num_properties = ARRAY_SIZE(sd76030_props),
	.get_property = sd76030_get_prop,
	.set_property = sd76030_set_prop,
	.property_is_writeable = sd76030_prop_is_writeable,
};

static int32_t sd76030_power_init(struct sd76030_chip *chip)
{
	struct power_supply_config cfg = {};

	cfg.drv_data = chip;
	cfg.of_node  = chip->dev->of_node;
	cfg.supplied_to = sd76030_supplied_to;
	cfg.num_supplicants = ARRAY_SIZE(sd76030_supplied_to);
	
	memcpy(&chip->psy_desc, &sd76030_psy_desc, sizeof(chip->psy_desc));

	chip->sd76030_psy = devm_power_supply_register(chip->dev,&chip->psy_desc,&cfg);

	if (IS_ERR(chip->sd76030_psy)) 
	{
		pr_err("Couldn't register power supply\n");
		return PTR_ERR(chip->sd76030_psy);
	}

	return 0;
}

//add for otg regulator interface start

static int sd76030_enable_vbus(struct regulator_dev *rdev)
{
	int ret =0;
	struct sd76030_chip *chip = rdev->reg_data;
	
	ret = sd76030_set_otg_en(chip,1);
	printk("sd76030 enable OTG,ret=%d\n",ret);

	return 0;
}

static int sd76030_disable_vbus(struct regulator_dev *rdev)
{
	int ret =0;
	struct sd76030_chip *chip = rdev->reg_data;
	
	ret = sd76030_set_otg_en(chip,0);
	printk("sd76030 disable OTG,ret=%d\n",ret);

	return 0;
}

static int sd76030_is_enabled_vbus(struct regulator_dev *rdev)
{
	int status;
	struct sd76030_chip *chip = rdev->reg_data;	
	
	status = sd76030_get_otg_config(chip);
	printk("---%s--%d-\n",__func__,status);
	return status;
}

/*
static int sd76030_list_voltage_linear(struct regulator_dev *rdev,unsigned int selector)
{
	return 1;
}

static int sd76030_set_voltage_sel(struct regulator_dev *rdev, unsigned sel)
{
	return 1;
}

static int sd76030_get_voltage_sel(struct regulator_dev *rdev)
{
	return 5000000;
}
*/

static int sd76030_set_otg_current_limit(struct regulator_dev *rdev,int min_uA, int max_uA)
{
	int ret =0;
	int set_cur = 500;
	struct sd76030_chip *chip = rdev->reg_data;	
	
	if (min_uA!=max_uA)
		printk("sd76030 set otg current not right(%d)!=(%d)\n",min_uA,max_uA);
	
	set_cur = min_uA/1000;
	ret = sd76030_set_boost_ilim(chip,set_cur);
	printk("sd76030_set_otg_current_limit:%d  ret:%d\n",set_cur,ret);
	return ret;
}

static int sd76030_get_otg_current_limit(struct regulator_dev *rdev)
{
	int ret =0;
	struct sd76030_chip *chip = rdev->reg_data;	

	ret = sd76030_get_boost_ilim(chip)*1000;
	printk("sd76030_get_otg_current_limit:%d\n",ret);	
	return ret;
}

static const struct regulator_ops sd76030_vbus_ops = {
	.enable = sd76030_enable_vbus,
	.disable = sd76030_disable_vbus,
	.is_enabled = sd76030_is_enabled_vbus,
	//.list_voltage = sd76030_list_voltage_linear,
	//.set_voltage_sel = sd76030_set_voltage_sel,
	//.get_voltage_sel = sd76030_get_voltage_sel,
	.set_current_limit = sd76030_set_otg_current_limit,
	.get_current_limit = sd76030_get_otg_current_limit,
};

static const struct regulator_desc sd76030_otg_rdesc = {
	.of_match = "usb-otg-vbus",
	.name = "usb-otg-vbus",
	.ops = &sd76030_vbus_ops,
	.owner = THIS_MODULE,
	.type = REGULATOR_VOLTAGE,
	.fixed_uV = 5150000,
	//.min_uV = 4500000,
	//.uV_step = 0,	
	.n_voltages = 1,

};

static const struct regulator_init_data sd76030_vbus_init_data = {
        .constraints = {
        .valid_ops_mask = REGULATOR_CHANGE_STATUS|REGULATOR_CHANGE_CURRENT,//|REGULATOR_CHANGE_VOLTAGE,
        .min_uA = 500000,
        .max_uA = 1200000,
        //.min_uV = 4500000,
        //.max_uV = 5000000,
        },
};

static int sd76030_vbus_regulator_register(struct sd76030_chip *chip)
{
	struct regulator_config config = {};
	int ret = 0;
	/* otg regulator */
	config.dev = chip->dev;
	config.driver_data = chip;
	config.init_data = &sd76030_vbus_init_data;
	chip->otg_rdev =
		devm_regulator_register(chip->dev, &sd76030_otg_rdesc, &config);
	//chip->otg_rdev->constraints->valid_ops_mask = REGULATOR_CHANGE_STATUS|REGULATOR_CHANGE_CURRENT;//|= REGULATOR_CHANGE_STATUS;
	if (IS_ERR(chip->otg_rdev)) {
		ret = PTR_ERR(chip->otg_rdev);
		printk("%s: register otg regulator failed (%d)-\n", __func__,ret);
	}
	return ret;
}

//add for otg regulator interface end



static int32_t sd76030_driver_probe(struct i2c_client *client)
{
	int32_t ret = 0;
	struct sd76030_chip *chip = NULL;
    
	sd76030_info("start\n");

	chip = devm_kzalloc(&client->dev, sizeof(struct sd76030_chip),GFP_KERNEL);
	if (!chip)
		return -ENOMEM; 

	chip->client = client;
	chip->dev = &client->dev;
	g_sd76030_chip = chip;
	//init mutex asap,before iic operate
	mutex_init(&chip->i2c_rw_lock);
	mutex_init(&chip->adc_lock);
	mutex_init(&chip->irq_lock);
	i2c_set_clientdata(client, chip);
	//get chg_dev_name as soon as possible
	ret = sd76030_parse_dt(chip, &client->dev);
	if (ret < 0)
		pr_err("sd76030_parse_dt failed ret %d\n",ret);

	ret = sd76030_detect_ic(chip);
	if(ret < 0)
	{
		sd76030_info("do not detect ic, exit\n");
		return -ENODEV;
	}
	sd76030_get_component_id(chip);

	sd76030_hw_init(chip);


#ifdef CONFIG_MTK_EXTERNAL_CHARGER_TYPE_DETECT
	sd76030_info("prepare for bc1.2\n");
	mutex_init(&chip->bc12_lock);
	INIT_DELAYED_WORK(&chip->psy_dwork, sd76030_inform_psy_dwork_handler);
	atomic_set(&chip->vbus_gd, 0);
	chip->attach = false;
	chip->port = SD76030_PORTSTAT_NO_INPUT;
	chip->chg_type = CHARGER_UNKNOWN;
	atomic_set(&chip->bc12_en_req_cnt, 0);
	init_waitqueue_head(&chip->bc12_en_req);
	chip->bc12_en_kthread = kthread_run(sd76030_bc12_en_kthread,chip,"sd76030_bc12");
	if (IS_ERR_OR_NULL(chip->bc12_en_kthread)) 
	{
		ret = PTR_ERR(chip->bc12_en_kthread);
		sd76030_info("kthread run fail(%d)\n", ret);
	}
#endif /* CONFIG_MTK_EXTERNAL_CHARGER_TYPE_DETECT */
	/* Register charger device */
	chip->chg_dev = charger_device_register(chip->chg_dev_name,&client->dev,chip,&sd76030_chg_ops,&chip->chg_props);
	if (IS_ERR_OR_NULL(chip->chg_dev)) 
	{
		ret = PTR_ERR(chip->chg_dev);
		sd76030_info("register chg dev fail(%d)\n", ret);
	}
	ret = sd76030_power_init(chip);
	if (ret < 0) 
		pr_err("register power supply fail(%d)\n", ret);

	ret = sd76030_register_irq(chip);
	if (ret < 0) 
		sd76030_info("register irq fail(%d)\n", ret);
	//process irq once when probe
	ret = sd76030_process_irq(chip);
	if (ret < 0) 
		sd76030_info("process irq fail(%d)\n", ret);

	device_init_wakeup(chip->dev, true);

	sd76030_vbus_regulator_register(chip);

	ret = sysfs_create_group(&(chip->dev->kobj), &sd76030_attribute_group);
	if (ret < 0) 
		sd76030_info("create sysfs failed %d\n",ret);
			
	INIT_DELAYED_WORK(&chip->sd76030_work, sd76030_work_func);
	schedule_delayed_work(&chip->sd76030_work, 2*HZ);	
	sd76030_info("end\n"); 
	return 0;
}

static int32_t sd76030_suspend(struct device *dev)
{
	struct sd76030_chip *chip = i2c_get_clientdata(to_i2c_client(dev));

	sd76030_info("\n");
	
	if (device_may_wakeup(dev) && chip->irq >= 0)
		enable_irq_wake(chip->irq);

	if(chip->irq >= 0)
		disable_irq(chip->irq);

	return 0;
}
static int32_t sd76030_resume(struct device *dev)
{
	struct sd76030_chip *chip = i2c_get_clientdata(to_i2c_client(dev));
	sd76030_info("\n");

	if(chip->irq >= 0)
		enable_irq(chip->irq);

	if (device_may_wakeup(dev) && chip->irq >= 0)
		disable_irq_wake(chip->irq);

	return 0;
}
static void sd76030_driver_remove(struct i2c_client *client)
{
	struct sd76030_chip *chip = i2c_get_clientdata(client);

	if(chip->irq >= 0)
		disable_irq(chip->irq);
	//if(-1 != chip->irq_gpio)
		//devm_gpio_free(chip->dev,chip->irq_gpio);

	charger_device_unregister(chip->chg_dev);
	sysfs_remove_group(&(chip->dev->kobj), &sd76030_attribute_group);
#ifdef CONFIG_MTK_EXTERNAL_CHARGER_TYPE_DETECT
	cancel_delayed_work_sync(&chip->psy_dwork);
	if (chip->psy)
		power_supply_put(chip->psy);

	kthread_stop(chip->bc12_en_kthread);
	mutex_destroy(&chip->bc12_lock);
#endif /* CONFIG_MTK_EXTERNAL_CHARGER_TYPE_DETECT */
	sd76030_set_dpdm(chip,CONC_DP_DM_VOL_HIZ,CONC_DP_DM_VOL_HIZ);
	mutex_destroy(&chip->adc_lock);
	mutex_destroy(&chip->i2c_rw_lock);
	return;
}
static void sd76030_driver_shutdown(struct i2c_client *client)
{
	struct sd76030_chip *chip = i2c_get_clientdata(client);
	// sd76030_set_charger_en(chip,0);
	sysfs_remove_group(&(chip->dev->kobj), &sd76030_attribute_group);
	sd76030_set_dpdm(chip,CONC_DP_DM_VOL_HIZ,CONC_DP_DM_VOL_HIZ);
}
static const struct dev_pm_ops sd76030_pm_ops = {
	.resume			= sd76030_resume,
	.suspend			= sd76030_suspend,
};
static const struct of_device_id sd76030_of_match[] =
{
	{.compatible = "bigmtech,sd76030"},
	{},
};
static const struct i2c_device_id sd76030_i2c_id[] =
{ 
	{"sd76030",   0}, 
};
static struct i2c_driver sd76030_driver =
{
	.driver =
	{
		.name 			= "sd76030",
		.owner 		= THIS_MODULE,
		.pm			= &sd76030_pm_ops,
		.of_match_table 	= sd76030_of_match,
	},
	.id_table 	= sd76030_i2c_id,
	.probe 	= sd76030_driver_probe,
	.remove	= sd76030_driver_remove,
	.shutdown	= sd76030_driver_shutdown,
};

static int32_t __init sd76030_init(void)
{
	int32_t ret = 0;
	ret = i2c_add_driver(&sd76030_driver);

	if (0 != ret) 
		sd76030_info("failed to register sd76030 i2c driver.\n");
	else 
		sd76030_info("Success to register sd76030 i2c driver.\n");
	
	return ret;
}

static void __exit sd76030_exit(void)
{
	i2c_del_driver(&sd76030_driver);
}
module_init(sd76030_init);
module_exit(sd76030_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("I2C sd76030 Driver");

