// SPDX-License-Identifier: GPL-2.0

/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/types.h>
#include <linux/init.h>   /* For init/exit macros */
#include <linux/module.h> /* For MODULE_ marcros  */
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#endif
// #include <mt-plat/mtk_boot.h>
// #include <mt-plat/upmu_common.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/power_supply.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/machine.h>
#include "cx25890.h"
#include "charger_class.h"
#include "mtk_charger.h"


#include <mtk_musb.h>

#include <linux/usb/otg.h>
#include <linux/usb/ulpi.h>
#include <linux/usb/ch9.h>
#include <linux/usb/gadget.h>
#include <linux/usb/phy.h>

static int cx25890_is_enabled_vbus(struct regulator_dev *rdev);
static int cx25890_disable_vbus(struct regulator_dev *rdev);

static int CHARGER = 0;

extern void Charger_Detect_Init(void);
extern void Charger_Detect_Release(void);
static int cx25890_get_vbus(struct charger_device *chg_dev, u32 *vbus);
static int cx25890_get_ibus(struct charger_device *chg_dev, u32 *ibus);
#define CX25890_REG_NUM (0x15)

static const unsigned int BOOST_CURRENT_LIMIT[] =
    {
        500000, 750000, 1200000, 1400000,
        1650000, 1875000, 2150000, 2450000};

static enum power_supply_usb_type cx25890_usb_type[] =
    {
        POWER_SUPPLY_USB_TYPE_UNKNOWN,
        POWER_SUPPLY_USB_TYPE_SDP,
        POWER_SUPPLY_USB_TYPE_DCP,
        POWER_SUPPLY_USB_TYPE_CDP,
};

static const struct charger_properties cx25890_chg_props =
    {
        .alias_name = CX25890_NAME,
};

/**********************************************************
 *
 *   [Global Variable]
 *
 *********************************************************/
static struct power_supply_desc cx25890_power_supply_desc;
static struct charger_device *s_chg_dev_otg;

// static bool is_probe_detect_usb = false;

// #define PHY_MODE_BC11_SET 1
// #define PHY_MODE_BC11_CLR 2

// enum cx2560x_usbsw_state {
// 	USBSW_CHG = 0,
// 	USBSW_USB,
// };

// static int cx2589x_set_usbsw_state(struct cx25890_device *cx, int state)
// {
// 	struct phy *phy2;

// 	phy2 = phy_get(cx->dev, "usb2-phy");
// 	// phy3 = phy_get(cx->dev, "usb3-phy");

// 	switch (state) {
// 	case USBSW_CHG:
// 		/* 断开平台的dpdm */
// 		if (IS_ERR_OR_NULL(phy2))
// 			phy_set_mode_ext(phy2, PHY_MODE_USB_DEVICE, PHY_MODE_BC11_SET);
// 		break;
// 	case USBSW_USB:
// 		/* 连接平台的dpdm */
// 		if (IS_ERR_OR_NULL(phy2))
// 			phy_set_mode_ext(phy2, PHY_MODE_USB_DEVICE, PHY_MODE_BC11_CLR);
// 		break;
// 	default:
// 		break;
// 	}

// 	if (IS_ERR_OR_NULL(phy2))
// 		phy_put(cx->dev, phy2);

// 	return 0;
// }

/**********************************************************
 *
 *   [I2C Function For Read/Write cx25890]
 *
 *********************************************************/

static int cx25890_write_reg_without_retry(struct cx25890_device *cx, u8 reg, u8 val)
{
    s32 ret;
    mutex_lock(&cx->i2c_rw_lock);
    ret = i2c_smbus_write_byte_data(cx->client, reg, val);
    if (ret < 0)
    {
        pr_err("i2c write fail: can't write 0x%02X to reg 0x%02X: %d\n",
               val, reg, ret);
		mutex_unlock(&cx->i2c_rw_lock);
        return ret;
    }
    mutex_unlock(&cx->i2c_rw_lock);
    return 0;
}

static int cx25890_read_reg_without_retry(struct cx25890_device *cx, u8 reg, u8 *data)
{
    s32 ret;
    mutex_lock(&cx->i2c_rw_lock);
    ret = i2c_smbus_read_byte_data(cx->client, reg);
    if (ret < 0)
    {
        pr_err("i2c read fail: can't read from reg 0x%02X\n", reg);
		mutex_unlock(&cx->i2c_rw_lock);
        return ret;
    }

    *data = (u8)ret;
    mutex_unlock(&cx->i2c_rw_lock);
    return 0;
}

static int cx25890_read_reg(struct cx25890_device *cx, u8 reg, u8 *data)
{
    s32 ret;

    u8 i, retry_count;
    u8 tmp_val[2];
    u8 reg_val;

    for (retry_count = 0; retry_count < 3; retry_count++)
    {
        for (i = 0; i < 2; i++)
        {
            ret = cx25890_read_reg_without_retry(cx, reg, &reg_val);
            if (ret < 0)
                break;
            else
                tmp_val[i] = reg_val;
        }
        if (ret >= 0)
        {
            if (tmp_val[0] == tmp_val[1])
            {
                *data = tmp_val[0];
                return 0;
            }
        }
    }
    pr_err("i2c retry read fail: can't read from reg 0x%02X\n", reg);
    return -1;
}

static int cx25890_write_reg(struct cx25890_device *cx, u8 reg, u8 val)
{
    s32 ret;
    u8 read_val = 0, retry_count;

    for (retry_count = 0; retry_count < 10; retry_count++)
    {
        ret = cx25890_write_reg_without_retry(cx, reg, val);
        if (ret >= 0)
        {
            ret = cx25890_read_reg_without_retry(cx, reg, &read_val);

            if (read_val == val)
                break;
            if (retry_count == 9)
            {
                pr_err("i2c retry write fail: can't write 0x%02X to reg 0x%02X: %d\n", val, reg, ret);
                return -1;
            }
        }
    }
    return 0;
}

static int cx25890_update_bits(struct cx25890_device *cx, u8 reg,
                               u8 mask, u8 val)
{
    int ret = -1;
    u8 tmp;

    ret = cx25890_read_reg(cx, reg, &tmp);
    if (ret)
        return ret;

    tmp &= ~mask;
    tmp |= val & mask;

    if ((reg == 0x02 && mask == 0x80) || (reg == 0x02 && mask == 0x02) ||
        (reg == 0x03 && mask == 0x40) || (reg == 0x09 && mask == 0x80) ||
        (reg == 0x09 && mask == 0x02) || (reg == 0x09 && mask == 0x01) ||
        (reg == 0x14 && mask == 0x80) || (reg == 0x09 && mask == 0x20) ||
		(reg == 0x00 && mask == 0x3f) )
    {
        ret = cx25890_write_reg_without_retry(cx, reg, tmp);
    }
    else
    {
        ret = cx25890_write_reg(cx, reg, tmp);
    }

    if (ret)
        pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);

    return ret;
}

static int cx25890_write_reg40(struct cx25890_device *cx, bool enable)
{
    int ret, try_count = 10;
    u8 read_val;

    if (enable)
    {
        while (try_count--)
        {
            ret = cx25890_write_reg_without_retry(cx, 0x40, 0x00);
            ret = cx25890_write_reg_without_retry(cx, 0x40, 0x50);
            ret = cx25890_write_reg_without_retry(cx, 0x40, 0x57);
            ret = cx25890_write_reg_without_retry(cx, 0x40, 0x44);

            cx25890_read_reg_without_retry(cx, 0x40, &read_val);
            if (0x03 == read_val)
            {
                ret = 1;
                break;
            }
        }
    }
    else
    {
        ret = cx25890_write_reg_without_retry(cx, 0x40, 0x00);
    }
    return ret;
}

/**********************************************************
 *
 *   [Internal Function]
 *
 *********************************************************/

/* WDT TIMER SET  */
#define CX25890_WDT_TIMER_MASK 0x30
#define CX25890_WDT_TIMER_DISABLE 0
#define CX25890_WDT_TIMER_40S 0x10
#define CX25890_WDT_TIMER_80S 0x20
#define CX25890_WDT_TIMER_160S 0x30

#define CX25890_WDT_RST_MASK 0x40
static int cx25890_set_watchdog_timer(struct cx25890_device *cx, int time)
{
    int ret;
    u8 reg_val;

    if (time == 0)
        reg_val = CX25890_WDT_TIMER_DISABLE;
    else if (time == 40)
        reg_val = CX25890_WDT_TIMER_40S;
    else if (time == 80)
        reg_val = CX25890_WDT_TIMER_80S;
    else
        reg_val = CX25890_WDT_TIMER_160S;

    ret = cx25890_update_bits(cx, CX25890_CHRG_CTRL_7,
                              CX25890_WDT_TIMER_MASK, reg_val);

    return ret;
}

static int cx25890_set_term_curr(struct charger_device *chg_dev, u32 uA)
{
    u8 reg_val;
    struct cx25890_device *cx = charger_get_data(chg_dev);

    if (uA < 64000)
        uA = 64000;
    else if (uA > 1024000)
        uA = 1024000;
    reg_val = (uA - 64000) / 64000;
    return cx25890_update_bits(cx, CX25890_CHRG_CTRL_5,
                               CX25890_TERMCHRG_CUR_MASK, reg_val);
}

