/* SPDX-License-Identifier: GPL-2.0 */
/*
* Copyright (c) 2022 Southchip Semiconductor Technology(Shanghai) Co., Ltd.
*/

#define pr_fmt(fmt)	"[sc8989x]:%s: " fmt, __func__

#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/err.h>
#include <linux/bitops.h>
#include <linux/math64.h>

#include <linux/phy/phy.h>
#include "mtk_charger.h"

#include "sc8989x_reg.h"
#include "charger_class.h"

#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/machine.h>
#include <linux/iio/consumer.h>

enum sc8989x_usbsw {
    USBSW_CHG = 0,
    USBSW_USB,
};
#define PHY_MODE_BC11_SET 1
#define PHY_MODE_BC11_CLR 2

enum {
    PN_SC8989X, 
};

static int pn_data[] = {
    [PN_SC8989X] = 0x05,
};

struct chg_para{
    int vlim;
    int ilim;

    int vreg;
    int ichg;
};

struct sc8989x_platform_data {
    int iprechg;
    int iterm;

    int boostv;
    int boosti;

    struct chg_para usb;
};

struct sc8989x {
    struct device *dev;
    struct i2c_client *client;

    int part_no;
    int revision;

    const char *chg_dev_name;
    const char *eint_name;

    int irq_gpio;

    enum power_supply_type chg_type;
    enum power_supply_usb_type psy_usb_type;

    int status;
    int irq;

    struct mutex i2c_rw_lock;

    bool charge_enabled;	/* Register bit status */
    bool power_good;
    bool vbus_good;
    int input_curr_limit;

    struct sc8989x_platform_data *platform_data;
    struct charger_device *chg_dev;
    struct power_supply_desc psy_desc;
    struct power_supply *psy;

    wait_queue_head_t bc12_en_req;
    atomic_t bc12_en_req_cnt;
    struct task_struct *bc12_en_kthread;

    struct regulator_dev *otg_rdev;

};
static struct sc8989x *st_sc;
static const struct charger_properties sc8989x_chg_props = {
    .alias_name = "sc8989x",
};

static int __sc8989x_read_reg(struct sc8989x *sc, u8 reg, u8 *data)
{
    s32 ret;

    ret = i2c_smbus_read_byte_data(sc->client, reg);
    if (ret < 0) {
        pr_err("i2c read fail: can't read from reg 0x%02X\n", reg);
        return ret;
    }

    *data = (u8) ret;

    return 0;
}

static int __sc8989x_write_reg(struct sc8989x *sc, int reg, u8 val)
{
    s32 ret;

    ret = i2c_smbus_write_byte_data(sc->client, reg, val);
    if (ret < 0) {
        pr_err("i2c write fail: can't write 0x%02X to reg 0x%02X: %d\n",
            val, reg, ret);
        return ret;
    }
    return 0;
}

static int sc8989x_read_byte(struct sc8989x *sc, u8 reg, u8 *data)
{
    int ret;

    mutex_lock(&sc->i2c_rw_lock);
    ret = __sc8989x_read_reg(sc, reg, data);
    mutex_unlock(&sc->i2c_rw_lock);

    return ret;
}

static int sc8989x_write_byte(struct sc8989x *sc, u8 reg, u8 data)
{
    int ret;

    mutex_lock(&sc->i2c_rw_lock);
    ret = __sc8989x_write_reg(sc, reg, data);
    mutex_unlock(&sc->i2c_rw_lock);

    if (ret)
        pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);

    return ret;
}

static int sc8989x_update_bits(struct sc8989x *sc, u8 reg, u8 mask, u8 data)
{
    int ret;
    u8 tmp;

    mutex_lock(&sc->i2c_rw_lock);
    ret = __sc8989x_read_reg(sc, reg, &tmp);
    if (ret) {
        pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);
        goto out;
    }

    tmp &= ~mask;
    tmp |= data & mask;

    ret = __sc8989x_write_reg(sc, reg, tmp);
    if (ret)
        pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);

out:
    mutex_unlock(&sc->i2c_rw_lock);
    return ret;
}
static int sc8989x_get_otg_config(struct sc8989x *sc)
{
    uint8_t data_reg = 0;
    int32_t ret = 0;

    ret = sc8989x_read_byte(sc,SC8989X_REG_03,&data_reg);
    printk("dsy  %s %d data_reg 0x%x\n", __func__,__LINE__,data_reg);
    if(ret < 0)
        return ret;

    data_reg = (data_reg & (0x1 << SC8989X_OTG_CONFIG_SHIFT)) >> SC8989X_OTG_CONFIG_SHIFT;
    printk("dsy  %s %d data_reg 0x%x\n", __func__,__LINE__,data_reg);
    return !!data_reg;
}

int sc8989x_is_vbus_on(void)
{
    int status=0;
        
    status = sc8989x_get_otg_config(st_sc);
    printk("%s:%d\n",__func__,status);
    return status;
}
EXPORT_SYMBOL(sc8989x_is_vbus_on);

static int sc8989x_enable_otg(struct sc8989x *sc)
{

    u8 val = SC8989X_OTG_ENABLE << SC8989X_OTG_CONFIG_SHIFT;

    return sc8989x_update_bits(sc, SC8989X_REG_03,
                SC8989X_OTG_CONFIG_MASK, val);
}

static int sc8989x_disable_otg(struct sc8989x *sc)
{
    u8 val = SC8989X_OTG_DISABLE << SC8989X_OTG_CONFIG_SHIFT;

    return sc8989x_update_bits(sc, SC8989X_REG_03,
                SC8989X_OTG_CONFIG_MASK, val);
}
/*
static int sc8989x_disable_hvdcp(struct sc8989x *sc)
{
    int ret;
    u8 val = SC8989X_HVDCP_DISABLE << SC8989X_HVDCPEN_SHIFT;

    ret = sc8989x_update_bits(sc, SC8989X_REG_02, 
                SC8989X_HVDCPEN_MASK, val);
    return ret;
}
*/
static int sc8989x_enable_charger(struct sc8989x *sc)
{
    int ret;

    u8 val = SC8989X_CHG_ENABLE << SC8989X_CHG_CONFIG_SHIFT;

    ret = sc8989x_update_bits(sc, SC8989X_REG_03, 
                SC8989X_CHG_CONFIG_MASK, val);
    return ret;
}

static int sc8989x_disable_charger(struct sc8989x *sc)
{
    int ret;

    u8 val = SC8989X_CHG_DISABLE << SC8989X_CHG_CONFIG_SHIFT;

    ret = sc8989x_update_bits(sc, SC8989X_REG_03, 
                SC8989X_CHG_CONFIG_MASK, val);
    return ret;
}

static int sc8989x_adc_start(struct sc8989x *sc, bool oneshot)
{
    u8 val;
    int ret;
    
    ret = sc8989x_read_byte(sc, SC8989X_REG_02, &val);
    if (ret < 0) {
        dev_err(sc->dev, "%s failed to read register 0x02:%d\n", __func__, ret);
    }
    
    if (((val & SC8989X_CONV_RATE_MASK) >> SC8989X_CONV_RATE_SHIFT) 
            == SC8989X_ADC_CONTINUE_ENABLE)
        return 0;

    if (oneshot) {
        ret = sc8989x_update_bits(sc, SC8989X_REG_02, SC8989X_CONV_START_MASK,
                SC8989X_CONV_START << SC8989X_CONV_START_SHIFT);
    }
    else {
        ret = sc8989x_update_bits(sc, SC8989X_REG_02, SC8989X_CONV_RATE_MASK,
                SC8989X_ADC_CONTINUE_ENABLE << SC8989X_CONV_RATE_SHIFT);
    }
    
    return ret;
}

// static int sc8989x_adc_stop(struct sc8989x *sc)
// {
//     return sc8989x_update_bits(sc, SC8989X_REG_02, SC8989X_CONV_RATE_MASK,
//                 SC8989X_ADC_CONTINUE_DISABLE << SC8989X_CONV_RATE_SHIFT);
// }

int sc8989x_set_chargecurrent(struct sc8989x *sc, int curr)
{
    u8 ichg;

    if (curr < SC8989X_ICHG_BASE)
        curr = SC8989X_ICHG_BASE;

    ichg = (curr - SC8989X_ICHG_BASE)/SC8989X_ICHG_LSB;

    return sc8989x_update_bits(sc, SC8989X_REG_04, 
                        SC8989X_ICHG_MASK, ichg << SC8989X_ICHG_SHIFT);

}

