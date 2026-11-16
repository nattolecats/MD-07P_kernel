/* SPDX-License-Identifier: GPL-2.0-only */
// cx25890 Charger Driver
// Copyright (C) 2021 Texas Instruments Incorporated - http://www.sg-micro.com

#ifndef _CX25890_CHARGER_H
#define _CX25890_CHARGER_H

#include <linux/i2c.h>
#define CX25890_MANUFACTURER   "SUNCORE"

#define __SGM41542_CHIP_ID__

#define CX25890_NAME       "cx25890h"
#define CX25890_PN_ID      3

/*define register*/
#define CX25890_CHRG_CTRL_0     0x00
#define CX25890_CHRG_CTRL_1     0x01
#define CX25890_CHRG_CTRL_2     0x02
#define CX25890_CHRG_CTRL_3     0x03
#define CX25890_CHRG_CTRL_4     0x04
#define CX25890_CHRG_CTRL_5     0x05
#define CX25890_CHRG_CTRL_6     0x06
#define CX25890_CHRG_CTRL_7     0x07
#define CX25890_CHRG_CTRL_8     0x08
#define CX25890_CHRG_CTRL_9     0x09
#define CX25890_CHRG_CTRL_a     0x0a
#define CX25890_CHRG_CTRL_b     0x0b
#define CX25890_CHRG_CTRL_c     0x0c
#define CX25890_CHRG_CTRL_d     0x0d
#define CX25890_CHRG_CTRL_e     0x0e
#define CX25890_CHRG_CTRL_f     0x0f
#define CX25890_CHRG_CTRL_10    0x10
#define CX25890_CHRG_CTRL_11    0x11
#define CX25890_CHRG_CTRL_12    0x12
#define CX25890_CHRG_CTRL_13    0x13
#define CX25890_CHRG_CTRL_14    0x14
#define CX25890_CHRG_CTRL_15    0x15

/* charge status flags  */
#define CX25890_CHRG_EN        BIT(4)
#define CX25890_HIZ_EN         BIT(7)
#define CX25890_TERM_EN        BIT(7)
#define CX25890_VAC_OVP_MASK   GENMASK(7, 6)
#define CX25890_DPDM_ONGOING   BIT(7)
#define CX25890_VBUS_GOOD      BIT(7)

#define CX25890_BOOSTV         GENMASK(5, 4)
#define CX25890_BOOST_LIM      BIT(7)
#define CX25890_OTG_MASK       BIT(5)
#define CX25890_OTG_SHIFT      5
#define CX25890_OTG_DISABLE     0
#define CX25890_OTG_ENABLE     1

/* Part ID  */
#define CX25890_PN_MASK        GENMASK(5, 3)



/* SAFETY TIMER SET  */
#define CX25890_SAFETY_TIMER_MASK     GENMASK(3, 3)
#define CX25890_SAFETY_TIMER_DISABLE     0
#define CX25890_SAFETY_TIMER_EN       BIT(3)
#define CX25890_SAFETY_TIMER_5H         0
#define CX25890_SAFETY_TIMER_10H      BIT(2)

/* recharge voltage  */
#define CX25890_VRECHARGE              BIT(0)
#define CX25890_VRECHRG_STEP_mV        100
#define CX25890_VRECHRG_OFFSET_mV      100

/* charge status  */
#define CX25890_VSYS_STAT      BIT(0)
#define CX25890_THERM_STAT     BIT(1)
#define CX25890_PG_STAT        BIT(2)
#define CX25890_CHG_STAT_MASK  GENMASK(4, 3)
#define CX25890_PRECHRG        BIT(3)
#define CX25890_FAST_CHRG      BIT(4)
#define CX25890_TERM_CHRG      (BIT(3)| BIT(4))

/* charge type  */
#define CX25890_VBUS_STAT_MASK GENMASK(7, 5)
#define CX25890_NOT_CHRGING    0
#define CX25890_USB_SDP        BIT(5)
#define CX25890_USB_CDP        BIT(6)
#define CX25890_USB_DCP        (BIT(5) | BIT(6))
#define CX25890_UNKNOWN        (BIT(7) | BIT(5))
#define CX25890_NON_STANDARD   (BIT(7) | BIT(6))
#define CX25890_OTG_MODE       (BIT(7) | BIT(6) | BIT(5))