static int cx25890_set_prechrg_curr(struct cx25890_device *cx, int uA)
{
    u8 reg_val;

    if (uA < 64000)
        uA = 64000;
    else if (uA > 1024000)
        uA = 1024000;
    reg_val = (uA - 64000) / 64000;
    reg_val = reg_val << 4;
    reg_val = 0xb0;
    return cx25890_update_bits(cx, CX25890_CHRG_CTRL_5,
                               CX25890_PRECHRG_CUR_MASK, reg_val);
}

static int cx25890_get_ichg_curr(struct charger_device *chg_dev, u32 *uA)
{
    int ret;
    u8 ichg;
    u32 curr;

    struct cx25890_device *cx = charger_get_data(chg_dev);

    ret = cx25890_read_reg(cx, CX25890_CHRG_CTRL_4, &ichg);
    if (ret)
        return ret;

    ichg &= CX25890_ICHRG_I_MASK;

    curr = ichg * CX25890_ICHRG_I_STEP_uA;

    *uA = curr;
    return 0;
}

static int cx25890_get_minichg_curr(struct charger_device *chg_dev, u32 *uA)
{

    *uA = CX25890_ICHRG_I_MIN_uA;
    return 0;
}

static int cx25890_set_ichrg_curr(struct charger_device *chg_dev, unsigned int uA)
{
    int ret;
    u8 reg_val;
    struct cx25890_device *cx = charger_get_data(chg_dev);

    if (uA < CX25890_ICHRG_I_MIN_uA)
        uA = CX25890_ICHRG_I_MIN_uA;
    else if (uA > CX25890_ICHRG_I_MAX_uA)
        uA = CX25890_ICHRG_I_MAX_uA;

    reg_val = uA / CX25890_ICHRG_I_STEP_uA;
    reg_val = 0x4f;
    /*
    if(reg_val == 0x20)
            reg_val = 0x4f;
        */

    ret = cx25890_update_bits(cx, CX25890_CHRG_CTRL_4,
                              CX25890_ICHRG_I_MASK, reg_val);

    return ret;
}

static int cx25890_set_chrg_volt(struct charger_device *chg_dev, u32 chrg_volt)
{
    int ret;
    u8 reg_val;
    struct cx25890_device *cx = charger_get_data(chg_dev);

    if (chrg_volt < CX25890_VREG_V_MIN_uV)
        chrg_volt = CX25890_VREG_V_MIN_uV;
    else if (chrg_volt > CX25890_VREG_V_MAX_uV)
        chrg_volt = CX25890_VREG_V_MAX_uV;

    reg_val = (chrg_volt - CX25890_VREG_V_MIN_uV) / CX25890_VREG_V_STEP_uV;
    reg_val = reg_val << 2;
    ret = cx25890_update_bits(cx, CX25890_CHRG_CTRL_6,
                              CX25890_VREG_V_MASK, reg_val);

    return ret;
}

static int cx25890_get_chrg_volt(struct charger_device *chg_dev, unsigned int *volt)
{
    int ret;
    u8 vreg_val;
    struct cx25890_device *cx = charger_get_data(chg_dev);

    ret = cx25890_read_reg(cx, CX25890_CHRG_CTRL_6, &vreg_val);
    if (ret<0)
        return ret;

    vreg_val = (vreg_val & CX25890_VREG_V_MASK) >> 2;

    *volt = vreg_val * CX25890_VREG_V_STEP_uV + CX25890_VREG_V_MIN_uV;

    return 0;
}

static int cx25890_set_input_volt_lim(struct charger_device *chg_dev, unsigned int vindpm)
{
    int ret;
    // unsigned int offset;
    u8 reg_val;
    // u8 os_val;
    struct cx25890_device *cx = charger_get_data(chg_dev);

    if (vindpm < CX25890_VINDPM_V_MIN_uV ||
        vindpm > CX25890_VINDPM_V_MAX_uV)
        return -EINVAL;

    reg_val = (vindpm - 2600000) / CX25890_VINDPM_STEP_uV;

    ret = cx25890_update_bits(cx, CX25890_CHRG_CTRL_d,
                              REG0D_FORCE_VINDPM_MASK, REG0D_FORCE_VINDPM_MASK);
    reg_val = 0x13;
    ret = cx25890_update_bits(cx, CX25890_CHRG_CTRL_d,
                              CX25890_VINDPM_V_MASK, reg_val);

    return ret;
}

static int cx25890_get_input_volt_lim(struct charger_device *chg_dev, u32 *uV)
{
    int ret;
    // int offset;
    u8 vlim;
    // int temp;

    struct cx25890_device *cx = charger_get_data(chg_dev);
    ret = cx25890_read_reg(cx, CX25890_CHRG_CTRL_d, &vlim);
    if (ret)
        return ret;

    vlim = vlim & CX25890_VINDPM_V_MASK;

    *uV = vlim * CX25890_VINDPM_STEP_uV + 2600000;

    return 0;
}

static int cx25890_set_input_curr_lim(struct charger_device *chg_dev, unsigned int iindpm)
{
    int ret;
    u8 reg_val;
    struct cx25890_device *cx = charger_get_data(chg_dev);

    if (iindpm < CX25890_IINDPM_I_MIN_uA ||
        iindpm > CX25890_IINDPM_I_MAX_uA)
        return -EINVAL;

    reg_val = (iindpm - CX25890_IINDPM_I_MIN_uA) / CX25890_IINDPM_STEP_uA;
    printk("cx25890_set_input_curr_lim1 iindpm = %d,reg_val = %x\n",iindpm,reg_val);	
    	if(iindpm>=2000000)
		reg_val = 0x28;
    printk("cx25890_set_input_curr_lim2 iindpm = %d,reg_val = %x\n",iindpm,reg_val);	
    ret = cx25890_update_bits(cx, CX25890_CHRG_CTRL_0,
                              CX25890_IINDPM_I_MASK, reg_val);
    return ret;
}

static int cx25890_get_input_curr_lim(struct charger_device *chg_dev, unsigned int *ilim)
{
    int ret;
    u8 reg_val;
    struct cx25890_device *cx = charger_get_data(chg_dev);

    ret = cx25890_read_reg(cx, CX25890_CHRG_CTRL_0, &reg_val);
    if (ret)
        return ret;

    reg_val = reg_val & CX25890_IINDPM_I_MASK;

    *ilim = CX25890_IINDPM_STEP_uA * reg_val + CX25890_IINDPM_I_MIN_uA;

    return 0;
}

static int cx25890_get_input_mincurr_lim(struct charger_device *chg_dev, u32 *ilim)
{

    *ilim = CX25890_IINDPM_I_MIN_uA;

    return 0;
}

static int cx25890_get_state(struct cx25890_device *cx,
                             struct cx25890_state *state)
{
    u8 reg_val;
    u8 fault;
    u8 chrg_param_0, chrg_param_1, chrg_param_2;
    int ret;

    ret = cx25890_read_reg(cx, CX25890_CHRG_CTRL_b, &reg_val);
    printk("cx25890_read_reg CX25890_CHRG_CTRL_b lzj 1111\n");
    if (ret)
    {
        ret = cx25890_read_reg(cx, CX25890_CHRG_CTRL_b, &reg_val);
        printk("cx25890_read_reg CX25890_CHRG_CTRL_b lzj 2222\n");
        if (ret)
        {
            pr_err("%s read CX25890_CHRG_STAT fail\n", __func__);
            return ret;
        }
    }
    state->chrg_type = (reg_val & CX25890_VBUS_STAT_MASK);
    state->chrg_stat = (reg_val & CX25890_CHG_STAT_MASK);
    state->online = !!(reg_val & CX25890_PG_STAT);
    state->pg_stat = !!(reg_val & CX25890_PG_STAT);
    state->vsys_stat = !!(reg_val & CX25890_VSYS_STAT);

    pr_info("%s chrg_type =0x%x,chrg_stat =0x%x online = 0x%x\n", __func__, state->chrg_type, state->chrg_stat,
            state->online);

    ret = cx25890_read_reg_without_retry(cx, CX25890_CHRG_CTRL_c, &fault);
    if (ret)
    {
        pr_err("%s read CX25890_CHRG_FAULT fail\n", __func__);
        return ret;
    }
    state->chrg_fault = fault;
    state->ntc_fault = fault & CX25890_TEMP_MASK;
    state->health = state->ntc_fault;
    ret = cx25890_read_reg(cx, CX25890_CHRG_CTRL_0, &chrg_param_0);
    if (ret)
    {
        pr_err("%s read CX25890_CHRG_CTRL_0 fail\n", __func__);
        return ret;
    }
    state->hiz_en = !!(chrg_param_0 & CX25890_HIZ_EN);

    ret = cx25890_read_reg(cx, CX25890_CHRG_CTRL_7, &chrg_param_1);
    if (ret)
    {
        pr_err("%s read CX25890_CHRG_CTRL_7 fail\n", __func__);
        return ret;
    }
    state->term_en = !!(chrg_param_1 & CX25890_TERM_EN);