int sc8989x_set_term_current(struct sc8989x *sc, int curr)
{
    u8 iterm;

    if (curr < SC8989X_ITERM_BASE)
        curr = SC8989X_ITERM_BASE;

    iterm = (curr - SC8989X_ITERM_BASE) / SC8989X_ITERM_LSB;

    return sc8989x_update_bits(sc, SC8989X_REG_05, 
                        SC8989X_ITERM_MASK, iterm << SC8989X_ITERM_SHIFT);

}

int sc8989x_get_term_current(struct sc8989x *sc, int *curr)
{
    u8 reg_val;
    int iterm;
    int ret;

    ret = sc8989x_read_byte(sc, SC8989X_REG_05, &reg_val);
    if (!ret) {
        iterm = (reg_val & SC8989X_ITERM_MASK) >> SC8989X_ITERM_SHIFT;
        iterm = iterm * SC8989X_ITERM_LSB + SC8989X_ITERM_BASE;

        *curr = iterm * 1000;
    }
    return ret;
}

int sc8989x_set_prechg_current(struct sc8989x *sc, int curr)
{
    u8 iprechg;

    if (curr < SC8989X_IPRECHG_BASE)
        curr = SC8989X_IPRECHG_BASE;

    iprechg = (curr - SC8989X_IPRECHG_BASE) / SC8989X_IPRECHG_LSB;

    return sc8989x_update_bits(sc, SC8989X_REG_05, 
                SC8989X_IPRECHG_MASK, iprechg << SC8989X_IPRECHG_SHIFT);

}

int sc8989x_set_chargevolt(struct sc8989x *sc, int volt)
{
    u8 val;

    if (volt < SC8989X_VREG_BASE)
        volt = SC8989X_VREG_BASE;

    val = (volt - SC8989X_VREG_BASE)/SC8989X_VREG_LSB;
    return sc8989x_update_bits(sc, SC8989X_REG_06, 
                        SC8989X_VREG_MASK, val << SC8989X_VREG_SHIFT);
}

int sc8989x_get_chargevol(struct sc8989x *sc, int *volt)
{
    u8 reg_val;
    int vchg;
    int ret;

    ret = sc8989x_read_byte(sc, SC8989X_REG_06, &reg_val);
    if (!ret) {
        vchg = (reg_val & SC8989X_VREG_MASK) >> SC8989X_VREG_SHIFT;
        vchg = vchg * SC8989X_VREG_LSB + SC8989X_VREG_BASE;
        *volt = vchg * 1000;
    }
    return ret;
}

int sc8989x_adc_read_vbus_volt(struct sc8989x *sc, u32 *vol)
{
    uint8_t val;
    int volt;
    int ret;

    sc8989x_adc_start(sc, true);
    msleep(50);

    ret = sc8989x_read_byte(sc, SC8989X_REG_11, &val);
    if (ret < 0) {
        dev_err(sc->dev, "read vbus voltage failed :%d\n", ret);
    } else{
        volt = SC8989X_VBUSV_BASE + ((val & SC8989X_VBUSV_MASK) >> 
            SC8989X_VBUSV_SHIFT) * SC8989X_VBUSV_LSB;
        *vol = volt * 1000;
    }

    return ret;
}

int sc8989x_adc_read_charge_current(struct sc8989x *sc, u32 *cur)
{
    uint8_t val;
    int curr;
    int ret;

    sc8989x_adc_start(sc, true);
    msleep(50);

    ret = sc8989x_read_byte(sc, SC8989X_REG_12, &val);
    if (ret < 0) {
        dev_err(sc->dev, "read charge current failed :%d\n", ret);
    } else{
        curr = (int)(SC8989X_ICHGR_BASE + ((val & SC8989X_ICHGR_MASK) >> 
            SC8989X_ICHGR_SHIFT) * SC8989X_ICHGR_LSB) ;
        *cur = curr * 1000; 
    }

    return ret;
}

int sc8989x_set_force_vindpm(struct sc8989x *sc, bool en)
{
    u8 val;

    if (en)
        val = SC8989X_FORCE_VINDPM_ENABLE;
    else
        val = SC8989X_FORCE_VINDPM_DISABLE;
    
    sc->power_good = false;
    return sc8989x_update_bits(sc, SC8989X_REG_0D, 
                SC8989X_FORCE_VINDPM_MASK, val << SC8989X_FORCE_VINDPM_SHIFT);
}

int sc8989x_set_input_volt_limit(struct sc8989x *sc, int volt)
{
    u8 val;

    if (volt < SC8989X_VINDPM_BASE)
        volt = SC8989X_VINDPM_BASE;
    
    sc8989x_set_force_vindpm(sc, true);

    val = (volt - SC8989X_VINDPM_BASE) / SC8989X_VINDPM_LSB;
    return sc8989x_update_bits(sc, SC8989X_REG_0D, 
                        SC8989X_VINDPM_MASK, val << SC8989X_VINDPM_SHIFT);
}

int sc8989x_set_input_current_limit(struct sc8989x *sc, int curr)
{
    u8 val;

    if (curr > sc->input_curr_limit)
        curr = sc->input_curr_limit;

    if (curr < SC8989X_IINLIM_BASE)
        curr = SC8989X_IINLIM_BASE;

    val = (curr - SC8989X_IINLIM_BASE) / SC8989X_IINLIM_LSB;

    return sc8989x_update_bits(sc, SC8989X_REG_00, SC8989X_IINLIM_MASK, 
                        val << SC8989X_IINLIM_SHIFT);
}

int sc8989x_get_input_volt_limit(struct sc8989x *sc, u32 *volt)
{
    u8 reg_val;
    int vchg;
    int ret;

    ret = sc8989x_read_byte(sc, SC8989X_REG_0D, &reg_val);
    if (!ret) {
        vchg = (reg_val & SC8989X_VINDPM_MASK) >> SC8989X_VINDPM_SHIFT;
        vchg = vchg * SC8989X_VINDPM_LSB + SC8989X_VINDPM_BASE;
        *volt = vchg * 1000;
    }
    return ret;
}

int sc8989x_get_input_current_limit(struct  sc8989x *sc, u32 *curr)
{
    u8 reg_val;
    int icl;
    int ret;

    ret = sc8989x_read_byte(sc, SC8989X_REG_00, &reg_val);
    if (!ret) {
        icl = (reg_val & SC8989X_IINLIM_MASK) >> SC8989X_IINLIM_SHIFT;
        icl = icl * SC8989X_IINLIM_LSB + SC8989X_IINLIM_BASE;
        *curr = icl * 1000;
    }

    return ret;
}

int sc8989x_set_watchdog_timer(struct sc8989x *sc, u8 timeout)
{
    u8 val;

    val = (timeout - SC8989X_WDT_BASE) / SC8989X_WDT_LSB;
    val <<= SC8989X_WDT_SHIFT;

    return sc8989x_update_bits(sc, SC8989X_REG_07, 
                        SC8989X_WDT_MASK, val); 
}

int sc8989x_disable_watchdog_timer(struct sc8989x *sc)
{
    u8 val = SC8989X_WDT_DISABLE << SC8989X_WDT_SHIFT;

    return sc8989x_update_bits(sc, SC8989X_REG_07, 
                        SC8989X_WDT_MASK, val);
}

int sc8989x_reset_watchdog_timer(struct sc8989x *sc)
{
    u8 val = SC8989X_WDT_RESET << SC8989X_WDT_RESET_SHIFT;

    return sc8989x_update_bits(sc, SC8989X_REG_03, 
                        SC8989X_WDT_RESET_MASK, val);
}

int sc8989x_force_dpdm(struct sc8989x *sc)
{
    int ret;
    u8 val = SC8989X_FORCE_DPDM << SC8989X_FORCE_DPDM_SHIFT;

    ret = sc8989x_update_bits(sc, SC8989X_REG_02, 
                        SC8989X_FORCE_DPDM_MASK, val);

    pr_info("sc8989x_force_dpdm Force DPDM %s\n", !ret ? "successfully" : "failed");
    
    return ret;

}

int sc8989x_reset_chip(struct sc8989x *sc)
{
    int ret;
    u8 val = SC8989X_RESET << SC8989X_RESET_SHIFT;

    ret = sc8989x_update_bits(sc, SC8989X_REG_14, 
                        SC8989X_RESET_MASK, val);
    return ret;
}

int sc8989x_enable_hiz_mode(struct sc8989x *sc, bool en)
{
    u8 val;

    if (en) {
        val = SC8989X_HIZ_ENABLE << SC8989X_ENHIZ_SHIFT;
    } else {
        val = SC8989X_HIZ_DISABLE << SC8989X_ENHIZ_SHIFT;
    }

    return sc8989x_update_bits(sc, SC8989X_REG_00, 
                        SC8989X_ENHIZ_MASK, val);

}