/* TEMP Status  */
#define CX25890_TEMP_MASK      GENMASK(2, 0)
#define CX25890_TEMP_NORMAL    BIT(0)
#define CX25890_TEMP_WARM      BIT(1)
#define CX25890_TEMP_COOL      (BIT(0) | BIT(1))
#define CX25890_TEMP_COLD      (BIT(0) | BIT(3))
#define CX25890_TEMP_HOT       (BIT(2) | BIT(3))

/* termination current  */
#define CX25890_TERMCHRG_CUR_MASK      GENMASK(3, 0)
#define CX25890_TERMCHRG_I_DEF_uA      180000
/* precharge current  */
#define CX25890_PRECHRG_CUR_MASK       GENMASK(7, 4)
#define CX25890_PRECHRG_I_DEF_uA       180000

/* charge current  */
#define CX25890_ICHRG_I_MASK       GENMASK(6, 0)
#define CX25890_ICHRG_I_MIN_uA         0
#define CX25890_ICHRG_I_STEP_uA        64000
#define CX25890_ICHRG_I_MAX_uA         5056000
#define CX25890_ICHRG_I_DEF_uA         3072000

/* charge voltage  */
#define CX25890_VREG_V_MASK        GENMASK(7, 2)
#define CX25890_VREG_V_MAX_uV      4608000
#define CX25890_VREG_V_MIN_uV      3840000
#define CX25890_VREG_V_DEF_uV      4208000
#define CX25890_VREG_V_STEP_uV     16000

/* iindpm current  */
#define CX25890_IINDPM_I_MASK      GENMASK(5, 0)
#define CX25890_IINDPM_I_MIN_uA    100000
#define CX25890_IINDPM_I_MAX_uA    3250000
#define CX25890_IINDPM_STEP_uA     50000
#define CX25890_IINDPM_DEF_uA      2000000

/* vindpm voltage  */
/* Register 0x0D*/
#define REG0D_FORCE_VINDPM_MASK			0x80

#define CX25890_VINDPM_V_MASK      GENMASK(6, 0)
#define CX25890_VINDPM_V_MIN_uV    3900000
#define CX25890_VINDPM_V_MAX_uV    15300000
#define CX25890_VINDPM_STEP_uV     100000
#define CX25890_VINDPM_DEF_uV      4600000

/* Register 0x0D*/
#define REG0D_FORCE_VINDPM_MASK			0x80
#define REG0D_FORCE_VINDPM_SHIFT		7
#define REG0D_VINDPM_MASK			0x7f
#define REG0D_VINDPM_SHIFT		


/* DP DM SEL  */
#define CX25890_DP_VSEL_MASK       GENMASK(4, 3)
#define CX25890_DM_VSEL_MASK       GENMASK(2, 1)

/* PUMPX SET  */
#define CX25890_EN_PUMPX           BIT(7)
#define CX25890_PUMPX_UP           BIT(6)
#define CX25890_PUMPX_DN           BIT(5)
#if 0
/* customer define jeita paramter */
#define JEITA_TEMP_ABOVE_T4_CV  0
#define JEITA_TEMP_T3_TO_T4_CV  4100000
#define JEITA_TEMP_T2_TO_T3_CV  4350000
#define JEITA_TEMP_T1_TO_T2_CV  4350000
#define JEITA_TEMP_T0_TO_T1_CV  0
#define JEITA_TEMP_BELOW_T0_CV  0

#define JEITA_TEMP_ABOVE_T4_CC_CURRENT  0
#define JEITA_TEMP_T3_TO_T4_CC_CURRENT  1000000
#define JEITA_TEMP_T2_TO_T3_CC_CURRENT  2400000
#define JEITA_TEMP_T1_TO_T2_CC_CURRENT  2000000
#define JEITA_TEMP_T0_TO_T1_CC_CURRENT  0
#define JEITA_TEMP_BELOW_T0_CC_CURRENT  0