    ret = cx25890_read_reg(cx, CX25890_CHRG_CTRL_11, &chrg_param_2);
    if (ret)
    {
        pr_err("%s read CX25890_CHRG_CTRL_a fail\n", __func__);
        return ret;
    }

    pr_err("%s ,REG_0B=%02x,REG_0C=%02x,REG_11=%02X\n", __func__, reg_val, fault, chrg_param_2);
    state->vbus_gd = !!(chrg_param_2 & CX25890_VBUS_GOOD);
    if (state->vbus_gd)
    {
        dev_err(cx->dev, "vbus_gd = 1");
    }
    else
    {
        dev_err(cx->dev, "vbus_gd = 0");
    }

    return 0;
}

static int cx25890_enable_charger(struct cx25890_device *cx)
{
    int ret;
    pr_err("%s \n", __func__);
    ret = cx25890_update_bits(cx, CX25890_CHRG_CTRL_3, CX25890_CHRG_EN,
                              CX25890_CHRG_EN);

    return ret;
}

static int cx25890_disable_charger(struct cx25890_device *cx)
{
    int ret;
    pr_err("%s \n", __func__);
    ret = cx25890_update_bits(cx, CX25890_CHRG_CTRL_3, CX25890_CHRG_EN,
                              0);
    return ret;
}

static int cx25890_is_charging(struct charger_device *chg_dev, bool *en)
{
    int ret;
    u8 val;
    struct cx25890_device *cx = charger_get_data(chg_dev);

    ret = cx25890_read_reg(cx, CX25890_CHRG_CTRL_3, &val);
    if (ret)
    {
        pr_err("%s read CX25890_CHRG_CTRL_a fail\n", __func__);
        return ret;
    }
    *en = (val & CX25890_CHRG_EN) ? 1 : 0;
    return ret;
}

static int cx25890_charging_switch(struct charger_device *chg_dev, bool enable)
{
    int ret = -1;
    struct cx25890_device *cx = charger_get_data(chg_dev);

    if (enable)
    {
        ret = cx25890_enable_charger(cx);
        cx->chg_config = true;
    }
    else
    {
        ret = cx25890_disable_charger(cx);
        cx->chg_config = false;
    }

    pr_err("%s :%d\n", __func__, enable);
    return ret;
}

static int cx25890_set_recharge_volt(struct cx25890_device *cx, int mV)
{
    u8 reg_val;

    reg_val = (mV - CX25890_VRECHRG_OFFSET_mV) / CX25890_VRECHRG_STEP_mV;

    return cx25890_update_bits(cx, CX25890_CHRG_CTRL_6,
                               CX25890_VRECHARGE, reg_val);
}

/**********************************************************
 *
 *   [Internal Function]
 *
 *********************************************************/
static int cx25890_dump_register(struct charger_device *chg_dev)
{

    unsigned char i = 0;
    unsigned int ret = 0;
    unsigned char cx25890_reg[CX25890_REG_NUM + 1] = {0};
    struct cx25890_device *cx = charger_get_data(chg_dev);

    for (i = 0; i < CX25890_REG_NUM + 1; i++)
    {
        ret = cx25890_read_reg_without_retry(cx, i, &cx25890_reg[i]);
        if (ret != 0)
        {
            pr_err("[cx25890] i2c transfor error\n");
            return 1;
        }
        pr_err("%s,[0x%x]=0x%x ", __func__, i, cx25890_reg[i]);
    }

    return 0;
}

/**********************************************************
 *
 *   [Internal Function]
 *
 *********************************************************/
static int cx25890_hw_chipid_detect(struct cx25890_device *cx)
{
    int ret = 0;
    u8 val = 0;
    ret = cx25890_update_bits(cx, CX25890_CHRG_CTRL_14, 0x80, 0x80);
    ret = cx25890_read_reg(cx, CX25890_CHRG_CTRL_14, &val);
    if (ret < 0)
    {
        pr_info("[%s] read CX25890_CHRG_CTRL_b reg 14 fail\n", __func__);
        return ret;
    }
    val = (val & CX25890_PN_MASK) >> 3;
    pr_info("[%s] Reg[0x14]=0x%x\n", __func__, val);

    return val;
}

static int cx25890_get_charging_status(struct charger_device *chg_dev,
                                       bool *is_done)
{
    struct cx25890_state state;
    struct cx25890_device *cx = charger_get_data(chg_dev);
    cx25890_get_state(cx, &state);

    if (state.chrg_stat == CX25890_TERM_CHRG)
    {
        *is_done = true;
    }
    else
    {
        *is_done = false;
    }

    return 0;
}

static int cx25890_set_en_timer(struct cx25890_device *cx)
{
    int ret;

    ret = cx25890_update_bits(cx, CX25890_CHRG_CTRL_7,
                              CX25890_SAFETY_TIMER_EN, CX25890_SAFETY_TIMER_EN);

    return ret;
}

static int cx25890_set_disable_timer(struct cx25890_device *cx)
{
    int ret;

    ret = cx25890_update_bits(cx, CX25890_CHRG_CTRL_7,
                              CX25890_SAFETY_TIMER_EN, 0);

    return ret;
}

static int cx25890_enable_safetytimer(struct charger_device *chg_dev, bool en)
{
    struct cx25890_device *cx = charger_get_data(chg_dev);
    int ret = 0;

    if (en)
        ret = cx25890_set_en_timer(cx);
    else
        ret = cx25890_set_disable_timer(cx);
    return ret;
}

static int cx25890_get_is_safetytimer_enable(struct charger_device
                                                 *chg_dev,
                                             bool *en)
{
    int ret = 0;
    u8 val = 0;

    struct cx25890_device *cx = charger_get_data(chg_dev);

    ret = cx25890_read_reg(cx, CX25890_CHRG_CTRL_7, &val);
    if (ret < 0)
    {
        pr_info("[%s] read CX25890_CHRG_CTRL_5 fail\n", __func__);
        return ret;
    }
    *en = !!(val & CX25890_SAFETY_TIMER_EN);
    return 0;
}

static int cx25890_get_chrg_stat(struct cx25890_device *cx)
{
    u8 chrg_stat = 0;
    int chrg_value = 0;
    int ret = 0;

    ret = cx25890_read_reg(cx, CX25890_CHRG_CTRL_b, &chrg_stat);
    if (ret)
    {
        ret = cx25890_read_reg(cx, CX25890_CHRG_CTRL_b, &chrg_stat);
        if (ret)
        {
            pr_err("%s read CX25890_CHRG_CTRL_B fail\n", __func__);
            return ret;
        }
    }

    chrg_stat = chrg_stat & CX25890_CHG_STAT_MASK;
    chrg_value = POWER_SUPPLY_STATUS_CHARGING;
    if (!cx->state.chrg_type || (cx->state.chrg_type == CX25890_OTG_MODE))
    {
        chrg_value = POWER_SUPPLY_STATUS_DISCHARGING;
    }
    else if (!chrg_stat)
    {
        if (cx->chg_config)
        {
            chrg_value = POWER_SUPPLY_STATUS_CHARGING;
        }
        else
        {
            chrg_value = POWER_SUPPLY_STATUS_NOT_CHARGING;
        }
    }
    else if (chrg_stat == CX25890_TERM_CHRG)
    {
        chrg_value = POWER_SUPPLY_STATUS_FULL;
    }
    else
    {
        chrg_value = POWER_SUPPLY_STATUS_CHARGING;
    }

    return chrg_value;
}

static enum power_supply_property cx25890_power_supply_props[] =
    {
        POWER_SUPPLY_PROP_MANUFACTURER,
        POWER_SUPPLY_PROP_MODEL_NAME,
        POWER_SUPPLY_PROP_ONLINE,
        POWER_SUPPLY_PROP_STATUS,
        POWER_SUPPLY_PROP_HEALTH,
        POWER_SUPPLY_PROP_VOLTAGE_NOW,
        POWER_SUPPLY_PROP_CURRENT_NOW,
        POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
		//POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT,
        POWER_SUPPLY_PROP_CHARGE_TYPE,
		POWER_SUPPLY_PROP_TYPE,
        POWER_SUPPLY_PROP_USB_TYPE,
        POWER_SUPPLY_PROP_PRESENT,
        POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE,
        };

static int cx25890_property_is_writeable(struct power_supply *psy,
                                         enum power_supply_property prop)
{
    switch (prop)
    {
    case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
	//case POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT:
    case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
    case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
	case POWER_SUPPLY_PROP_STATUS:
	case POWER_SUPPLY_PROP_ONLINE:
    case POWER_SUPPLY_PROP_PRECHARGE_CURRENT:
    case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
        // case POWER_SUPPLY_PROP_CHARGING_ENABLED:
        return true;
    default:
        return false;
    }
}
static int cx25890_charger_set_property(struct power_supply *psy,
                                        enum power_supply_property prop,
                                        const union power_supply_propval *val)
{
    int ret = -EINVAL;
    // struct cx25890_device *cx = power_supply_get_drvdata(psy);

    switch (prop)
    {
    case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
        ret = cx25890_set_input_curr_lim(s_chg_dev_otg, val->intval);
        break;
	/*case POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT:
		ret = cx25890_set_input_volt_lim(s_chg_dev_otg, val->intval);
		break;*/
    default:
        return -EINVAL;
    }

    return ret;
}