int sc8989x_exit_hiz_mode(struct sc8989x *sc)
{

    u8 val = SC8989X_HIZ_DISABLE << SC8989X_ENHIZ_SHIFT;

    return sc8989x_update_bits(sc, SC8989X_REG_00, 
                        SC8989X_ENHIZ_MASK, val);

}

int sc8989x_get_hiz_mode(struct sc8989x *sc, u8 *state)
{
    u8 val;
    int ret;

    ret = sc8989x_read_byte(sc, SC8989X_REG_00, &val);
    if (ret)
        return ret;
    *state = (val & SC8989X_ENHIZ_MASK) >> SC8989X_ENHIZ_SHIFT;

    return 0;
}

static int sc8989x_enable_term(struct sc8989x *sc, bool enable)
{
    u8 val;
    int ret;

    if (enable)
        val = SC8989X_TERM_ENABLE << SC8989X_EN_TERM_SHIFT;
    else
        val = SC8989X_TERM_DISABLE << SC8989X_EN_TERM_SHIFT;

    ret = sc8989x_update_bits(sc, SC8989X_REG_07, 
                        SC8989X_EN_TERM_MASK, val);

    return ret;
}

int sc8989x_set_boost_current(struct sc8989x *sc, int curr)
{
    u8 temp;

    if (curr == 500)
        temp = SC8989X_BOOST_LIM_500MA;
    else if (curr == 750)
        temp = SC8989X_BOOST_LIM_750MA;
    else if (curr == 1200)
        temp = SC8989X_BOOST_LIM_1200MA;
    else
        temp = SC8989X_BOOST_LIM_1400MA;

    return sc8989x_update_bits(sc, SC8989X_REG_0A, 
                SC8989X_BOOST_LIM_MASK, 
                temp << SC8989X_BOOST_LIM_SHIFT);

}

int sc8989x_set_boost_voltage(struct sc8989x *sc, int volt)
{
    u8 val = 0;

    if (volt < SC8989X_BOOSTV_BASE)
        volt = SC8989X_BOOSTV_BASE;
    if (volt > SC8989X_BOOSTV_BASE 
            + (SC8989X_BOOSTV_MASK >> SC8989X_BOOSTV_SHIFT) 
            * SC8989X_BOOSTV_LSB)
        volt = SC8989X_BOOSTV_BASE 
            + (SC8989X_BOOSTV_MASK >> SC8989X_BOOSTV_SHIFT) 
            * SC8989X_BOOSTV_LSB;

    val = ((volt - SC8989X_BOOSTV_BASE) / SC8989X_BOOSTV_LSB) 
            << SC8989X_BOOSTV_SHIFT;

    return sc8989x_update_bits(sc, SC8989X_REG_0A, 
                SC8989X_BOOSTV_MASK, val);


}

static int sc8989x_enable_ico(struct sc8989x* sc, bool enable)
{
    u8 val;
    int ret;

    if (enable)
        val = SC8989X_ICO_ENABLE << SC8989X_ICOEN_SHIFT;
    else
        val = SC8989X_ICO_DISABLE << SC8989X_ICOEN_SHIFT;

    ret = sc8989x_update_bits(sc, SC8989X_REG_02, SC8989X_ICOEN_MASK, val);

    return ret;

}


static int sc8989x_enable_safety_timer(struct sc8989x *sc)
{
    const u8 val = SC8989X_CHG_TIMER_ENABLE << SC8989X_EN_TIMER_SHIFT;

    return sc8989x_update_bits(sc, SC8989X_REG_07, SC8989X_EN_TIMER_MASK,
                val);
}

static int sc8989x_disable_safety_timer(struct sc8989x *sc)
{
    const u8 val = SC8989X_CHG_TIMER_DISABLE << SC8989X_EN_TIMER_SHIFT;

    return sc8989x_update_bits(sc, SC8989X_REG_07, SC8989X_EN_TIMER_MASK,
                val);

}

static struct sc8989x_platform_data *sc8989x_parse_dt(struct device_node *np,
                            struct sc8989x *sc)
{
    int ret;
    struct sc8989x_platform_data *pdata;
    pdata = devm_kzalloc(sc->dev, sizeof(struct sc8989x_platform_data),
                GFP_KERNEL);
    if (!pdata)
        return NULL;

    if (of_property_read_string(np, "charger_name", &sc->chg_dev_name) < 0) {
        sc->chg_dev_name = "primary_chg";
        pr_warn("no charger name\n");
    }

    if (of_property_read_string(np, "eint_name", &sc->eint_name) < 0) {
        sc->eint_name = "chr_stat";
        pr_warn("no eint name\n");
    }

    ret = of_property_read_u32(np, "sc,sc8989x,usb-vlim", &pdata->usb.vlim);
    if (ret) {
        pdata->usb.vlim = 4500;
        pr_err("Failed to read node of sc,sc8989x,usb-vlim\n");
    }

    sc->irq_gpio = of_get_named_gpio(np, "sc,intr-gpio", 0);
    if (sc->irq_gpio < 0)
        pr_err("sc,intr-gpio is not available\n");

    ret = of_property_read_u32(np, "sc,sc8989x,usb-ilim", &pdata->usb.ilim);
    if (ret) {
        pdata->usb.ilim = 2000;
        pr_err("Failed to read node of sc,sc8989x,usb-ilim\n");
    }

    ret = of_property_read_u32(np, "sc,sc8989x,usb-vreg", &pdata->usb.vreg);
    if (ret) {
        pdata->usb.vreg = 4200;
        pr_err("Failed to read node of sc,sc8989x,usb-vreg\n");
    }

    ret = of_property_read_u32(np, "sc,sc8989x,usb-ichg", &pdata->usb.ichg);
    if (ret) {
        pdata->usb.ichg = 2000;
        pr_err("Failed to read node of sc,sc8989x,usb-ichg\n");
    }

    ret = of_property_read_u32(np, "sc,sc8989x,precharge-current",
                &pdata->iprechg);
    if (ret) {
        pdata->iprechg = 180;
        pr_err("Failed to read node of sc,sc8989x,precharge-current\n");
    }

    ret = of_property_read_u32(np, "sc,sc8989x,termination-current",
                &pdata->iterm);
    if (ret) {
        pdata->iterm = 180;
        pr_err
            ("Failed to read node of sc,sc8989x,termination-current\n");
    }

    ret =
        of_property_read_u32(np, "sc,sc8989x,boost-voltage",
                &pdata->boostv);
    if (ret) {
        pdata->boostv = 5000;
        pr_err("Failed to read node of sc,sc8989x,boost-voltage\n");
    }

    ret =
        of_property_read_u32(np, "sc,sc8989x,boost-current",
                &pdata->boosti);
    if (ret) {
        pdata->boosti = 1200;
        pr_err("Failed to read node of sc,sc8989x,boost-current\n");
    }


    return pdata;
}