#define TEMP_T4_THRES  50
#define TEMP_T4_THRES_MINUS_X_DEGREE 47
#define TEMP_T3_THRES  45
#define TEMP_T3_THRES_MINUS_X_DEGREE 39
#define TEMP_T2_THRES  20
#define TEMP_T2_THRES_PLUS_X_DEGREE 16
#define TEMP_T1_THRES  0
#define TEMP_T1_THRES_PLUS_X_DEGREE 6
#define TEMP_T0_THRES  0
#define TEMP_T0_THRES_PLUS_X_DEGREE  0
#define TEMP_NEG_10_THRES 0
#endif
struct cx25890_init_data
{
    u32 ichg;   /* charge current       */
    u32 ilim;   /* input current        */
    u32 vreg;   /* regulation voltage       */
    u32 iterm;  /* termination current      */
    u32 iprechg;    /* precharge current        */
    u32 vlim;   /* minimum system voltage limit */
    u32 max_ichg;
    u32 max_vreg;
};

struct cx25890_state
{
    bool vsys_stat;
    bool therm_stat;
    bool online;
    u8 chrg_stat;
    u8 vbus_status;
    u8 pg_stat;

    bool chrg_en;
    bool hiz_en;
    bool term_en;
    bool vbus_gd;
    u8 chrg_type;
    u8 health;
    u8 chrg_fault;
    u8 ntc_fault;
};

struct cx25890_jeita
{
    int jeita_temp_above_t4_cv;
    int jeita_temp_t3_to_t4_cv;
    int jeita_temp_t2_to_t3_cv;
    int jeita_temp_t1_to_t2_cv;
    int jeita_temp_t0_to_t1_cv;
    int jeita_temp_below_t0_cv;
    int jeita_temp_above_t4_cc_current;
    int jeita_temp_t3_to_t4_cc_current;
    int jeita_temp_t2_to_t3_cc_current;
    int jeita_temp_t1_to_t2_cc_current;
    int jeita_temp_below_t0_cc_current;
    int temp_t4_thres;
    int temp_t4_thres_minus_x_degree;
    int temp_t3_thres;
    int temp_t3_thres_minus_x_degree;
    int temp_t2_thres;
    int temp_t2_thres_plus_x_degree;
    int temp_t1_thres;
    int temp_t1_thres_plus_x_degree;
    int temp_t0_thres;
    int temp_t0_thres_plus_x_degree;
    int temp_neg_10_thres;
};

struct cx25890_device
{

    int chg_type;
	int psy_usb_type;
    bool chg_config;
    struct i2c_client *client;
    struct device *dev;
    struct power_supply *charger;
    struct power_supply *usb;
    struct power_supply *ac;
    struct power_supply *battery_psy;
    struct power_supply_desc psy_desc;
    struct mutex lock;
    struct mutex i2c_rw_lock;

    struct usb_phy *usb2_phy;
    struct usb_phy *usb3_phy;
    struct notifier_block usb_nb;
    struct work_struct usb_work;
    unsigned long usb_event;
    struct regmap *regmap;

    char model_name[I2C_NAME_SIZE];
    int device_id;

    struct cx25890_init_data init_data;
    struct cx25890_state state;
    u32 watchdog_timer;
#if 1//defined(CONFIG_MTK_GAUGE_VERSION) && (CONFIG_MTK_GAUGE_VERSION == 30)
    struct charger_device *chg_dev;
#endif
    struct regulator_dev *otg_rdev;

    struct delayed_work charge_detect_delayed_work;
    struct delayed_work charge_monitor_work;
    struct delayed_work charge_done_detect_work;
    struct delayed_work recharge_detect_work;
    struct delayed_work charge_usb_detect_work;
    struct delayed_work irq_work;
	struct delayed_work vindpm_work;
    struct notifier_block pm_nb;
    bool cx25890_suspend_flag;
	bool vbus_gd_irq;

    struct wakeup_source *charger_wakelock;
    bool enable_sw_jeita;
    struct cx25890_jeita data;
};

#endif /* _CX25890_CHARGER_H */