static int cx25890_charger_get_property(struct power_supply *psy,
                                        enum power_supply_property psp,
                                        union power_supply_propval *val)
{
    struct cx25890_device *cx = power_supply_get_drvdata(psy);
    struct cx25890_state state;
    int ret = 0;
    u32 data;

    pr_err("%s:psp = %d\n", __func__, psp);
    mutex_lock(&cx->lock);
    memcpy(&state, &cx->state, sizeof(cx->state));
    mutex_unlock(&cx->lock);

    switch (psp)
    {
    case POWER_SUPPLY_PROP_STATUS:
        ret = cx25890_get_chrg_stat(cx);
        if (ret < 0)
        {
            break;
        }
        val->intval = ret;
        break;
    case POWER_SUPPLY_PROP_CHARGE_TYPE:
        pr_err("cx25890---POWER_SUPPLY_PROP_CHARGE_TYPE\n");
        switch (state.chrg_stat)
        {
        case CX25890_PRECHRG:
            val->intval = POWER_SUPPLY_CHARGE_TYPE_TRICKLE;
            pr_err("cx25890 POWER_SUPPLY_CHARGE_TYPE_TRICKLE");
            break;
        case CX25890_FAST_CHRG:
            val->intval = POWER_SUPPLY_CHARGE_TYPE_FAST;
            pr_err("cx25890 POWER_SUPPLY_CHARGE_TYPE_FAST");
            break;
        case CX25890_TERM_CHRG:
            val->intval = POWER_SUPPLY_CHARGE_TYPE_TRICKLE;
            pr_err("cx25890 POWER_SUPPLY_CHARGE_TYPE_TRICKLE\n");
            break;
        case CX25890_NOT_CHRGING:
            val->intval = POWER_SUPPLY_CHARGE_TYPE_NONE;
            pr_err("cx25890 POWER_SUPPLY_CHARGE_TYPE_NONE\n");
            break;
        default:
            val->intval = POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;
        }
        break;
    case POWER_SUPPLY_PROP_MANUFACTURER:
        val->strval = CX25890_MANUFACTURER;
        break;

    case POWER_SUPPLY_PROP_MODEL_NAME:
        val->strval = CX25890_NAME;
        break;
    case POWER_SUPPLY_PROP_USB_TYPE:
        val->intval = cx->psy_usb_type;
        pr_err("POWER_SUPPLY_PROP_USB_TYPE = %d\n", val->intval);
        break;
    case POWER_SUPPLY_PROP_ONLINE:
        val->intval = state.online;
        pr_err("cx25890--state.online=%d\n", val->intval);
        break;
    case POWER_SUPPLY_PROP_PRESENT:
        val->intval = state.vbus_gd;
        break;
    case POWER_SUPPLY_PROP_TYPE:
        val->intval = cx->psy_desc.type;
        pr_err("POWER_SUPPLY_PROP_TYPE = %d\n", val->intval);
        break;

    case POWER_SUPPLY_PROP_HEALTH:
        val->intval = POWER_SUPPLY_HEALTH_GOOD;
        break;

    case POWER_SUPPLY_PROP_VOLTAGE_NOW:
        // val->intval = state.vbus_adc;
		cx25890_get_vbus(cx->chg_dev, &(val->intval));
        break;

    case POWER_SUPPLY_PROP_CURRENT_NOW:
        // val->intval = state.ibus_adc;
		cx25890_get_ibus(cx->chg_dev, &(val->intval));
        break;
    case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
        break;
    case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
        ret = cx25890_get_chrg_volt(s_chg_dev_otg,&data);
        if(ret<0)
        {
            pr_err("Cann't get vreg\n");
            break;
        }
        val->intval = data/1000;
        break;
    default:
        return -EINVAL;
    }

    return ret;
}

static void charger_monitor_work_func(struct work_struct *work)
{
    int ret = 0;
    struct cx25890_device *cx = NULL;
    struct delayed_work *charge_monitor_work = NULL;
    // static u8 last_chg_method = 0;
    struct cx25890_state state;

    charge_monitor_work = container_of(work, struct delayed_work, work);
    if (charge_monitor_work == NULL)
    {
        pr_err("Cann't get charge_monitor_work\n");
        return;
    }
    cx = container_of(charge_monitor_work, struct cx25890_device, charge_monitor_work);
    if (cx == NULL)
    {
        pr_err("Cann't get cx \n");
        return;
    }

    ret = cx25890_get_state(cx, &state);
    if(ret)
            goto OUT;
    mutex_lock(&cx->lock);
    cx->state = state;
    mutex_unlock(&cx->lock);

    if (!cx->state.vbus_gd)
    {
        dev_err(cx->dev, "Vbus not present, disable charge\n");
        cx25890_disable_charger(cx);
        goto OUT;
    }
    if (!state.online)
    {
        dev_err(cx->dev, "Vbus not online\n");
        goto OUT;
    }
    cx25890_dump_register(cx->chg_dev);
OUT:
    schedule_delayed_work(&cx->charge_monitor_work, 10 * HZ);
}

static void charger_detect_work_func(struct work_struct *work)
{
    struct delayed_work *charge_detect_delayed_work = NULL;
    struct cx25890_device *cx = NULL;
    struct cx25890_state state;
    int ret;

    charge_detect_delayed_work = container_of(work, struct delayed_work, work);
    if (charge_detect_delayed_work == NULL)
    {
        pr_err("Cann't get charge_detect_delayed_work\n");
        return;
    }
    cx = container_of(charge_detect_delayed_work, struct cx25890_device, charge_detect_delayed_work);
    if (cx == NULL)
    {
        pr_err("Cann't get cx25890_device\n");
        return;
    }

    ret = cx25890_get_state(cx, &state);
    mutex_lock(&cx->lock);
    cx->state = state;
    mutex_unlock(&cx->lock);

    if (!cx->state.vbus_gd)
    {
        dev_err(cx->dev, "Vbus not present, disable charge ret %d\n",ret);
        // cx25890_disable_charger(cx);
        goto err;
    }
    if (!state.online)
    {
        dev_err(cx->dev, "Vbus not online\n");
        goto err;
    }

    switch (cx->state.chrg_type)
    {
    case CX25890_NOT_CHRGING:
        cx->chg_type = POWER_SUPPLY_TYPE_UNKNOWN;
        cx->psy_usb_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
        cx->psy_desc.type = POWER_SUPPLY_TYPE_UNKNOWN;
        pr_info("CX25890 no input detected\n");
        break;
    case CX25890_USB_SDP:
    	pr_info("[%s] CX25890 charger type: SDP\n", __func__);
		cx->chg_type = POWER_SUPPLY_TYPE_USB;
		cx->psy_usb_type = POWER_SUPPLY_USB_TYPE_SDP;
		cx->psy_desc.type = POWER_SUPPLY_TYPE_USB;		
		cx25890_set_input_curr_lim(cx->chg_dev, 500000);  
		break;

    case CX25890_USB_CDP:
        pr_info("CX25890 charger type: CDP\n");
        cx->chg_type = POWER_SUPPLY_TYPE_USB_CDP;
        cx->psy_usb_type = POWER_SUPPLY_USB_TYPE_CDP;
        cx->psy_desc.type = POWER_SUPPLY_TYPE_USB_CDP;
        cx25890_set_input_curr_lim(cx->chg_dev, 1500000);
        break;

    case CX25890_USB_DCP:
        pr_info("CX25890 charger type: DCP\n");
        cx->chg_type = POWER_SUPPLY_TYPE_USB_DCP;
        cx->psy_usb_type = POWER_SUPPLY_USB_TYPE_DCP;
        cx->psy_desc.type = POWER_SUPPLY_TYPE_USB_DCP;
        cx25890_set_input_curr_lim(cx->chg_dev, 2000000);
        break;

    case CX25890_UNKNOWN:
        pr_info("CX25890 charger type: UNKNOWN\n");
        /*
        cx->chg_type = POWER_SUPPLY_TYPE_UNKNOWN;
        cx->psy_usb_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
        cx->psy_desc.type = POWER_SUPPLY_TYPE_USB;
        cx25890_set_input_curr_lim(cx->chg_dev, 1000000);*/
        cx->chg_type = POWER_SUPPLY_TYPE_USB_DCP;
        cx->psy_usb_type = POWER_SUPPLY_USB_TYPE_DCP;
        cx->psy_desc.type = POWER_SUPPLY_TYPE_USB_DCP;
        cx25890_set_input_curr_lim(cx->chg_dev, 2000000);
        break;
    case CX25890_NON_STANDARD:
        cx->chg_type = POWER_SUPPLY_TYPE_USB_DCP;
        cx->psy_usb_type = POWER_SUPPLY_USB_TYPE_DCP;
        cx->psy_desc.type = POWER_SUPPLY_TYPE_USB_DCP;
        cx25890_set_input_curr_lim(cx->chg_dev, 1000000);		

        pr_info("CX25890 charger type: not standard\n");
        break;
    default:
        cx->chg_type = POWER_SUPPLY_TYPE_USB;
        cx->psy_usb_type = POWER_SUPPLY_USB_TYPE_DCP;
        cx->psy_desc.type = POWER_SUPPLY_TYPE_USB;
        pr_info("CX25890 charger type: default\n");
        break;
    }
    if (cx->state.chrg_type != CX25890_USB_DCP)
    {
        Charger_Detect_Release(); 
		printk("cx25890 chrg_type Charger_Detect_Release");		
    }
		
    cx25890_dump_register(cx->chg_dev);

err:	
    power_supply_changed(cx->charger);

    return;
}