static int sc8989x_get_charge_stat(struct sc8989x *sc, int *state)
{
    int ret;
    u8 val;

    ret = sc8989x_read_byte(sc, SC8989X_REG_0B, &val);
    printk("dsy  %s %d val 0x%x\n", __func__,__LINE__,val);
    if (!ret) {
        if ((val & SC8989X_VBUS_STAT_MASK) >> SC8989X_VBUS_STAT_SHIFT 
                == SC8989X_VBUS_TYPE_OTG) {
            *state = POWER_SUPPLY_STATUS_DISCHARGING;
            return ret;
        }
        val = val & SC8989X_CHRG_STAT_MASK;
        val = val >> SC8989X_CHRG_STAT_SHIFT;
        switch (val)
        {
        case SC8989X_CHRG_STAT_IDLE:
            ret = sc8989x_read_byte(sc, SC8989X_REG_11, &val);
             printk("dsy  %s %d SC8989X_CHRG_STAT_IDLE val 0x%x\n", __func__,__LINE__,val);
            if (val & SC8989X_VBUS_GD_MASK) {
                *state = POWER_SUPPLY_STATUS_CHARGING;
            } else {
                *state = POWER_SUPPLY_STATUS_NOT_CHARGING;
            }
            break;
        case SC8989X_CHRG_STAT_PRECHG:
        case SC8989X_CHRG_STAT_FASTCHG:
        printk("dsy  %s %d SC8989X_CHRG_STAT_FASTCHG SC8989X_CHRG_STAT_PRECHG\n", __func__,__LINE__);
            *state = POWER_SUPPLY_STATUS_CHARGING;
            break;
        case SC8989X_CHRG_STAT_CHGDONE:
         printk("dsy  %s %d SC8989X_CHRG_STAT_CHGDONE \n", __func__,__LINE__);
            *state = POWER_SUPPLY_STATUS_FULL;
            break;
        default:
        printk("dsy  %s %d POWER_SUPPLY_STATUS_UNKNOWN \n", __func__,__LINE__);
            *state = POWER_SUPPLY_STATUS_UNKNOWN;
            break;
        }
    }
    pr_err("%s---->%d  %02x\n", __func__, *state, val);

    return ret;
}
// extern void st_connect_host(void);
// extern void st_disconnect_host(void);
static int sc8989x_get_charger_type(struct sc8989x *sc)
{
    int ret;

    u8 reg_val = 0;
    int vbus_stat = 0;

    ret = sc8989x_read_byte(sc, SC8989X_REG_0B, &reg_val);
printk("dsy  %s %d reg_val 0x%x\n", __func__,__LINE__,reg_val);
    if (ret)
        return ret;

    vbus_stat = (reg_val & SC8989X_VBUS_STAT_MASK);
    vbus_stat >>= SC8989X_VBUS_STAT_SHIFT;
printk("dsy  %s %d vbus_stat %d\n", __func__,__LINE__,vbus_stat);
    switch (vbus_stat) {

    case SC8989X_VBUS_TYPE_NONE:
     printk("dsy  %s %d SC8989X_VBUS_TYPE_NONE %d \n", __func__,__LINE__,SC8989X_VBUS_TYPE_NONE);
        sc->psy_usb_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
        sc->chg_type = POWER_SUPPLY_TYPE_UNKNOWN;
        sc->psy_desc.type = POWER_SUPPLY_TYPE_UNKNOWN;
       // st_disconnect_host();
        break;
    case SC8989X_VBUS_TYPE_SDP:
     printk("dsy  %s %d SC8989X_VBUS_TYPE_SDP %d \n", __func__,__LINE__,SC8989X_VBUS_TYPE_SDP);
        sc->psy_usb_type = POWER_SUPPLY_USB_TYPE_SDP;
        sc->chg_type = POWER_SUPPLY_TYPE_USB;
        sc->psy_desc.type = POWER_SUPPLY_TYPE_USB;
        break;
    case SC8989X_VBUS_TYPE_CDP:
     printk("dsy  %s %d SC8989X_VBUS_TYPE_CDP %d \n", __func__,__LINE__,SC8989X_VBUS_TYPE_CDP);
        sc->psy_usb_type = POWER_SUPPLY_USB_TYPE_CDP;
        sc->chg_type = POWER_SUPPLY_TYPE_USB_CDP;
        sc->psy_desc.type = POWER_SUPPLY_TYPE_USB_CDP;
        break;
    case SC8989X_VBUS_TYPE_DCP:
    case SC8989X_VBUS_TYPE_HVDCP:
     printk("dsy  %s %d SC8989X_VBUS_TYPE_DCP %d \n", __func__,__LINE__,SC8989X_VBUS_TYPE_DCP);
        sc->psy_usb_type = POWER_SUPPLY_USB_TYPE_DCP;
        sc->chg_type = POWER_SUPPLY_TYPE_USB_DCP;
        sc->psy_desc.type = POWER_SUPPLY_TYPE_USB_DCP;
        break;
    case SC8989X_VBUS_TYPE_UNKNOWN:
     printk("dsy  %s %d SC8989X_VBUS_TYPE_UNKNOWN %d \n", __func__,__LINE__,SC8989X_VBUS_TYPE_UNKNOWN);
      //  sc->psy_usb_type =  POWER_SUPPLY_USB_TYPE_UNKNOWN;
      //  sc->chg_type = POWER_SUPPLY_TYPE_USB;
        sc->psy_usb_type =  POWER_SUPPLY_USB_TYPE_CDP;
        sc->chg_type = POWER_SUPPLY_TYPE_USB_CDP;
        sc->psy_desc.type = POWER_SUPPLY_TYPE_USB_CDP;
     //   st_connect_host();
        break;
    case SC8989X_VBUS_TYPE_NON_STD:
     printk("dsy  %s %d SC8989X_VBUS_TYPE_NON_STD %d \n", __func__,__LINE__,SC8989X_VBUS_TYPE_NON_STD);
        sc->psy_usb_type = POWER_SUPPLY_USB_TYPE_DCP;
        sc->chg_type = POWER_SUPPLY_TYPE_USB_DCP;
        sc->psy_desc.type = POWER_SUPPLY_TYPE_USB_DCP;
        break;
    default:
     printk("dsy  %s %d POWER_SUPPLY_USB_TYPE_UNKNOWN %d \n", __func__,__LINE__,POWER_SUPPLY_USB_TYPE_UNKNOWN);
        sc->psy_usb_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
        sc->chg_type = POWER_SUPPLY_TYPE_UNKNOWN;
        sc->psy_desc.type = POWER_SUPPLY_TYPE_UNKNOWN;
        break;
    }
    
    pr_err("%s ---->0x%02x  chg type %d  usb type %d\n", __func__, reg_val, 
        sc->chg_type, sc->psy_usb_type);

    return 0;
}
static bool is_usb_rdy(void)
{
    bool ready = true;
    struct device_node *node;
    node = of_parse_phandle(st_sc->dev->of_node, "usb", 0);
    if (node) {
        ready = !of_property_read_bool(node, "cdp-block");
        dev_err(st_sc->dev, "usb ready = %d\n", ready);
    } else
        dev_err(st_sc->dev, "usb node missing or invalid\n");
    return ready;
}
static int32_t sc8989x_bc12_preprocess(struct sc8989x *sc)
{
    atomic_inc(&sc->bc12_en_req_cnt);
    wake_up(&sc->bc12_en_req);
    return 0;
}

void sc8989x_process_irq(struct sc8989x *sc){

    sc8989x_bc12_preprocess(sc);
}

static void sc8989x_dump_regs(struct sc8989x *sc);
static irqreturn_t sc8989x_irq_handler(int irq, void *data)
{
    // int ret;
    // u8 reg_val;
    // bool prev_pg;
    // bool prev_vbus_pg;
    struct sc8989x *sc = data;

     sc8989x_process_irq(sc);

    // ret = sc8989x_read_byte(sc, SC8989X_REG_0B, &reg_val);
    // if (ret)
    //     return IRQ_HANDLED;
    // prev_pg = sc->power_good;
    // sc->power_good = !!(reg_val & SC8989X_PG_STAT_MASK);

    // ret = sc8989x_read_byte(sc, SC8989X_REG_11, &reg_val);
    // if (ret)
    //     return IRQ_HANDLED;

    // prev_vbus_pg = sc->vbus_good;

    // sc->vbus_good = !!(reg_val & SC8989X_VBUS_GD_MASK);

    // if (!prev_vbus_pg && sc->vbus_good){
    //     sc->input_curr_limit = 3000;
    //     pr_err("adapter/usb inserted\n");
    // } else if (prev_vbus_pg && !sc->vbus_good) {
    //     pr_err("adapter/usb removed\n");
    // }

    // if (!prev_pg && sc->power_good)
    //     sc8989x_get_charger_type(sc);
    
    // sc8989x_dump_regs(sc);

    // power_supply_changed(sc->psy);

    return IRQ_HANDLED;
}

static int sc8989x_register_interrupt(struct sc8989x *sc)
{
    int ret = 0;

    ret = devm_gpio_request(sc->dev, sc->irq_gpio, "chr-irq");
    if (ret < 0) {
        pr_err("failed to request GPIO%d ; ret = %d", sc->irq_gpio, ret);
        return ret;
    }

    ret = gpio_direction_input(sc->irq_gpio);
    if (ret < 0) {
        pr_err("failed to set GPIO%d ; ret = %d", sc->irq_gpio, ret);
        return ret;
    }

    sc->irq = gpio_to_irq(sc->irq_gpio);
    if (ret < 0) {
        pr_err("failed gpio to irq GPIO%d ; ret = %d", sc->irq_gpio, ret);
        return ret;
    }

    ret = devm_request_threaded_irq(sc->dev, sc->irq, NULL,
                    sc8989x_irq_handler,
                    IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
                    "chr_stat", sc);
    if (ret < 0) {
        pr_err("request thread irq failed:%d\n", ret);
        return ret;
    }else{
        pr_err("request thread irq pass:%d  sc->irq =%d\n", ret, sc->irq);
    }

    enable_irq_wake(sc->irq);

    return 0;
}

static int sc8989x_init_device(struct sc8989x *sc)
{
    int ret;

    sc8989x_disable_watchdog_timer(sc);
    sc8989x_enable_ico(sc, false);

    ret = sc8989x_set_prechg_current(sc, sc->platform_data->iprechg);
    if (ret)
        pr_err("Failed to set prechg current, ret = %d\n", ret);

    ret = sc8989x_set_term_current(sc, sc->platform_data->iterm);
    if (ret)
        pr_err("Failed to set termination current, ret = %d\n", ret);

    ret = sc8989x_set_boost_voltage(sc, sc->platform_data->boostv);
    if (ret)
        pr_err("Failed to set boost voltage, ret = %d\n", ret);

    ret = sc8989x_set_boost_current(sc, sc->platform_data->boosti);
    if (ret)
        pr_err("Failed to set boost current, ret = %d\n", ret);

    return 0;
}

static void determine_initial_status(struct sc8989x *sc)
{
    sc8989x_irq_handler(sc->irq, (void *) sc);
}

static int sc8989x_detect_device(struct sc8989x *sc)
{
    int ret;
    u8 data;

    ret = sc8989x_read_byte(sc, SC8989X_REG_14, &data);
    if (!ret) {
        sc->part_no = (data & SC8989X_PN_MASK) >> SC8989X_PN_SHIFT;
        sc->revision =
            (data & SC8989X_DEV_REV_MASK) >> SC8989X_DEV_REV_SHIFT;
    }

    return ret;
}

static void sc8989x_dump_regs(struct sc8989x *sc)
{
    int addr;
    u8 val;
    int ret;

    for (addr = 0x0; addr <= 0x14; addr++) {
        ret = sc8989x_read_byte(sc, addr, &val);
        if (ret == 0)
            pr_err("Reg[%.2x] = 0x%.2x\n", addr, val);
    }
}

static ssize_t
sc8989x_show_registers(struct device *dev, struct device_attribute *attr,
            char *buf)
{
    struct sc8989x *sc = dev_get_drvdata(dev);
    u8 addr;
    u8 val;
    u8 tmpbuf[200];
    int len;
    int idx = 0;
    int ret;

    idx = snprintf(buf, PAGE_SIZE, "%s:\n", "sc8989x Reg");
    for (addr = 0x0; addr <= 0x14; addr++) {
        ret = sc8989x_read_byte(sc, addr, &val);
        if (ret == 0) {
            len = snprintf(tmpbuf, PAGE_SIZE - idx,
                    "Reg[%.2x] = 0x%.2x\n", addr, val);
            memcpy(&buf[idx], tmpbuf, len);
            idx += len;
        }
    }

    return idx;
}

static ssize_t
sc8989x_store_registers(struct device *dev,
            struct device_attribute *attr, const char *buf,
            size_t count)
{
    struct sc8989x *sc = dev_get_drvdata(dev);
    int ret;
    unsigned int reg;
    unsigned int val;

    ret = sscanf(buf, "%x %x", &reg, &val);
    if (ret == 2 && reg < 0x14) {
        sc8989x_write_byte(sc, (unsigned char) reg,
                (unsigned char) val);
    }

    return count;
}

static DEVICE_ATTR(registers, S_IRUGO | S_IWUSR, sc8989x_show_registers,
        sc8989x_store_registers);

static struct attribute *sc8989x_attributes[] = {
    &dev_attr_registers.attr,
    NULL,
};

static const struct attribute_group sc8989x_attr_group = {
    .attrs = sc8989x_attributes,
};

static int sc8989x_charging(struct charger_device *chg_dev, bool enable)
{
    struct sc8989x *sc = dev_get_drvdata(&chg_dev->dev);
    int ret = 0;
    u8 val;

    if (enable)
        ret = sc8989x_enable_charger(sc);
    else
        ret = sc8989x_disable_charger(sc);

    pr_err("%s charger %s\n", enable ? "enable" : "disable",
        !ret ? "successfully" : "failed");

    ret = sc8989x_read_byte(sc, SC8989X_REG_03, &val);

    if (!ret)
        sc->charge_enabled = !!(val & SC8989X_CHG_CONFIG_MASK);

    return ret;
}

static int sc8989x_plug_in(struct charger_device *chg_dev)
{
    int ret;
    
    ret = sc8989x_charging(chg_dev, true);
    if (ret)
        pr_err("Failed to enable charging:%d\n", ret);

    return ret;
}

static int sc8989x_plug_out(struct charger_device *chg_dev)
{
    int ret;

    ret = sc8989x_charging(chg_dev, false);

    if (ret)
        pr_err("Failed to disable charging:%d\n", ret);

    return ret;
}

static int sc8989x_dump_register(struct charger_device *chg_dev)
{
    struct sc8989x *sc = dev_get_drvdata(&chg_dev->dev);

    sc8989x_dump_regs(sc);

    return 0;
}

static int sc8989x_is_charging_enable(struct charger_device *chg_dev, bool *en)
{
    struct sc8989x *sc = dev_get_drvdata(&chg_dev->dev);

    *en = sc->charge_enabled;

    return 0;
}

static int sc8989x_is_charging_done(struct charger_device *chg_dev, bool *done)
{
    struct sc8989x *sc = dev_get_drvdata(&chg_dev->dev);
    int ret;
    u8 val;

    ret = sc8989x_read_byte(sc, SC8989X_REG_0B, &val);
    if (!ret) {
        val = val & SC8989X_CHRG_STAT_MASK;
        val = val >> SC8989X_CHRG_STAT_SHIFT;
        *done = (val == SC8989X_CHRG_STAT_CHGDONE);
    }

    return ret;
}

static int sc8989x_set_ichg(struct charger_device *chg_dev, u32 curr)
{
    struct sc8989x *sc = dev_get_drvdata(&chg_dev->dev);

    pr_err("charge curr = %d\n", curr);

    return sc8989x_set_chargecurrent(sc, curr / 1000);
}

static int sc8989x_get_ichg(struct charger_device *chg_dev, u32 *curr)
{
    struct sc8989x *sc = dev_get_drvdata(&chg_dev->dev);
    u8 reg_val;
    int ichg;
    int ret;

    ret = sc8989x_read_byte(sc, SC8989X_REG_04, &reg_val);
    if (!ret) {
        ichg = (reg_val & SC8989X_ICHG_MASK) >> SC8989X_ICHG_SHIFT;
        ichg = ichg * SC8989X_ICHG_LSB + SC8989X_ICHG_BASE;
        *curr = ichg * 1000;
    }

    return ret;
}

static int sc8989x_get_min_ichg(struct charger_device *chg_dev, u32 *curr)
{
    *curr = 60 * 1000;

    return 0;
}

static int sc8989x_set_vchg(struct charger_device *chg_dev, u32 volt)
{
    struct sc8989x *sc = dev_get_drvdata(&chg_dev->dev);

    pr_err("charge volt = %d\n", volt);

    return sc8989x_set_chargevolt(sc, volt / 1000);
}

static int sc8989x_get_vchg(struct charger_device *chg_dev, u32 *volt)
{
    struct sc8989x *sc = dev_get_drvdata(&chg_dev->dev);

    return sc8989x_get_chargevol(sc, volt);
}

static int sc8989x_set_ivl(struct charger_device *chg_dev, u32 volt)
{
    struct sc8989x *sc = dev_get_drvdata(&chg_dev->dev);

    pr_err("vindpm volt = %d\n", volt);

    return sc8989x_set_input_volt_limit(sc, volt / 1000);
}

static int sc8989x_get_ivl(struct charger_device *chgdev, u32 *volt)
{
    struct sc8989x *sc = dev_get_drvdata(&chgdev->dev);

    return sc8989x_get_input_volt_limit(sc, volt);
}

static int sc8989x_get_vbus_adc(struct charger_device *chgdev, u32 *vbus)
{
    struct sc8989x *sc = dev_get_drvdata(&chgdev->dev);
    
    return sc8989x_adc_read_vbus_volt(sc, vbus);
}

static int sc8989x_get_ibus_adc(struct charger_device *chgdev, u32 *ibus)
{
    struct sc8989x *sc = dev_get_drvdata(&chgdev->dev);
    
    return sc8989x_adc_read_charge_current(sc, ibus);
}

static int sc8989x_set_icl(struct charger_device *chg_dev, u32 curr)
{
    struct sc8989x *sc = dev_get_drvdata(&chg_dev->dev);

    pr_err("indpm curr = %d\n", curr);

    return sc8989x_set_input_current_limit(sc, curr / 1000);
}

static int sc8989x_get_icl(struct charger_device *chg_dev, u32 *curr)
{
    struct sc8989x *sc = dev_get_drvdata(&chg_dev->dev);
    
    return sc8989x_get_input_current_limit(sc, curr);
}

static int sc8989x_kick_wdt(struct charger_device *chg_dev)
{
    struct sc8989x *sc = dev_get_drvdata(&chg_dev->dev);

    return sc8989x_reset_watchdog_timer(sc);
}