static irqreturn_t cx25890_irq_handler_thread(int irq, void *private)
{
    bool prev_vbus_gd;
    struct cx25890_state state;
    struct cx25890_device *cx = private;
    //int vbus_test;
    dev_err(cx->dev, "cx25890_irq_handler_thread\n");
    cx25890_dump_register(cx->chg_dev);
    cx25890_get_state(cx, &state);
    prev_vbus_gd = cx->state.vbus_gd;	
    cx->state = state;
    if (!prev_vbus_gd && cx->state.vbus_gd)
    {
        if (!cx->charger_wakelock->active)
            __pm_stay_awake(cx->charger_wakelock);
        pr_err("adapter/usb inserted\n");				
        Charger_Detect_Init();
        cx25890_enable_charger(cx);
		cancel_delayed_work_sync(&cx->vindpm_work);
		schedule_delayed_work(&cx->vindpm_work, 0);
    }
    else if (prev_vbus_gd && !cx->state.vbus_gd)
    {
        pr_err("adapter/usb removed\n");	
        cx->chg_type = POWER_SUPPLY_TYPE_UNKNOWN;
        cx->psy_usb_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
        cx->psy_desc.type = POWER_SUPPLY_TYPE_UNKNOWN;
		power_supply_changed(cx->charger);
        cx25890_disable_charger(cx);
        cx25890_disable_vbus(NULL);
        CHARGER=0;
		dev_err(cx->dev, "Relax wakelock\n");
		cancel_delayed_work_sync(&cx->vindpm_work);
		__pm_relax(cx->charger_wakelock);
    }
    if (cx->state.pg_stat)
    {
        schedule_delayed_work(&cx->charge_detect_delayed_work, 0);
    }

    return IRQ_HANDLED;
}

static char *cx25890_charger_supplied_to[] =
    {
        "battery",
        "mtk-master-charger",//battery
};

static struct power_supply_desc cx25890_power_supply_desc =
    {
        .name = "primary_chg",//primary_chg cx25890-charger
        .type = POWER_SUPPLY_TYPE_USB,//POWER_SUPPLY_TYPE_BATTERY,
        .usb_types = cx25890_usb_type,
        .num_usb_types = ARRAY_SIZE(cx25890_usb_type),
        .properties = cx25890_power_supply_props,
        .num_properties = ARRAY_SIZE(cx25890_power_supply_props),
        .get_property = cx25890_charger_get_property,
        .set_property = cx25890_charger_set_property,
        .property_is_writeable = cx25890_property_is_writeable,
};

static int cx25890_power_supply_init(struct cx25890_device *cx,
                                     struct device *dev)
{
    struct power_supply_config psy_cfg = {
        .drv_data = cx,
        .of_node = dev->of_node,
    };

    psy_cfg.supplied_to = cx25890_charger_supplied_to;
    psy_cfg.num_supplicants = ARRAY_SIZE(cx25890_charger_supplied_to);
    memcpy(&cx->psy_desc, &cx25890_power_supply_desc, sizeof(cx25890_power_supply_desc));
    cx->charger = devm_power_supply_register(cx->dev,
                                             &cx->psy_desc,
                                             &psy_cfg);
    if (IS_ERR(cx->charger))
    {
        pr_err("cx->charger  register fail\n");
        return -EINVAL;
    }

    return 0;
}

static int cx25890_hw_init(struct cx25890_device *cx)
{
    int ret = 0;
    struct power_supply_battery_info bat_info = {};

    bat_info.constant_charge_current_max_ua =
        CX25890_ICHRG_I_DEF_uA;

    bat_info.constant_charge_voltage_max_uv = 4350000;

    bat_info.precharge_current_ua =
        CX25890_PRECHRG_I_DEF_uA;

    bat_info.charge_term_current_ua =
        CX25890_TERMCHRG_I_DEF_uA;

    cx->init_data.max_ichg =
        CX25890_ICHRG_I_MAX_uA;

    cx->init_data.max_vreg =
        CX25890_VREG_V_MAX_uV;

    cx25890_set_watchdog_timer(cx, 0);
    ret = cx25890_write_reg40(cx, 1);
    if (ret == 1)
        pr_err("write reg40 success\n");
    cx25890_write_reg(cx, 0x41, 0x08);
    cx25890_write_reg(cx, 0x44, 0x18);
    cx25890_write_reg40(cx, 0);

    ret = cx25890_set_ichrg_curr(s_chg_dev_otg,
                                 bat_info.constant_charge_current_max_ua);
    if (ret)
        goto err_out;

    ret = cx25890_set_prechrg_curr(cx, bat_info.precharge_current_ua);
    if (ret)
        goto err_out;

    ret = cx25890_set_chrg_volt(s_chg_dev_otg,
                                bat_info.constant_charge_voltage_max_uv);
    if (ret)
        goto err_out;

    ret = cx25890_set_term_curr(s_chg_dev_otg, bat_info.charge_term_current_ua);
    if (ret)
        goto err_out;

    /*ret = cx25890_set_input_volt_lim(s_chg_dev_otg, cx->init_data.vlim);
    if (ret)
        goto err_out;*/

    ret = cx25890_set_input_curr_lim(s_chg_dev_otg, cx->init_data.ilim);
    if (ret)
        goto err_out;
#if 0
    ret = cx25890_set_vac_ovp(cx);//14V
    if (ret)
        goto err_out;
#endif

    ret = cx25890_set_input_volt_lim(s_chg_dev_otg, 4600000); // set vindpm 4.6v
    ret = cx25890_update_bits(cx, 0x86, 0x20, 0x20);          // disable ntc
    ret = cx25890_update_bits(cx, CX25890_CHRG_CTRL_8, 0x03, 0x0); // Thermal Regulation
    ret = cx25890_update_bits(cx, CX25890_CHRG_CTRL_0, 0x40, 0x0); // disable ILIM Pin
    ret = cx25890_update_bits(cx, CX25890_CHRG_CTRL_2, 0x10, 0x0); // disable  ICO_EN
    ret = cx25890_update_bits(cx, CX25890_CHRG_CTRL_7, 0x80, 0x80); // disable  EN_TERM
    ret = cx25890_update_bits(cx, CX25890_CHRG_CTRL_2, 0x08, 0x0); // Disable HVDCP handshake
    ret = cx25890_update_bits(cx, CX25890_CHRG_CTRL_0, 0x80, 0x0); // disable EN_HIZ

    ret = cx25890_set_recharge_volt(cx, 200); // 100~200mv
    if (ret)
        goto err_out;

    dev_notice(cx->dev, "ichrg_curr:%d prechrg_curr:%d chrg_vol:%d"
                        " term_curr:%d input_curr_lim:%d",
               bat_info.constant_charge_current_max_ua,
               bat_info.precharge_current_ua,
               bat_info.constant_charge_voltage_max_uv,
               bat_info.charge_term_current_ua,
               cx->init_data.ilim);

    return 0;

err_out:
    return ret;
}

static int cx25890_parse_dt(struct cx25890_device *cx)
{
    int ret;
    int irq_gpio = 0, irqn = 0;
    int chg_en_gpio = 0;
    //int otg_gpio = 0;

    ret = device_property_read_u32(cx->dev,
                                   "input-voltage-limit-microvolt",
                                   &cx->init_data.vlim);
    if (ret)
        cx->init_data.vlim = CX25890_VINDPM_DEF_uV;

    if (cx->init_data.vlim > CX25890_VINDPM_V_MAX_uV ||
        cx->init_data.vlim < CX25890_VINDPM_V_MIN_uV)
        return -EINVAL;

    ret = device_property_read_u32(cx->dev,
                                   "input-current-limit-microamp",
                                   &cx->init_data.ilim);
    if (ret)
        cx->init_data.ilim = CX25890_IINDPM_DEF_uA;

    if (cx->init_data.ilim > CX25890_IINDPM_I_MAX_uA ||
        cx->init_data.ilim < CX25890_IINDPM_I_MIN_uA)
        return -EINVAL;

    irq_gpio = of_get_named_gpio(cx->dev->of_node, "cx,irq-gpio", 0);
    printk("cx25890 find irq_gpio done ! \n");
    if (!gpio_is_valid(irq_gpio))
    {
        dev_err(cx->dev, "%s: %d gpio get failed\n", __func__, irq_gpio);
        return -EINVAL;
    }
    ret = gpio_request(irq_gpio, "cx25890 irq pin");
    if (ret)
    {
        dev_err(cx->dev, "%s: %d gpio request failed\n", __func__, irq_gpio);
        return ret;
    }
    gpio_direction_input(irq_gpio);
    irqn = gpio_to_irq(irq_gpio);
    if (irqn < 0)
    {
        dev_err(cx->dev, "%s:%d gpio_to_irq failed\n", __func__, irqn);
        return irqn;
    }
    cx->client->irq = irqn;

    chg_en_gpio = of_get_named_gpio(cx->dev->of_node, "cx,chg-en-gpio", 0);
    if (!gpio_is_valid(chg_en_gpio))
    {
        dev_err(cx->dev, "%s: %d gpio get failed\n", __func__, chg_en_gpio);
        return -EINVAL;
    }
    ret = gpio_request(chg_en_gpio, "cx chg en pin");
    if (ret)
    {
        dev_err(cx->dev, "%s: %d gpio request failed\n", __func__, chg_en_gpio);
        return ret;
    }
    gpio_direction_output(chg_en_gpio, 0); // default enable charge

    // otg_gpio = of_get_named_gpio(cx->dev->of_node, "cx,otg-gpio", 0);
    // if (!gpio_is_valid(otg_gpio))
    // {
    //     dev_err(cx->dev, "%s: %d gpio get failed\n", __func__, otg_gpio);
    //     return -EINVAL;
    // }
    // ret = gpio_request(otg_gpio, "cx chg en pin");
    // if (ret)
    // {
    //     dev_err(cx->dev, "%s: %d gpio request failed\n", __func__, otg_gpio);
    //     return ret;
    // }
    // gpio_direction_output(otg_gpio, 1); // default enable otg
    return 0;
}