static int sc8989x_set_ieoc(struct charger_device *chgdev, u32 ieoc)
{
    struct sc8989x *sc = dev_get_drvdata(&chgdev->dev);
    
    return sc8989x_set_term_current(sc, ieoc / 1000);
}

static int sc8989x_enable_te(struct charger_device *chgdev, bool en)
{
    struct sc8989x *sc = dev_get_drvdata(&chgdev->dev);

    return sc8989x_enable_term(sc, en);
}

static int sc8989x_enable_hz(struct charger_device *chgdev, bool en)
{
    struct sc8989x *sc = dev_get_drvdata(&chgdev->dev);

    return sc8989x_enable_hiz_mode(sc, en);
}

static int sc8989x_set_otg(struct charger_device *chg_dev, bool en)
{
    int ret;
    struct sc8989x *sc = dev_get_drvdata(&chg_dev->dev);

    if (en) {
        ret = sc8989x_disable_charger(sc);
        ret = sc8989x_enable_otg(sc);
    }
    else {
        ret = sc8989x_disable_otg(sc);
        ret = sc8989x_enable_charger(sc);
    }

    pr_err("%s OTG %s\n", en ? "enable" : "disable",
        !ret ? "successfully" : "failed");

    return ret;
}

static int sc8989x_set_safety_timer(struct charger_device *chg_dev, bool en)
{
    struct sc8989x *sc = dev_get_drvdata(&chg_dev->dev);
    int ret;

    if (en)
        ret = sc8989x_enable_safety_timer(sc);
    else
        ret = sc8989x_disable_safety_timer(sc);

    return ret;
}

static int sc8989x_is_safety_timer_enabled(struct charger_device *chg_dev,
                    bool *en)
{
    struct sc8989x *sc = dev_get_drvdata(&chg_dev->dev);
    int ret;
    u8 reg_val;

    ret = sc8989x_read_byte(sc, SC8989X_REG_07, &reg_val);

    if (!ret)
        *en = !!(reg_val & SC8989X_EN_TIMER_MASK);

    return ret;
}

static int sc8989x_set_boost_ilmt(struct charger_device *chg_dev, u32 curr)
{
    struct sc8989x *sc = dev_get_drvdata(&chg_dev->dev);
    int ret;

    pr_err("otg curr = %d\n", curr);

    ret = sc8989x_set_boost_current(sc, curr / 1000);

    return ret;
}

static enum power_supply_usb_type sc8989x_chg_psy_usb_types[] = {
    POWER_SUPPLY_USB_TYPE_UNKNOWN,
    POWER_SUPPLY_USB_TYPE_SDP,
    POWER_SUPPLY_USB_TYPE_CDP,
    POWER_SUPPLY_USB_TYPE_DCP,
};


static enum power_supply_property sc8989x_chg_psy_properties[] = {
    POWER_SUPPLY_PROP_MANUFACTURER,
    POWER_SUPPLY_PROP_ONLINE,
    POWER_SUPPLY_PROP_STATUS,
    POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
    POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE,
    POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
    POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT,
    POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT,
    POWER_SUPPLY_PROP_USB_TYPE,
    POWER_SUPPLY_PROP_CURRENT_MAX,
    POWER_SUPPLY_PROP_VOLTAGE_MAX,
};

static int sc8989x_chg_property_is_writeable(struct power_supply *psy,
                        enum power_supply_property psp)
{
    switch (psp) {
    case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
    case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
    case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
    case POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT:
    case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
    case POWER_SUPPLY_PROP_STATUS:
    case POWER_SUPPLY_PROP_ONLINE:
    case POWER_SUPPLY_PROP_ENERGY_EMPTY:
        return 1;
    default:
        return 0;
    }
    return 0;
}

static int sc8989x_chg_get_property(struct power_supply *psy,
                enum power_supply_property psp,
                union power_supply_propval *val)
{
    int ret = 0;
    u32 _val;
    int data;
    struct sc8989x *sc = power_supply_get_drvdata(psy);
	printk("%s==========psp:%d\n",__func__,psp);
    switch (psp) {
    case POWER_SUPPLY_PROP_MANUFACTURER:
    printk("dsy  %s %d POWER_SUPPLY_PROP_MANUFACTURER \n", __func__,__LINE__);
        val->strval = "SouthChip";
        break;
    case POWER_SUPPLY_PROP_ONLINE:
     printk("dsy  %s %d POWER_SUPPLY_PROP_ONLINE sc->vbus_good %d \n", __func__,__LINE__,sc->vbus_good);
        val->intval = sc->vbus_good;
        break;
    case POWER_SUPPLY_PROP_STATUS:
    printk("dsy  %s %d POWER_SUPPLY_PROP_STATUS \n", __func__,__LINE__);
        ret = sc8989x_get_charge_stat(sc, &data);
        if (ret < 0)
            break;
        val->intval = data;
        break;
    case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
    printk("dsy  %s %d POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT \n", __func__,__LINE__);
        val->intval = 0;
        break;
    case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
       printk("dsy  %s %d POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE \n", __func__,__LINE__);
        ret = sc8989x_get_chargevol(sc, &data);
        if (ret < 0)
            break;
        val->intval = data;
        break;
    case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
     printk("dsy  %s %d POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT \n", __func__,__LINE__);
        ret = sc8989x_get_input_current_limit(sc, &_val);
        if (ret < 0)
            break;
        val->intval = _val;
        break;
    case POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT:
    printk("dsy  %s %d POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT \n", __func__,__LINE__);
        ret = sc8989x_get_input_volt_limit(sc, &_val);
        if (ret < 0)
            break;
        val->intval = _val;
        break;
    case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
      printk("dsy  %s %d POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT \n", __func__,__LINE__);
        ret = sc8989x_get_term_current(sc, &data);
        if (ret < 0)
            break;
        val->intval = data;
        break;
    case POWER_SUPPLY_PROP_USB_TYPE:
      printk("dsy  %s %d POWER_SUPPLY_PROP_USB_TYPE \n", __func__,__LINE__);
        val->intval = sc->psy_usb_type;
        break;
    case POWER_SUPPLY_PROP_CURRENT_MAX:
    printk("dsy  %s %d POWER_SUPPLY_PROP_CURRENT_MAX \n", __func__,__LINE__);
        if (sc->chg_type == POWER_SUPPLY_TYPE_USB)
            val->intval = 500000;
        break;
    case POWER_SUPPLY_PROP_VOLTAGE_MAX:
       printk("dsy  %s %d POWER_SUPPLY_PROP_VOLTAGE_MAX \n", __func__,__LINE__);
        if (sc->chg_type == POWER_SUPPLY_TYPE_USB)
            val->intval = 5000000;
        break;
    case POWER_SUPPLY_PROP_TYPE:
    printk("dsy  %s %d POWER_SUPPLY_PROP_TYPE \n", __func__,__LINE__);
        val->intval = sc->chg_type;
        break;
    default:
        ret = -EINVAL;
        break;
    }
    return ret;
}

static int sc8989x_chg_set_property(struct power_supply *psy,
                enum power_supply_property psp,
                const union power_supply_propval *val)
{
    int ret = 0;
    struct sc8989x *sc = power_supply_get_drvdata(psy);

    switch (psp) {
    case POWER_SUPPLY_PROP_ONLINE:
     printk("dsy  %s %d POWER_SUPPLY_PROP_ONLINE \n", __func__,__LINE__);
        sc8989x_force_dpdm(sc);
        break;
    case POWER_SUPPLY_PROP_STATUS:
     printk("dsy  %s %d POWER_SUPPLY_PROP_STATUS \n", __func__,__LINE__);
        if (val->intval) {
            ret = sc8989x_enable_charger(sc);
        } else {
            ret = sc8989x_disable_charger(sc);
        }
        break;
    case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
     printk("dsy  %s %d POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT \n", __func__,__LINE__);
        ret = sc8989x_set_chargecurrent(sc, val->intval / 1000);
        break;
    case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
     printk("dsy  %s %d POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE \n", __func__,__LINE__);
        ret = sc8989x_set_chargevolt(sc, val->intval / 1000);
        break;
    case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
     printk("dsy  %s %d POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT \n", __func__,__LINE__);
        sc->input_curr_limit = val->intval / 1000;
        ret = sc8989x_set_input_current_limit(sc, sc->input_curr_limit);
        break;
    case POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT:
     printk("dsy  %s %d POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT \n", __func__,__LINE__);
        ret = sc8989x_set_input_volt_limit(sc, val->intval / 1000);
        break;
    case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
    printk("dsy  %s %d POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT \n", __func__,__LINE__);
        ret = sc8989x_set_term_current(sc, val->intval / 1000);
        break;
    default:
        ret = -EINVAL;
        break;
    }
    return ret;
}

static char *sc8989x_psy_supplied_to[] = {
    "battery",
    "mtk-master-charger",
};

static const struct power_supply_desc sc8989x_psy_desc = {
    .type = POWER_SUPPLY_TYPE_USB,
    .usb_types = sc8989x_chg_psy_usb_types,
    .num_usb_types = ARRAY_SIZE(sc8989x_chg_psy_usb_types),
    .properties = sc8989x_chg_psy_properties,
    .num_properties = ARRAY_SIZE(sc8989x_chg_psy_properties),
    .property_is_writeable = sc8989x_chg_property_is_writeable,
    .get_property = sc8989x_chg_get_property,
    .set_property = sc8989x_chg_set_property,
};

static int sc8989x_chg_init_psy(struct sc8989x *sc)
{
    struct power_supply_config cfg = {
        .drv_data = sc,
        .of_node = sc->dev->of_node,
        .supplied_to = sc8989x_psy_supplied_to,
        .num_supplicants = ARRAY_SIZE(sc8989x_psy_supplied_to),
    };

    memcpy(&sc->psy_desc, &sc8989x_psy_desc, sizeof(sc->psy_desc));
    sc->psy_desc.name = "primary_chg";//dev_name(sc->dev);
    sc->psy = devm_power_supply_register(sc->dev, &sc->psy_desc,
                        &cfg);
    if (IS_ERR(sc->psy)) 
    {
        pr_err("yxch Couldn't register power supply\n");
    }

    return IS_ERR(sc->psy) ? PTR_ERR(sc->psy) : 0;
}




static struct charger_ops sc8989x_chg_ops = {
    /* cable plug in/out */
    .plug_in = sc8989x_plug_in,
    .plug_out = sc8989x_plug_out,
    /* enable */
    .enable = sc8989x_charging,
    .is_enabled = sc8989x_is_charging_enable,
    /* charging current */
    .set_charging_current = sc8989x_set_ichg,
    .get_charging_current = sc8989x_get_ichg,
    .get_min_charging_current = sc8989x_get_min_ichg,
    /* charging voltage */
    .set_constant_voltage = sc8989x_set_vchg,
    .get_constant_voltage = sc8989x_get_vchg,
    /* input current limit */
    .set_input_current = sc8989x_set_icl,
    .get_input_current = sc8989x_get_icl,
    .get_min_input_current = NULL,
    /* MIVR */
    .set_mivr = sc8989x_set_ivl,
    .get_mivr = sc8989x_get_ivl,
    .get_mivr_state = NULL,
    /* ADC */
    .get_adc = NULL,
    .get_vbus_adc = sc8989x_get_vbus_adc,
    .get_ibus_adc = sc8989x_get_ibus_adc,
    .get_ibat_adc = NULL,
    .get_tchg_adc = NULL,
    .get_zcv = NULL,
    /* charing termination */
    .set_eoc_current = sc8989x_set_ieoc,
    .enable_termination = sc8989x_enable_te,
    .reset_eoc_state = NULL,
    .safety_check = NULL,
    .is_charging_done = sc8989x_is_charging_done,
    /* power path */
    .enable_powerpath = NULL,
    .is_powerpath_enabled = NULL,
    /* timer */
    .enable_safety_timer = sc8989x_set_safety_timer,
    .is_safety_timer_enabled = sc8989x_is_safety_timer_enabled,
    .kick_wdt = sc8989x_kick_wdt,
    /* AICL */
    .run_aicl = NULL,
    /* PE+/PE+20 */
    .send_ta_current_pattern = NULL,
    .set_pe20_efficiency_table = NULL,
    .send_ta20_current_pattern = NULL,
    .reset_ta = NULL,
    .enable_cable_drop_comp = NULL,
    /* OTG */
    .set_boost_current_limit = sc8989x_set_boost_ilmt,
    .enable_otg = sc8989x_set_otg,
    .enable_discharge = sc8989x_set_otg,
    /* charger type detection */
    .enable_chg_type_det = NULL,
    /* misc */
    .dump_registers = sc8989x_dump_register,
    .enable_hz = sc8989x_enable_hz,
    /* event */
    .event = NULL,
    /* 6pin battery */
    .enable_6pin_battery_charging = NULL,
};

static struct of_device_id sc8989x_charger_match_table[] = {
    {
    .compatible = "sc,sc8989x_charger",
    .data = &pn_data[PN_SC8989X],
    },
    {},
};
MODULE_DEVICE_TABLE(of, sc8989x_charger_match_table);



static int32_t sc8989x_chg_type_det(struct sc8989x *sc)
 {
     int ret;
    u8 reg_val;
    bool prev_pg;
    bool prev_vbus_pg;
    const int32_t max_wait_cnt = 200;
    int32_t i =0;

    ret = sc8989x_read_byte(sc, SC8989X_REG_0B, &reg_val);
    printk("dsy  %s %d reg_val 0x%x\n", __func__,__LINE__,reg_val);
    if (ret)
        return IRQ_HANDLED;
    prev_pg = sc->power_good;
    sc->power_good = !!(reg_val & SC8989X_PG_STAT_MASK);

    ret = sc8989x_read_byte(sc, SC8989X_REG_11, &reg_val);
     printk("dsy  %s %d reg_val 0x%x prev_pg %d\n", __func__,__LINE__,reg_val,prev_pg);
    if (ret)
        return IRQ_HANDLED;

    prev_vbus_pg = sc->vbus_good;

    sc->vbus_good = !!(reg_val & SC8989X_VBUS_GD_MASK);
printk("dsy  %s %d prev_vbus_pg %d sc->vbus_good %d\n", __func__,__LINE__,prev_vbus_pg,sc->vbus_good);
    if (!prev_vbus_pg && sc->vbus_good){
        sc->input_curr_limit = 3000;
        msleep(100);
        pr_err("adapter/usb inserted\n");
    } else if (prev_vbus_pg && !sc->vbus_good) {
        pr_err("adapter/usb removed\n");
    }

    for (i = 0; i < max_wait_cnt; i++) 
    {
        if (is_usb_rdy())
            break;
        printk("dsy CDP block\n");
        msleep(100);
    }
    if (i == max_wait_cnt)
        printk("dsy CDP timeout\n");
    else
        printk("dsy CDP free\n");


printk("dsy  %s %d prev_pg %d sc->power_good %d\n", __func__,__LINE__,prev_pg,sc->power_good);
    if (!prev_pg && sc->power_good){
        printk("dsy  %s %d prev_pg %d sc->power_good %d\n", __func__,__LINE__,prev_pg,sc->power_good);
        sc8989x_get_charger_type(sc);
    }
    
    sc8989x_dump_regs(sc);

    power_supply_changed(sc->psy);
    return 0;
}
static int32_t sc8989x_chg_set_usbsw(struct sc8989x *sc,enum sc8989x_usbsw usbsw)
{
    struct phy *phy = NULL;
    int32_t ret, mode = (usbsw == USBSW_CHG) ? PHY_MODE_BC11_SET : PHY_MODE_BC11_CLR;

    pr_info("usbsw = %d\n", usbsw);
    phy = phy_get(sc->dev, "usb2-phy");
    if (IS_ERR_OR_NULL(phy)) {
        dev_err(sc->dev, "failed to get usb2-phy\n");
        return -ENODEV;
    }
    ret = phy_set_mode_ext(phy, PHY_MODE_USB_DEVICE, mode);
    if (ret)
        dev_err(sc->dev, "failed to set phy ext mode\n");
    phy_put(sc->dev, phy);
    return ret;
}


static int32_t sc8989x_bc12_en_kthread(void *data)
{
    int32_t ret = 0, i = 0;
    struct sc8989x *sc = data;
    const int32_t max_wait_cnt = 200;

wait:
    wait_event(sc->bc12_en_req, atomic_read(&sc->bc12_en_req_cnt) > 0 || kthread_should_stop());

    if (atomic_read(&sc->bc12_en_req_cnt) <= 0 && kthread_should_stop()) 
    {
        pr_err( "sc8989x bye bye\n");
        return 0;
    }
    atomic_dec(&sc->bc12_en_req_cnt);
    
    pm_stay_awake(sc->dev);

    //if(atomic_read(&chip->vbus_gd))
    //{
        /* Workaround for CDP port */
        for (i = 0; i < max_wait_cnt; i++) 
        {
            if (is_usb_rdy())
                break;
            //pr_err("hl7015 CDP block\n");
            //if (!atomic_read(&chip->vbus_gd)) 
            //{
            //  pr_err("plug out\n");
            //  break;
            //}
            //msleep(100);
        }
        if (i == max_wait_cnt)
            pr_err("sc8989x CDP timeout\n");
        else
            pr_err("sc8989x CDP free\n");    
    //}

    ret = sc8989x_chg_set_usbsw(sc,USBSW_USB);
    if(ret)
        return ret;
    sc8989x_chg_type_det(sc);
    //ret = sd7601_chg_type_det(chip,atomic_read(&chip->vbus_gd));
    //if (ret < 0)
    //  pr_err("fail(%d)\n",ret);
    //sd7601_bc12_postprocess(chip);

    pm_relax(sc->dev);
    goto wait;

    return 0;
}