static int cx25890_enable_vbus(struct regulator_dev *rdev)
{
    int ret = 0;
    u8 temp = 0;
    struct cx25890_device *cx = charger_get_data(s_chg_dev_otg);

    ret = cx25890_read_reg_without_retry(cx, CX25890_CHRG_CTRL_c, &temp);
    pr_info("%s   temp = 0x%02X \n", __func__, temp);

    ret = cx25890_update_bits(cx, CX25890_CHRG_CTRL_a, 0x7, 0x6); // set Boost Current Limit  2.15A
    cx25890_write_reg40(cx, true);
    cx25890_write_reg(cx, 0x83, 0x03);
    cx25890_write_reg(cx, 0x41, 0x88);
    ret = cx25890_update_bits(cx, CX25890_CHRG_CTRL_3, CX25890_OTG_MASK,
                              CX25890_OTG_ENABLE << CX25890_OTG_SHIFT);
    msleep(100);
    ret = cx25890_update_bits(cx, CX25890_CHRG_CTRL_a, 0x7, 0x4); // set Boost Current Limit  2.15A
    cx25890_write_reg(cx, 0x83, 0x01);
    msleep(10);
    cx25890_write_reg(cx, 0x41, 0x08);
    cx25890_write_reg40(cx, false);
    pr_info("%s ret = %d\n", __func__, ret);
    return ret;
}

static int cx25890_disable_vbus(struct regulator_dev *rdev)
{
    int ret = 0;
    struct cx25890_device *cx = charger_get_data(s_chg_dev_otg);

    cx25890_write_reg40(cx, true);
    cx25890_write_reg(cx, 0x83, 0x00);
    cx25890_write_reg40(cx, false);
    ret = cx25890_update_bits(cx, CX25890_CHRG_CTRL_3, CX25890_OTG_MASK,
                              CX25890_OTG_DISABLE << CX25890_OTG_SHIFT);
    pr_info("%s ret = %d\n", __func__, ret);
    return ret;
}

static int cx25890_is_enabled_vbus(struct regulator_dev *rdev)
{
    u8 temp = 0;
    int ret = 0;
    struct cx25890_device *cx = charger_get_data(s_chg_dev_otg);

    ret = cx25890_read_reg(cx, CX25890_CHRG_CTRL_3, &temp);
    pr_info("%s ret = %d\n", __func__, ret);
    return (temp & CX25890_OTG_MASK) ? 1 : 0;
}

static int cx25890_enable_otg(struct charger_device *chg_dev, bool en)
{
    int ret = 0;
    pr_info("%s en = %d\n", __func__, en);
    if (en)
    {
        ret = cx25890_enable_vbus(NULL);
    }
    else
    {
        ret = cx25890_disable_vbus(NULL);
    }

    return ret;
}

static int cx25890_set_boost_current_limit(struct charger_device *chg_dev, u32 uA)
{
    int ret = 0;
    u8 reg_val;
    struct cx25890_device *cx = charger_get_data(chg_dev);

    for (reg_val = 0; reg_val < 8 && uA >= BOOST_CURRENT_LIMIT[reg_val]; reg_val++)
        ;

    reg_val = reg_val << 4;

    return cx25890_update_bits(cx, CX25890_CHRG_CTRL_a,
                               CX25890_PRECHRG_CUR_MASK, reg_val);

    if (uA == BOOST_CURRENT_LIMIT[0])
    {
        ret = cx25890_update_bits(cx, CX25890_CHRG_CTRL_2, CX25890_BOOST_LIM,
                                  0);
    }

    else if (uA == BOOST_CURRENT_LIMIT[1])
    {
        ret = cx25890_update_bits(cx, CX25890_CHRG_CTRL_2, CX25890_BOOST_LIM,
                                  BIT(7));
    }
    return ret;
}

static int cx25890_set_boost_current_limit_st(struct cx25890_device *cx, u32 uA)
{
    int ret = 0;
    u8 reg_val;
   // struct cx25890_device *cx = charger_get_data(chg_dev);

    for (reg_val = 0; reg_val < 8 && uA >= BOOST_CURRENT_LIMIT[reg_val]; reg_val++)
        ;

    reg_val = reg_val << 4;

    return cx25890_update_bits(cx, CX25890_CHRG_CTRL_a,
                               CX25890_PRECHRG_CUR_MASK, reg_val);

    if (uA == BOOST_CURRENT_LIMIT[0])
    {
        ret = cx25890_update_bits(cx, CX25890_CHRG_CTRL_2, CX25890_BOOST_LIM,
                                  0);
    }

    else if (uA == BOOST_CURRENT_LIMIT[1])
    {
        ret = cx25890_update_bits(cx, CX25890_CHRG_CTRL_2, CX25890_BOOST_LIM,
                                  BIT(7));
    }
    return ret;
}

static int cx25890_set_otg_current_limit(struct regulator_dev *rdev,int min_uA, int max_uA)
{
    int ret =0;
    int set_cur = 500000;
    struct cx25890_device *cx = rdev->reg_data; 
    
    if (min_uA!=max_uA)
        printk("cx25890 set otg current not right(%d)!=(%d)\n",min_uA,max_uA);
    
    //set_cur = min_uA/1000;
    ret = cx25890_set_boost_current_limit_st(cx,set_cur);
    printk("cx25890_set_otg_current_limit:%d  ret:%d\n",set_cur,ret);
    return ret;
}
static const struct regulator_ops cx25890_vbus_ops =
    {
        .enable = cx25890_enable_vbus,
        .disable = cx25890_disable_vbus,
        .is_enabled = cx25890_is_enabled_vbus,
        .set_current_limit = cx25890_set_otg_current_limit,
};

static const struct regulator_desc cx25890_otg_rdesc =
    {
        .of_match = "usb-otg-vbus",
        .name = "usb-otg-vbus",
        .ops = &cx25890_vbus_ops,
        .owner = THIS_MODULE,
        .type = REGULATOR_VOLTAGE,
        .fixed_uV = 5150000,
        .n_voltages = 1,
};

static const struct regulator_init_data cx25890_vbus_init_data = {
        .constraints = {
        .valid_ops_mask = REGULATOR_CHANGE_STATUS|REGULATOR_CHANGE_CURRENT,//|REGULATOR_CHANGE_VOLTAGE,
        .min_uA = 500000,
        .max_uA = 1200000,
        //.min_uV = 4500000,
        //.max_uV = 5000000,
        },
};