//add for otg regulator interface start

static int sc8989x_enable_vbus(struct regulator_dev *rdev)
{
    int ret =0;
     struct sc8989x *sc = rdev->reg_data; 


    //ret = sc8989x_disable_charger(sc);
    ret = sc8989x_enable_otg(sc);

    printk("sc8989x enable OTG,ret=%d\n",ret);

    return 0;
}

static int sc8989x_disable_vbus(struct regulator_dev *rdev)
{
    int ret =0;
     struct sc8989x *sc = rdev->reg_data; 

    //ret = sc8989x_set_otg_en(chip,0);

    ret = sc8989x_disable_otg(sc);
   // ret = sc8989x_enable_charger(sc);


    printk("sc8989x disable OTG,ret=%d\n",ret);

    return 0;
}

static int sc8989x_is_enabled_vbus(struct regulator_dev *rdev)
{
    int status;
    struct sc8989x *sc = rdev->reg_data; 

    status = sc8989x_get_otg_config(sc);
    printk("---%s--%d-\n",__func__,status);
    return status;
}

/*
static int sc8989x_list_voltage_linear(struct regulator_dev *rdev,unsigned int selector)
{
    return 1;
}

static int sc8989x_set_voltage_sel(struct regulator_dev *rdev, unsigned sel)
{
    return 1;
}

static int sc8989x_get_voltage_sel(struct regulator_dev *rdev)
{
    return 5000000;
}
*/

static int sc8989x_set_otg_current_limit(struct regulator_dev *rdev,int min_uA, int max_uA)
{
    int ret =0;
    int set_cur = 500;
    struct sc8989x *sc = rdev->reg_data; 
    
    if (min_uA!=max_uA)
        printk("sc8989x set otg current not right(%d)!=(%d)\n",min_uA,max_uA);
    
    set_cur = min_uA/1000;
  //  ret = sc8989x_set_boost_ilmt(sc,set_cur);
    ret =  sc8989x_set_boost_current(sc, set_cur);
    printk("sc8989x_set_otg_current_limit:%d  ret:%d\n",set_cur,ret);
    return ret;
}

static int sc8989x_get_otg_current_limit(struct regulator_dev *rdev)
{
    int ret =0;
    struct sc8989x *sc  = rdev->reg_data; 
    ret = 500;
   // ret = sd76030_get_boost_ilim(chip)*1000;
    dev_err(sc->dev, "sc8989x_get_otg_current_limit:%d \n",ret); 
    return ret;
}

static const struct regulator_ops sc8989x_vbus_ops = {
    .enable = sc8989x_enable_vbus,
    .disable = sc8989x_disable_vbus,
    .is_enabled = sc8989x_is_enabled_vbus,
    //.list_voltage = sd76030_list_voltage_linear,
    //.set_voltage_sel = sd76030_set_voltage_sel,
    //.get_voltage_sel = sd76030_get_voltage_sel,
    .set_current_limit = sc8989x_set_otg_current_limit,
    .get_current_limit = sc8989x_get_otg_current_limit,
};

static const struct regulator_desc sc8989x_otg_rdesc = {
    .of_match = "usb-otg-vbus",
    .name = "usb-otg-vbus",
    .ops = &sc8989x_vbus_ops,
    .owner = THIS_MODULE,
    .type = REGULATOR_VOLTAGE,
    .fixed_uV = 5150000,
    //.min_uV = 4500000,
    //.uV_step = 0, 
    .n_voltages = 1,

};

static const struct regulator_init_data sc8989x_vbus_init_data = {
        .constraints = {
        .valid_ops_mask = REGULATOR_CHANGE_STATUS|REGULATOR_CHANGE_CURRENT,//|REGULATOR_CHANGE_VOLTAGE,
        .min_uA = 500000,
        .max_uA = 1200000,
        //.min_uV = 4500000,
        //.max_uV = 5000000,
        },
};

static int sc8989x_vbus_regulator_register(struct sc8989x *sc)
{
    struct regulator_config config = {};
    int ret = 0;
    /* otg regulator */
    config.dev = sc->dev;
    config.driver_data = sc;
    config.init_data = &sc8989x_vbus_init_data;
    sc->otg_rdev =
        devm_regulator_register(sc->dev, &sc8989x_otg_rdesc, &config);
    //sc->otg_rdev->constraints->valid_ops_mask = REGULATOR_CHANGE_STATUS|REGULATOR_CHANGE_CURRENT;//|= REGULATOR_CHANGE_STATUS;
    if (IS_ERR(sc->otg_rdev)) {
        ret = PTR_ERR(sc->otg_rdev);
        printk("%s: register otg regulator failed (%d)-\n", __func__,ret);
    }
    return ret;
}

//add for otg regulator interface end



static int sc8989x_charger_probe(struct i2c_client *client)
{
    struct sc8989x *sc;
    const struct of_device_id *match;
    struct device_node *node = client->dev.of_node;

    int ret = 0;

    sc = devm_kzalloc(&client->dev, sizeof(struct sc8989x), GFP_KERNEL);
    if (!sc)
        return -ENOMEM;

    sc->dev = &client->dev;
    sc->client = client;

    i2c_set_clientdata(client, sc);
    st_sc = sc;
    mutex_init(&sc->i2c_rw_lock);

    ret = sc8989x_detect_device(sc);
    if (ret) {
        pr_err("No sc8989x device found!\n");
        return -ENODEV;
    }

    match = of_match_node(sc8989x_charger_match_table, node);
    if (match == NULL) {
        pr_err("device tree match not found\n");
        return -EINVAL;
    }

    sc->platform_data = sc8989x_parse_dt(node, sc);

    if (!sc->platform_data) {
        pr_err("No platform data provided.\n");
        return -EINVAL;
    }

    ret = sc8989x_chg_init_psy(sc);
    if (ret < 0) {
        dev_err(sc->dev, "failed to init power supply\n");
        return -EINVAL;
    }

    ret = sc8989x_init_device(sc);
    if (ret) {
        pr_err("Failed to init device\n");
        return ret;
    }

    sc8989x_register_interrupt(sc);

    sc->chg_dev = charger_device_register("primary_chg",
                        &client->dev, sc,
                        &sc8989x_chg_ops,
                        &sc8989x_chg_props);
    if (IS_ERR_OR_NULL(sc->chg_dev)) {
        ret = PTR_ERR(sc->chg_dev);
        return ret;
    }

    ret = sysfs_create_group(&sc->dev->kobj, &sc8989x_attr_group);
    if (ret)
        dev_err(sc->dev, "failed to register sysfs. err: %d\n", ret);

    atomic_set(&sc->bc12_en_req_cnt, 0);
    init_waitqueue_head(&sc->bc12_en_req);
    sc->bc12_en_kthread = kthread_run(sc8989x_bc12_en_kthread,sc,"sc8989x_bc12");
    if (IS_ERR_OR_NULL(sc->bc12_en_kthread)) 
    {
        ret = PTR_ERR(sc->bc12_en_kthread);
        pr_err("sc8989x kthread run fail(%d)\n", ret);
    }


    determine_initial_status(sc);


    sc8989x_vbus_regulator_register(sc);

    pr_err("sc8989x probe successfully, Part Num:%d, Revision:%d\n!",
        sc->part_no, sc->revision);

    return 0;
}

static void sc8989x_charger_remove(struct i2c_client *client)
{
    struct sc8989x *sc = i2c_get_clientdata(client);

    sysfs_remove_group(&sc->dev->kobj, &sc8989x_attr_group);

}

static void sc8989x_charger_shutdown(struct i2c_client *client)
{

}

static struct i2c_driver sc8989x_charger_driver = {
    .driver = {
        .name = "sc8989x-charger",
        .owner = THIS_MODULE,
        .of_match_table = sc8989x_charger_match_table,
    },

    .probe = sc8989x_charger_probe,
    .remove = sc8989x_charger_remove,
    .shutdown = sc8989x_charger_shutdown,

};

module_i2c_driver(sc8989x_charger_driver);

MODULE_DESCRIPTION("SC SC8989X Charger Driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("South Chip");