static int cx25890_vbus_regulator_register(struct cx25890_device *cx)
{
    struct regulator_config config = {};
    int ret = 0;
    /* otg regulator */
    config.dev = cx->dev;
    config.driver_data = cx;
    config.init_data = &cx25890_vbus_init_data;
    cx->otg_rdev = devm_regulator_register(cx->dev,
                                           &cx25890_otg_rdesc, &config);
   // cx->otg_rdev->constraints->valid_ops_mask |= REGULATOR_CHANGE_STATUS;
    if (IS_ERR(cx->otg_rdev))
    {
        ret = PTR_ERR(cx->otg_rdev);
        pr_info("%s: register otg regulator failed (%d)\n", __func__, ret);
    }
    return ret;
}
static int cx25890_get_vbus(struct charger_device *chg_dev, u32 *vbus)
{
        struct cx25890_device *cx = charger_get_data(chg_dev);
	u8 value, status;
	int ret;
	int i;
        // printk("func:%s line:%d\n",__func__,__LINE__);
        // ret = cx25890_read_reg(cx, CX25890_CHRG_CTRL_11, &value);
        // value = (value & 0x80) >> 7;
        // if (value == 0) {
                // *vbus = 0;
                // return 0;
        // }
		
		cx25890_update_bits(cx, CX25890_CHRG_CTRL_2, 0x80, 1 << 7); 

		 for (i = 0; i < 10; i++)
		 {		  
			  ret = cx25890_read_reg(cx, CX25890_CHRG_CTRL_2, &value);
		 
			  status = (value & 0x80) >> 7;
			  if (!status)
				  break;
				  
			  msleep(20);
		 }
		
		if (!status) {
			ret = cx25890_read_reg(cx, CX25890_CHRG_CTRL_11, &value);			
			value = value & 0x7f;			
			*vbus = (value + 26) * 100 * 1000;
			printk("vbus :%d line:%d i %d ret %d\n", *vbus, __LINE__, i,ret);
		}

		
		return 0;
}
static int cx25890_get_ibus(struct charger_device *chg_dev, u32 *ibus)
{
		struct cx25890_device *cx = charger_get_data(chg_dev);
		u8 value, status;
		int ret;
		int i;

		
		
		printk("func:%s line:%d\n",__func__,__LINE__);
        ret = cx25890_read_reg(cx, CX25890_CHRG_CTRL_11, &value);
        value = (value & 0x80) >> 7;
		if (value == 0) {
            *ibus = 0;            
		}else {
			*ibus = 800*1000;
		}
		
		return 0;
		
		cx25890_update_bits(cx, CX25890_CHRG_CTRL_2, 0x40, 1 << 6); 

		 for (i = 0; i < 10; i++)
		 {		  
			  ret = cx25890_read_reg(cx, CX25890_CHRG_CTRL_2, &value);
		 
			  status = (value & 0x80) >> 7;
			  if (!status)
				  break;
				  
			  msleep(200);
		 }
		
		if (!status) {
			ret = cx25890_read_reg(cx, CX25890_CHRG_CTRL_13, &value);			
			value = value & 0x3F;			
			*ibus = 100 * 1000 + value * 50 * 1000;
			printk("ibus :%d line:%d i %d ret %d\n", *ibus, __LINE__, i,ret);
		}
		
        return 0;
}
static int cx25890_get_ibat(struct charger_device *chg_dev, u32 *ibat)
{
	struct cx25890_device *cx = charger_get_data(chg_dev);
	u8 value, status;
	int ret;
	int i;
	
	printk("func:%s line:%d\n",__func__,__LINE__);

        ret = cx25890_read_reg(cx, CX25890_CHRG_CTRL_11, &value);
        value = (value & 0x80) >> 7;
		if (value == 0) {
            *ibat = 0;
            return 0;
		}
	
	cx25890_update_bits(cx, CX25890_CHRG_CTRL_2, 0x40, 1 << 6); 
	
	 for (i = 0; i < 10; i++)
	 {		  
		  ret = cx25890_read_reg(cx, CX25890_CHRG_CTRL_2, &value);
	 
		  status = (value & 0x80) >> 7;
		  if (!status)
			  break;
			  
		  msleep(200);
	 }
	
	if (!status) {
		ret = cx25890_read_reg(cx, CX25890_CHRG_CTRL_12, &value);			
		value = value & 0x7F;			
		*ibat = value * 50 * 1000;
		printk("ibat :%d line:%d i %d ret %d\n", *ibat, __LINE__, i,ret);
	}
	
	return 0;
	
}


#if 1
static int cx25890_reset_ta(struct charger_device *chg_dev)
{
 	int32_t ret = 0;
 	struct cx25890_device *cx = charger_get_data(chg_dev);

 	pr_info("cx25890_reset_ta enter\n");

	ret = cx25890_set_input_volt_lim(cx->chg_dev, 4500000);
 	if (ret < 0)
 		return ret;

 	ret = cx25890_set_input_curr_lim(cx->chg_dev, 100000);	/* 100mA */
 	if (ret < 0)
 		return ret;

 	msleep(250);

 	ret = cx25890_set_input_curr_lim(cx->chg_dev, 700000);	/* 700mA */
 	if (ret < 0)
 		return ret;

	return (ret < 0) ? ret : 0;
}
#endif



static int32_t cx25890_charging_set_ta_current_pattern(struct charger_device *chg_dev, bool is_increase)
{
	bool increase = is_increase;
	struct cx25890_device *cx = charger_get_data(chg_dev);

	pr_info("is_increase %d\n",is_increase);
	//sd76030_set_vreg_volt(chip,4400);	/* Set CV */	
	cx25890_set_ichrg_curr(cx->chg_dev,800000);/* Set charging current 800ma */
	/* Enable Charging */
	cx25890_update_bits(cx, CX25890_CHRG_CTRL_0, 0x80, 0x0); // disable EN_HIZ
	cx25890_enable_charger(cx);

	if (increase == 1) 
	{
		cx25890_set_input_curr_lim(cx->chg_dev, 100000);	/* 100mA */
		msleep(85);

		cx25890_set_input_curr_lim(cx->chg_dev, 500000);	/* 500mA */
		msleep(85);

		cx25890_set_input_curr_lim(cx->chg_dev, 100000);	/* 100mA */
		msleep(85);

		cx25890_set_input_curr_lim(cx->chg_dev, 500000);	/* 500mA */
		msleep(85);

		cx25890_set_input_curr_lim(cx->chg_dev, 100000);	/* 100mA */
		msleep(85);

		cx25890_set_input_curr_lim(cx->chg_dev, 500000);	/* 500mA */
		msleep(281);

		cx25890_set_input_curr_lim(cx->chg_dev, 100000);	/* 100mA */
		msleep(85);

		cx25890_set_input_curr_lim(cx->chg_dev, 500000);/* 500mA */
		msleep(281);

		cx25890_set_input_curr_lim(cx->chg_dev, 100000);	/* 100mA */
		msleep(85);

		cx25890_set_input_curr_lim(cx->chg_dev, 500000);	/* 500mA */
		msleep(281);

		cx25890_set_input_curr_lim(cx->chg_dev, 100000);	/* 100mA */
		msleep(85);

		cx25890_set_input_curr_lim(cx->chg_dev, 500000);	/* 500mA */
		msleep(485);

		cx25890_set_input_curr_lim(cx->chg_dev, 100000);	/* 100mA */
		msleep(50);

		cx25890_set_input_curr_lim(cx->chg_dev, 500000);	/* 500mA */
		msleep(200);
	} else {
		cx25890_set_input_curr_lim(cx->chg_dev, 100000);	/* 100mA */
		msleep(85);

		cx25890_set_input_curr_lim(cx->chg_dev, 500000);	/* 500mA */
		msleep(281);

		cx25890_set_input_curr_lim(cx->chg_dev, 100000);	/* 100mA */
		msleep(85);

		cx25890_set_input_curr_lim(cx->chg_dev, 500000);	/* 500mA */
		msleep(281);

		cx25890_set_input_curr_lim(cx->chg_dev, 100000);	/* 100mA */
		msleep(85);

		cx25890_set_input_curr_lim(cx->chg_dev, 500000);	/* 500mA */
		msleep(281);

		cx25890_set_input_curr_lim(cx->chg_dev, 100000);	/* 100mA */
		msleep(85);

		cx25890_set_input_curr_lim(cx->chg_dev, 500000);	/* 500mA */
		msleep(85);

		cx25890_set_input_curr_lim(cx->chg_dev, 100000);	/* 100mA */
		msleep(85);

		cx25890_set_input_curr_lim(cx->chg_dev, 500000);	/* 500mA */
		msleep(85);

		cx25890_set_input_curr_lim(cx->chg_dev, 100000);	/* 100mA */
		msleep(85);

		cx25890_set_input_curr_lim(cx->chg_dev, 500000);	/* 500mA */
		msleep(485);

		cx25890_set_input_curr_lim(cx->chg_dev, 100000);	/* 100mA */
		msleep(50);

		cx25890_set_input_curr_lim(cx->chg_dev, 500000);	/* 500mA */
	}

	return 0;
}




/*
static int cx25890_plug_in(struct charger_device *chg_dev)
{       
        printk("cx25890_plug_in\n");
        return 1;
        
}
static int cx25890_plug_out(struct charger_device *chg_dev)
{
        printk("cx25890_plug_out\n");
        return 0;
}
*/
static struct charger_ops cx25890_chg_ops =
    {

        .dump_registers = cx25890_dump_register,
        /* cable plug in/out */
        //.plug_in = cx25890_plug_in,
        //.plug_out = cx25890_plug_out,
        /* enable */
        .enable = cx25890_charging_switch,
        .is_enabled = cx25890_is_charging,
        /* charging current */
        .set_charging_current = cx25890_set_input_curr_lim,
        .get_charging_current = cx25890_get_ichg_curr,
        .get_min_charging_current = cx25890_get_minichg_curr,
        /* charging voltage */
        .set_constant_voltage = cx25890_set_chrg_volt,
        .get_constant_voltage = cx25890_get_chrg_volt,
        /* input current limit */
        .set_input_current = cx25890_set_input_curr_lim,
        .get_input_current = cx25890_get_input_curr_lim,
        .get_min_input_current = cx25890_get_input_mincurr_lim,
        /* MIVR */
        .set_mivr = cx25890_set_input_volt_lim,
        .get_mivr = cx25890_get_input_volt_lim,
        //   .get_mivr_state = cx25890_get_input_minvolt_lim,
        /* ADC */
        .get_adc = NULL,
        .get_vbus_adc = cx25890_get_vbus,
        .get_ibus_adc = cx25890_get_ibus,
        .get_ibat_adc = cx25890_get_ibat,
        .get_tchg_adc = NULL,
        .get_zcv = NULL,
        /* charing termination */
        .set_eoc_current = cx25890_set_term_curr,
        //.enable_termination = mt6375_enable_te,
        //.reset_eoc_state = mt6375_reset_eoc_state,
        //.safety_check = mt6375_sw_check_eoc,
        .is_charging_done = cx25890_get_charging_status,
        /* power path */
        //.enable_powerpath = mt6375_enable_buck,
        //.is_powerpath_enabled = mt6375_is_buck_enabled,
        /* timer */
        .enable_safety_timer = cx25890_enable_safetytimer,
        .is_safety_timer_enabled = cx25890_get_is_safetytimer_enable,
		/* PE/PE+ */
		.reset_ta 			= cx25890_reset_ta,//cx25890_reset_ta,
		.send_ta_current_pattern	= cx25890_charging_set_ta_current_pattern,
// .kick_wdt = cx25890_reset_watch_dog_timer,
/* AICL */
//.run_aicl = mt6375_run_aicc,
/* PE+/PE+20 */
#if 0
#if (defined(__SGM41542_CHIP_ID__) || defined(__SGM41516D_CHIP_ID__) || defined(__SGM41543D_CHIP_ID__))
    .send_ta_current_pattern = cx25890_en_pe_current_partern,
#else
    .send_ta_current_pattern = NULL,
#endif
#endif
        //.set_pe20_efficiency_table = mt6375_set_pe20_efficiency_table,
        //.send_ta20_current_pattern = mt6375_set_pe20_current_pattern,
        //.reset_ta = mt6375_reset_pe_ta,
        //.enable_cable_drop_comp = mt6
        /* OTG */
        .enable_otg = cx25890_enable_otg,
        .set_boost_current_limit = cx25890_set_boost_current_limit,
};

static void vindpm_work_func(struct work_struct *work)
{
 //   int ret = 0;
    struct cx25890_device *cx = NULL;
    struct delayed_work *vindpm_work = NULL;
    // static u8 last_chg_method = 0;
  //  struct cx25890_state state;
	u32 vbus_value = 0;
	

    vindpm_work = container_of(work, struct delayed_work, work);
    if (vindpm_work == NULL)
    {
        pr_err("Cann't get charge_monitor_work\n");
        return;
    }
    cx = container_of(vindpm_work, struct cx25890_device, vindpm_work);
    if (cx == NULL)
    {
        pr_err("Cann't get cx \n");
        return;
    }

    cx25890_get_vbus(cx->chg_dev, &vbus_value);
    vbus_value = vbus_value/1000;
	pr_err("vindpm_work_func vbus_value %d \n", vbus_value);
	if (vbus_value > 8000)
		cx25890_set_input_volt_lim(cx->chg_dev, 8300000);
	else 
        cx25890_set_input_volt_lim(cx->chg_dev, 4500000);
	
    schedule_delayed_work(&cx->vindpm_work, 5 * HZ);
}

static int cx25890_driver_probe(struct i2c_client *client)
{
    int ret = 0;
    struct device *dev = &client->dev;
    struct cx25890_device *cx;
	u8 value;

    char *name = NULL;
    pr_err("%s:kernel enter client->addr %x\n", __func__,client->addr);
    cx = devm_kzalloc(dev, sizeof(*cx), GFP_KERNEL);
    if (!cx)
        return -ENOMEM;

    //client->addr = 0x6a;    

    cx->client = client;
    cx->dev = dev;

    mutex_init(&cx->lock);
    mutex_init(&cx->i2c_rw_lock);

    i2c_set_clientdata(client, cx);

    ret = cx25890_parse_dt(cx);
    if (ret)
        return ret;

    ret = cx25890_hw_chipid_detect(cx);
    if (ret != CX25890_PN_ID)
    {
        pr_info("[%s] device not found !!!\n", __func__);
        return ret;
    }
    pr_err("%s:found cx25890H client->addr %x \n", __func__,client->addr);
    name = devm_kasprintf(cx->dev, GFP_KERNEL, "%s", "cx25890 suspend wakelock");
    cx->charger_wakelock = wakeup_source_register(NULL, name);

    /* Register charger device */
    cx->chg_dev = charger_device_register("primary_chg",
                                          &client->dev, cx,
                                          &cx25890_chg_ops,
                                          &cx25890_chg_props);
    if (IS_ERR_OR_NULL(cx->chg_dev))
    {
        pr_info("%s: register charger device  failed\n", __func__);
        ret = PTR_ERR(cx->chg_dev);
        return ret;
    }

    /* otg regulator */
    s_chg_dev_otg = cx->chg_dev;

    INIT_DELAYED_WORK(&cx->charge_detect_delayed_work, charger_detect_work_func);
    INIT_DELAYED_WORK(&cx->charge_monitor_work, charger_monitor_work_func);
	INIT_DELAYED_WORK(&cx->vindpm_work, vindpm_work_func);
	
    if (client->irq)
    {
        ret = devm_request_threaded_irq(dev, client->irq, NULL,
                                        cx25890_irq_handler_thread,
                                        IRQF_TRIGGER_FALLING |
                                            IRQF_ONESHOT,
                                        dev_name(&client->dev), cx);
        if (ret)
            return ret;
        enable_irq_wake(client->irq);
        device_init_wakeup(cx->dev, true);
    }

    ret = cx25890_power_supply_init(cx, dev);
    if (ret)
    {
        pr_err("Failed to register power supply\n");
        return ret;
    }
    pr_err("%s:power supply register success\n", __func__);
    ret = cx25890_hw_init(cx);
    if (ret)
    {
        dev_err(dev, "Cannot initialize the chip.\n");
        return ret;
    }
    pr_err("%s:cx25890 init success client->addr %x  \n", __func__,client->addr);
    cx->chg_config = false;
    ret = cx25890_vbus_regulator_register(cx);	

    schedule_delayed_work(&cx->charge_monitor_work, 100);
    schedule_delayed_work(&cx->charge_detect_delayed_work, 3000);

    ret = cx25890_read_reg(cx, CX25890_CHRG_CTRL_11, &value);
    if (ret)
    {
        pr_err("%s read CX25890_CHRG_CTRL_a fail\n", __func__);
        return ret;
    }

    value = (value & CX25890_VBUS_GOOD) >> 7;
    if (value)
    {
        dev_err(cx->dev, "vbus_gd = 1");
		cancel_delayed_work_sync(&cx->vindpm_work);
		schedule_delayed_work(&cx->vindpm_work, 0);
    }
    else
    {
        dev_err(cx->dev, "vbus_gd = 0");
    }	

    pr_err("cx25890---probe done! client->addr %x \n",client->addr);

    return ret;
}

static void cx25890_charger_remove(struct i2c_client *client)
{
    struct cx25890_device *cx = i2c_get_clientdata(client);
	
    cx25890_set_input_volt_lim(cx->chg_dev, 4500000);
    cancel_delayed_work_sync(&cx->charge_monitor_work);

    regulator_unregister(cx->otg_rdev);

    power_supply_unregister(cx->charger);

    mutex_destroy(&cx->lock);
    mutex_destroy(&cx->i2c_rw_lock);

    return;
}

static void cx25890_charger_shutdown(struct i2c_client *client)
{
    int ret = 0;

    struct cx25890_device *cx = i2c_get_clientdata(client);
	cx25890_set_input_volt_lim(cx->chg_dev, 4500000);
    ret = cx25890_disable_charger(cx);
    if (ret)
    {
        pr_err("Failed to disable charger, ret = %d\n", ret);
    }
    pr_info("cx25890_charger_shutdown\n");
}

static const struct i2c_device_id cx25890_i2c_ids[] =
    {
        {"sd76030", 0},
        {},
};
MODULE_DEVICE_TABLE(i2c, cx25890_i2c_ids);

static const struct of_device_id cx25890_of_match[] =
    {
        {
            .compatible = "bigmtech,sd76030",
        },
        {},
};
MODULE_DEVICE_TABLE(of, cx25890_of_match);

static struct i2c_driver cx25890_driver =
    {
        .driver = {
            .name = "sd76030",
            .of_match_table = cx25890_of_match,
        },
        .probe = cx25890_driver_probe,
        .remove = cx25890_charger_remove,
        .shutdown = cx25890_charger_shutdown,
        .id_table = cx25890_i2c_ids,
};
// module_i2c_driver(cx25890_driver);


static int32_t __init cx25890_init(void)
{
    int32_t ret = 0;
    ret = i2c_add_driver(&cx25890_driver);

    if (0 != ret) 
        printk("cx25890_init failed to register cx25890 i2c driver.\n");
    else 
        printk("cx25890_init Success to register cx25890 i2c driver.\n");
    
    return ret;
}

static void __exit cx25890_exit(void)
{
    i2c_del_driver(&cx25890_driver);
}
module_init(cx25890_init);
module_exit(cx25890_exit);


MODULE_AUTHOR(" zyf <yuanfa.zhuang@cx-semi.com.cn>");
MODULE_DESCRIPTION("cx25890 charger driver");
MODULE_LICENSE("GPL");
