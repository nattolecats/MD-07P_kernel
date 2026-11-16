/*
 * Fuelgauge battery driver
 *
 * Copyright (C) 2021 SinoWealth
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 */

#define pr_fmt(fmt) "[sh366100] %s(%d): " fmt, __func__, __LINE__

#include <asm/unaligned.h>
#include <linux/acpi.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/idr.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/ktime.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/param.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/random.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>
//#include <stdbool.h>
#include "sh366100_fg.h"
#include <linux/printk.h>

// #undef FG_DEBUG
// #define FG_DEBUG 1

// #undef pr_debug
// #undef pr_info
// #if FG_DEBUG
// #define pr_debug pr_err
// #define pr_info pr_err
// #endif

enum sh_fg_reg_idx {
	SH_FG_REG_DEVICE_ID = 0,
	SH_FG_REG_CNTL,
	SH_FG_REG_INT,
	SH_FG_REG_STATUS,
	SH_FG_REG_SOC,
	SH_FG_REG_OCV,
	SH_FG_REG_VOLTAGE,
	SH_FG_REG_CURRENT,
	SH_FG_REG_TEMPERATURE_IN,
	SH_FG_REG_TEMPERATURE_EX,
	/* 20211029, Ethan */
/* 	SH_FG_REG_SHUTDOWN_EN, */
/*	SH_FG_REG_SHUTDOWN, */
	SH_FG_REG_BAT_RMC,
	SH_FG_REG_BAT_FCC,
	SH_FG_REG_RESET,
	SH_FG_REG_SOC_CYCLE,
	SH_FG_REG_DESIGN_CAPCITY, /* 20211116, Ethan */
	NUM_REGS,
};

static u32 sh366100_regs[NUM_REGS] = {
    CMDMASK_ALTMAC_R | 0x0001, /* DEVICE_ID */
    CMDMASK_ALTMAC_R | 0x0000, /* CNTL */
    CMDMASK_SINGLE | 0x6E,     /* INT */
    0x0A,		       /* STATUS */
    0x2C,		       /* SOC */
    0x64,		       /* OCV */
    0x08,		       /* VOLTAGE */
    0x0C,		       /* CURRENT */
    0x28,		       /* TEMPERATURE_IN */
    0x06,		       /* TEMPERATURE_EX */
	/* 20211029, Ethan */
/*    CMDMASK_ALTMAC_W | 0x1B,    SHUTDOWN_EN */
/*    CMDMASK_ALTMAC_W | 0x1C,    SHUTDOWN */
    0x10,		       /* BAT_RMC */
    0x12,		       /* BAT_FCC */
    CMDMASK_ALTMAC_W | 0x41,   /* RESET */
    0x1A,		       /* SOC_CYCLE */
    0x3C,		       /* SH_FG_REG_DESIGN_CAPCITY. 20211116, Ethan */
};

const unsigned char* device2str[] = {
    "sh366100",
};

struct sh_fg_chip;

struct sh_fg_chip {
	struct device* dev;
	struct i2c_client* client;
	struct mutex i2c_rw_lock; /* I2C Read/Write Lock */
	struct mutex data_lock;	  /* Data Lock */
	struct mutex cali_lock;	  /* Cali Lock. 20220807, Sinowealth */
	u8 chip;
	u32 regs[NUM_REGS];

	struct delayed_work monitor_work; //20220727, Sinowealth
	/* Status Tracking */
	int batt_soc;
	int batt_soc_h;
	int batt_volt;
	int batt_curr;
	u64 log_lastUpdate; /* 20211025, Ethan */
	int designCapacity;
	int	cyclecount;
	int  FullChargingCapacity;
	u16 controlStatus;

	int terminate_voltage;			     /* 20211112, Ethan. Termniate Voltage */
};


static	int Unseal = 0x80008000;
static	int Fullaccess = 0xFFFFFFFF;

static int st_batt_volt = 0;

static int st_batt_curr = 0;

int ST_CHARGE_EN_GPIO = 0;

int SUNRITEL_CHARGER_UISOC = 1;

int st_charge_temp = 0;

int st_charge_stop_en_state = 0;

int charge_plug_in_status = 0;

int charge_mode_status;

int st_designCapacity = 0;
int	st_cyclecount = 0;
int st_FullChargingCapacity = 0;
int st_batt_soc_h = 0;
u8 st_Day_Read = 0;
u8 st_Month_Read = 0;
u16 st_Year_Read = 0;
u8 st_Day_Write = 23;
u8 st_Month_Write = 10;
u16 st_Year_Write = 2022;
u8 st_FirstUse_Day_Read = 0;
u8 st_FirstUse_Month_Read = 0;
u16 st_FirstUse_Year_Read = 0;
u8 st_FirstUse_Day_Write = 23;
u8 st_FirstUse_Month_Write = 10;
u16 st_FirstUse_Year_Write = 2024;
u8 st_SNCode_Write[17] = {'U','T','L','2','0','2','4','0','8','1','3','0','1','1','5','7','1'};
u8 st_SNCode_Read[33] = {0};
int st_batt_SH366100_WriteFirstTimeToSeriaNo_status = 0;

EXPORT_SYMBOL_GPL(ST_CHARGE_EN_GPIO);
EXPORT_SYMBOL_GPL(SUNRITEL_CHARGER_UISOC);
EXPORT_SYMBOL_GPL(st_charge_temp);
EXPORT_SYMBOL_GPL(st_charge_stop_en_state);
EXPORT_SYMBOL_GPL(charge_plug_in_status);
EXPORT_SYMBOL_GPL(charge_mode_status);
EXPORT_SYMBOL_GPL(st_designCapacity);
EXPORT_SYMBOL_GPL(st_cyclecount);
EXPORT_SYMBOL_GPL(st_FullChargingCapacity);
EXPORT_SYMBOL_GPL(st_batt_soc_h);
EXPORT_SYMBOL_GPL(st_Day_Read);
EXPORT_SYMBOL_GPL(st_Month_Read);
EXPORT_SYMBOL_GPL(st_Year_Read);
EXPORT_SYMBOL_GPL(st_Day_Write);
EXPORT_SYMBOL_GPL(st_Month_Write);
EXPORT_SYMBOL_GPL(st_Year_Write);
EXPORT_SYMBOL_GPL(st_FirstUse_Day_Read);
EXPORT_SYMBOL_GPL(st_FirstUse_Month_Read);
EXPORT_SYMBOL_GPL(st_FirstUse_Year_Read);
EXPORT_SYMBOL_GPL(st_FirstUse_Day_Write);
EXPORT_SYMBOL_GPL(st_FirstUse_Month_Write);
EXPORT_SYMBOL_GPL(st_FirstUse_Year_Write);
EXPORT_SYMBOL_GPL(st_SNCode_Write);
EXPORT_SYMBOL_GPL(st_SNCode_Read);
EXPORT_SYMBOL_GPL(st_batt_SH366100_WriteFirstTimeToSeriaNo_status);


static int SH366100_WriteFirstTimeToSeriaNo(struct sh_fg_chip* sm);
//static int SH366100_WriteManufactureData(struct sh_fg_chip* sm);
//static int SH366100_Write_SNCode_To_BlockA(struct sh_fg_chip* sm);
static int fg_SH366100_Unseal(struct sh_fg_chip* sm);
static struct sh_fg_chip *st_sm; 
static int fg_read_soc(struct sh_fg_chip* sm);

static int __fg_read_word(struct i2c_client* client, u8 reg, u16* val)
{
	int ret;

	ret = i2c_smbus_read_word_data(client, reg); /* little endian */
	if (ret < 0) {
		pr_err("i2c read word fail: can't read from reg 0x%02X\n", reg);
		return ret;
	}
	*val = (u16)ret;

	return 0;
}

static int __fg_write_word(struct i2c_client* client, u8 reg, u16 val)
{
	int ret;

	ret = i2c_smbus_write_word_data(client, reg, val); /* little endian */
	if (ret < 0) {
		pr_err("i2c write word fail: can't write 0x%02X to reg 0x%02X\n", val, reg);
		return ret;
	}

	return 0;
}

static int fg_read_sbs_word(struct sh_fg_chip* sm, u32 reg, u16* val)
{
	int ret = -1;

	pr_info("fg_read_sbs_word start, reg=%08X\n", reg);
	/* 20211029, Ethan. */
	/* 
	if (sm->skip_reads) {
		*val = 0;
		return 0;
	} 
	*/

	mutex_lock(&sm->i2c_rw_lock);
	if ((reg & CMDMASK_ALTMAC_R) == CMDMASK_ALTMAC_R) { /* 20211116, Ethan */
		ret = __fg_write_word(sm->client, CMD_ALTMAC, (u16)reg);
		if (ret < 0)
			goto fg_read_sbs_word_end;

		HOST_DELAY(CMD_SBS_DELAY); /* 20211029, Ethan */
		ret = __fg_read_word(sm->client, CMD_ALTBLOCK, val);
	} else {
		ret = __fg_read_word(sm->client, (u8)reg, val);
	}
fg_read_sbs_word_end:
	mutex_unlock(&sm->i2c_rw_lock);

	return ret;
}

#if 1
static int fg_write_sbs_word(struct sh_fg_chip* sm, u32 reg, u16 val)
{
	int ret;

	/* 20211029, Ethan. */
	/* 
	if (sm->skip_writes)
		return 0; 
	*/

	mutex_lock(&sm->i2c_rw_lock);
	ret = __fg_write_word(sm->client, (u8)reg, val);
	mutex_unlock(&sm->i2c_rw_lock);

	return ret;
}
#endif

/* return -1: error; else return string valid length */
static int print_buffer(char* str, int strlen, u8* buf, int buflen)
{
#define PRINT_BUFFER_FORMAT_LEN 3
	int i, j;

	if ((strlen <= 0) || (buflen <= 0))
		return -1;

	memset(str, 0, strlen * sizeof(char));

	j = min(buflen, strlen / PRINT_BUFFER_FORMAT_LEN);
	for (i = 0; i < j; i++) {
		sprintf(&str[i * PRINT_BUFFER_FORMAT_LEN], "%02X ", buf[i]);
	}

	return i * PRINT_BUFFER_FORMAT_LEN;
}

static int __fg_read_buffer(struct i2c_client* client, u8 reg, u8 length, u8* val)
{
	static struct i2c_msg msg[2];

	if (!client->adapter)
		return -ENODEV;

	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].buf = &reg;
	msg[0].len = sizeof(u8);
	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].buf = val;
	msg[1].len = length;

	return (int)i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg));
}

static int fg_read_block(struct sh_fg_chip* sm, u32 reg, u8 length, u8* val)
{
	int ret = -1;
	int i;
	u8 sum;
	u16 checksum;

	/* 20211029, Ethan. */
	/* 
	if (sm->skip_reads) {
		*val = 0;
		return 0;
	} 
	*/

	mutex_lock(&sm->i2c_rw_lock);
	if ((reg & CMDMASK_ALTMAC_R) == CMDMASK_ALTMAC_R) { /* 20211116, Ethan */
		ret = __fg_write_word(sm->client, CMD_ALTMAC, (u16)reg);
		if (ret < 0)
			goto fg_read_block_end;
		msleep(CMD_SBS_DELAY);

		if (length > 32)
			length = 32;

		ret = __fg_read_buffer(sm->client, CMD_ALTBLOCK, length, val);
		if (ret < 0)
			goto fg_read_block_end;
		msleep(CMD_SBS_DELAY);

		/* check buffer */
		ret = __fg_read_word(sm->client, CMD_ALTCHK, &checksum);
		if (ret < 0)
			goto fg_read_block_end;

		i = (checksum >> 8) - 4;
		if (i <= 0)
			goto fg_read_block_end;

		sum = (u8)(reg & 0xFF) + (u8)((reg >> 8) & 0xFF);
		while (i--)
			sum += val[i];
		sum = ~sum;
		if (sum != (u8)checksum)
			ret = -1;
		else
			ret = 0;
	} else {
		ret = __fg_read_buffer(sm->client, reg, length, val);
	}

fg_read_block_end:
	mutex_unlock(&sm->i2c_rw_lock);

	return ret;
}

static int __fg_write_buffer(struct i2c_client* client, u8 reg, u8 length, u8* val)
{
	static struct i2c_msg msg[1];
	static u8 write_buf[WRITE_BUF_MAX_LEN];
	int ret;

	if (!client->adapter)
		return -ENODEV;

	if ((length <= 0) || (length + 1 >= WRITE_BUF_MAX_LEN)) {
		pr_err("i2c write buffer fail: length invalid!");
		return -1;
	}

	memset(write_buf, 0, WRITE_BUF_MAX_LEN * sizeof(u8));
	write_buf[0] = reg;
	memcpy(&write_buf[1], val, length);

	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].buf = write_buf;
	msg[0].len = sizeof(u8) * (length + 1);

	ret = i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg));
	if (ret < 0) {
		pr_err("i2c write buffer fail: can't write reg 0x%02X\n", reg);
		return (int)ret;
	}

	return 0;
}

/* 20211116, Ethan */
static int fg_write_block(struct sh_fg_chip* sm, u32 reg, u8 length, u8* val)
{
	int ret;
	int i;
	u8 sum;
	u16 checksum;

	if (length > 32)
		length = 32;

	mutex_lock(&sm->i2c_rw_lock);
	if ((reg & CMDMASK_ALTMAC_W) == CMDMASK_ALTMAC_W) {
		ret = __fg_write_word(sm->client, CMD_ALTMAC, (u16)reg);
		if (ret < 0)
			goto fg_write_block_end;
		msleep(CMD_SBS_DELAY);

		ret = __fg_write_buffer(sm->client, CMD_ALTBLOCK, length, val);
		if (ret < 0)
			goto fg_write_block_end;
		msleep(CMD_SBS_DELAY);

		sum = (u8)reg + (u8)(reg >> 8);
		for (i = 0; i < length; i++)
			sum += val[i];
		sum = ~sum; /* 20220104, Ethan */
		checksum = length + 4;
		checksum = (checksum << 8) | sum;

		ret = __fg_write_word(sm->client, CMD_ALTCHK, checksum);
		if (ret < 0)
			goto fg_write_block_end;
	} else {
		ret = __fg_write_buffer(sm->client, (u8)reg, length, val);
	}
fg_write_block_end:
	mutex_unlock(&sm->i2c_rw_lock);

	return ret;
}

#if 1 /* 20211026, Ethan. FileDecode Struct */
struct sh_decoder;

struct sh_decoder {
	u8 addr;
	u8 reg;
	u8 length;
	u8 buf_first_val;
};

static int fg_decode_iic_read(struct sh_fg_chip* sm, struct sh_decoder* decoder, u8* pBuf)
{
	static struct i2c_msg msg[2];
	u8 addr = IIC_ADDR_OF_2_KERNEL(decoder->addr);
	int ret;

	if (!sm->client->adapter)
		return -ENODEV;

	mutex_lock(&sm->i2c_rw_lock);

	msg[0].addr = addr;
	msg[0].flags = 0;
	msg[0].buf = &(decoder->reg);
	msg[0].len = sizeof(u8);
	msg[1].addr = addr;
	msg[1].flags = I2C_M_RD;
	msg[1].buf = pBuf;
	msg[1].len = decoder->length;
	ret = (int)i2c_transfer(sm->client->adapter, msg, ARRAY_SIZE(msg));

	mutex_unlock(&sm->i2c_rw_lock);
	return ret;
}

static int fg_decode_iic_write(struct sh_fg_chip* sm, struct sh_decoder* decoder)
{
	static struct i2c_msg msg[1];
	static u8 write_buf[WRITE_BUF_MAX_LEN];
	u8 addr = IIC_ADDR_OF_2_KERNEL(decoder->addr);
	u8 length = decoder->length;
	int ret;

	if (!sm->client->adapter)
		return -ENODEV;

	if ((length <= 0) || (length + 1 >= WRITE_BUF_MAX_LEN)) {
		pr_err("i2c write buffer fail: length invalid!");
		return -1;
	}

	mutex_lock(&sm->i2c_rw_lock);
	memset(write_buf, 0, WRITE_BUF_MAX_LEN * sizeof(u8));
	write_buf[0] = decoder->reg;
	memcpy(&write_buf[1], &(decoder->buf_first_val), length);

	msg[0].addr = addr;
	msg[0].flags = 0;
	msg[0].buf = write_buf;
	msg[0].len = sizeof(u8) * (length + 1);

	ret = i2c_transfer(sm->client->adapter, msg, ARRAY_SIZE(msg));
	if (ret < 0) {
		pr_err("i2c write buffer fail: can't write reg 0x%02X\n", decoder->reg);
	}

	mutex_unlock(&sm->i2c_rw_lock);
	return (ret < 0) ? ret : 0;
}
#endif
#if 0
static int fg_read_gaugeinfo_block(struct sh_fg_chip* sm)
{
	static u8 buf[GAUGEINFO_LEN];
	static char str[GAUGESTR_LEN];
	int i, j = 0;
	int ret;
	u16 temp;

	/* 20211029, Ethan. Tick Start*/
	u64 jiffies_now = get_jiffies_64(); /* 20211203, Ethan */
	s64 tick = (s64)(jiffies_now - sm->log_lastUpdate);
	if (tick < 0)  //overflow
	{
		tick = (s64)(U64_MAXVALUE - (u64)sm->log_lastUpdate);
		tick += jiffies_now + 1;
	}
	//tick /= HZ;
	do_div(tick,HZ);
	if (tick < GAUGE_LOG_MIN_TIMESPAN)
		return 0;

	if (!mutex_trylock(&sm->cali_lock)) { /* 20220807, Sinowealth */
		pr_err("SH366100_GaugeLog: could not get mutex!\n");
		return 0;
	}

	sm->log_lastUpdate = jiffies_now;
	/* 20211029, Ethan. Tick End */

	/* Cali Info */
	ret = fg_read_block(sm, CMD_CALIINFO, GAUGEINFO_LEN, buf);
	if (ret < 0) {
		pr_err("SH366100_GaugeLog: could not read CALIINFO, ret = %d\n", ret);
		goto fg_read_gaugeinfo_block_end; /* 20220105, Ethan */
	}

	memset(str, 0, GAUGESTR_LEN);
	i = 0;
	i += sprintf(&str[i], "Tick=%d, ", (u32)jiffies_now); //20211111, Ethan. In case print twice
	i += sprintf(&str[i], "Elasp=%lld, ", tick);
	i += sprintf(&str[i], "Voltage=%d, ", (s16)BUF2U16_LT(&buf[12]));
	i += sprintf(&str[i], "Current=%d, ", (s16)BUF2U32_LT(&buf[8]));
	i += sprintf(&str[i], "TS1Temp=%d, ", (s16)(BUF2U16_LT(&buf[22]) - TEMPER_OFFSET));
	i += sprintf(&str[i], "IntTemper=%d, ", (s16)(BUF2U16_LT(&buf[18]) - TEMPER_OFFSET));
	j = max(i, j);
	pr_err("SH366100_GaugeLog: CMD_CALIINFO is %s\n", str);

	/* 20211116, Ethan. Charge Info */
	ret = fg_read_block(sm, CMD_CHARGESTATUS, LEN_CHARGESTATUS, buf);
	if (ret < 0) {
		pr_err("SH366100_GaugeLog: could not read CHARGESTATUS, ret = %d\n", ret);
		goto fg_read_gaugeinfo_block_end; /* 20220105, Ethan */
	}
	ret = BUF2U16_BG(&buf[1]);
	ret |= ((u32)buf[0]) << 16;

	memset(str, 0, GAUGESTR_LEN);
	i = 0;
	i += sprintf(&str[i], "Tick=%d, ", (u32)jiffies_now);  //20211116, Ethan. In case print twice
	i += sprintf(&str[i], "ChgStatus=0x%06X, ", (u32)(ret & 0xFFFFFF));
	i += sprintf(&str[i], "DegradeFlag=0x%08X, ", BUF2U32_BG(&buf[3]));

	/* 20211116, Ethan. Term Volt */
	ret = fg_read_block(sm, CMD_TERMINATEVOLT, LEN_TERMINATEVOLT, buf);
	if (ret < 0) {
		pr_err("SH366100_GaugeLog: could not read TERMINATEVOLT, ret = %d\n", ret);
		goto fg_read_gaugeinfo_block_end; /* 20220105, Ethan */
	}

	i += sprintf(&str[i], "TermVolt=%d, ", BUF2U16_LT(&buf[0]));
	i += sprintf(&str[i], "TermVoltTime=%d, ", buf[2]);

	/* 20211123, Ethan */
	ret = fg_read_sbs_word(sm, CMD_CONTROLSTATUS, &temp);
		if (ret < 0) {
		pr_err("SH366100_GaugeLog: could not read CMD_CONTROLSTATUS, ret = %d\n", ret);
		goto fg_read_gaugeinfo_block_end; /* 20220105, Ethan */
	}
	i += sprintf(&str[i], "ControlStatus=0x%04X, ", temp);

	ret = fg_read_sbs_word(sm, CMD_RUNFLAG, &temp);
		if (ret < 0) {
		pr_err("SH366100_GaugeLog: could not read CMD_RUNFLAG, ret = %d\n", ret);
		goto fg_read_gaugeinfo_block_end; /* 20220105, Ethan */
	}
	i += sprintf(&str[i], "Flags=0x%04X, ", temp);

	ret = fg_read_sbs_word(sm, CMD_OEMFLAG, &temp); /* 20211126, Ethan */
		if (ret < 0) {
		pr_err("SH366100_GaugeLog: could not read CMD_OEMFLAG, ret = %d\n", ret);
		goto fg_read_gaugeinfo_block_end; /* 20220105, Ethan */
	}
	i += sprintf(&str[i], "OEMFLAG=0x%04X, ", temp);


	j = max(i, j);
	pr_err("SH366100_GaugeLog: CHARGESTATUS is %s\n", str);

	/* Lifetime Info. 20211126, Ethan */
//	ret = fg_read_block(sm, CMD_LIFETIMEADC, LEN_LIFETIMEADC, buf);
//	if (ret < 0) {
//		pr_err("SH366100_GaugeLog: could not read LIFETIMEADC, ret = %d\n", ret);
//		goto fg_read_gaugeinfo_block_end; /* 20220105, Ethan */
//	}
//
//	memset(str, 0, GAUGESTR_LEN);
//	i = 0;
//	i += sprintf(&str[i], "Tick=%d, ", (u32)jiffies_now);  //20211116, Ethan. In case print twice
//	i += sprintf(&str[i], "LT_MaxVolt=%d, ", (s16)BUF2U16_BG(&buf[0]));
//	i += sprintf(&str[i], "LT_MinVolt=%d, ", (s16)BUF2U16_BG(&buf[2]));
//	i += sprintf(&str[i], "LT_MaxChgCUR=%d, ", (s16)BUF2U16_BG(&buf[4]));
//	i += sprintf(&str[i], "LT_MaxDsgCUR=%d, ", (s16)BUF2U16_BG(&buf[6]));
//	i += sprintf(&str[i], "LT_MaxTemper=%d, ", (s8)buf[8]);
//	i += sprintf(&str[i], "LT_MinTemper=%d, ", (s8)buf[9]);
//	i += sprintf(&str[i], "LT_MaxIntTemper=%d, ", (s8)buf[10]);
//	i += sprintf(&str[i], "LT_MinIntTemper=%d, ", (s8)buf[11]);
//	j = max(i, j);
//	pr_err("SH366100_GaugeLog: LIFETIMEADC is %s\n", str);

	/* Gauge Info */
//	ret = fg_read_block(sm, CMD_GAUGEINFO, GAUGEINFO_LEN, buf);
//	if (ret < 0) {
//		pr_err("SH366100_GaugeLog: could not read GAUGEINFO, ret = %d\n", ret);
//		goto fg_read_gaugeinfo_block_end; /* 20220105, Ethan */
//	}
//
//	memset(str, 0, GAUGESTR_LEN);
//	i = 0;
//	i += sprintf(&str[i], "Tick=%d, ", (u32)jiffies_now);  //20211116, Ethan. In case print twice
//	i += sprintf(&str[i], "RunState=0x%08X, ", BUF2U32_LT(&buf[0]));
//	i += sprintf(&str[i], "GaugeState=0x%08X, ", BUF2U32_LT(&buf[4]));
//	i += sprintf(&str[i], "GaugeS2=0x%04X, ", BUF2U16_LT(&buf[8]));
//	i += sprintf(&str[i], "WorkState=0x%04X, ", BUF2U16_LT(&buf[10]));
//	i += sprintf(&str[i], "TimeInc=%d, ", buf[12]);
//	i += sprintf(&str[i], "MainTick=%d, ", buf[13]);
//	i += sprintf(&str[i], "SysTick=%d, ", buf[14]);
//	i += sprintf(&str[i], "ClockH=%d, ", BUF2U16_LT(&buf[15]));
//	i += sprintf(&str[i], "RamCheckT=%d, ", buf[17]);
//	i += sprintf(&str[i], "AutoCaliT=%d, ", buf[18]);
//	i += sprintf(&str[i], "LTHour=%d, ", buf[19]);
//	i += sprintf(&str[i], "LTTimer=%d, ", buf[20]);
//	i += sprintf(&str[i], "FlashT=%d, ", buf[21]);
//	i += sprintf(&str[i], "LTFlag=0x%02X, ", buf[22]);
//	i += sprintf(&str[i], "RSTS=0x%02X, ", buf[23]);
//	j = max(i, j);
//	pr_err("SH366100_GaugeLog: CMD_GAUGEINFO is %s\n", str);

	/* Gauge Block 2 */
//	ret = fg_read_block(sm, CMD_GAUGEBLOCK2, GAUGEINFO_LEN, buf);
//	if (ret < 0) {
//		pr_err("SH366100_GaugeLog: could not read CMD_GAUGEBLOCK2, ret = %d\n", ret);
//		goto fg_read_gaugeinfo_block_end; /* 20220105, Ethan */
//	}
//
//	memset(str, 0, GAUGESTR_LEN);
//	i = 0;
//	i += sprintf(&str[i], "Tick=%d, ", (u32)jiffies_now);  //20211116, Ethan. In case print twice
//	i += sprintf(&str[i], "QF1=0x%02X, ", buf[0]);
//	i += sprintf(&str[i], "QF2=0x%02X, ", buf[1]);
//	i += sprintf(&str[i], "PackQmax=%d, ", (s16)BUF2U16_BG(&buf[2]));
//	i += sprintf(&str[i], "CycleCount=%d, ", BUF2U16_BG(&buf[14]));
//	i += sprintf(&str[i], "QmaxCount=%d, ", BUF2U16_BG(&buf[16]));
//	i += sprintf(&str[i], "QmaxCycle=%d, ", BUF2U16_BG(&buf[18]));
//	i += sprintf(&str[i], "VatEOC=%d, ", (s16)BUF2U16_BG(&buf[4]));
//	i += sprintf(&str[i], "IatEOC=%d, ", (s16)BUF2U16_BG(&buf[6]));
//	i += sprintf(&str[i], "ChgVEOC=%d, ", (s16)BUF2U16_BG(&buf[8]));
//	i += sprintf(&str[i], "AVILR=%d, ", (s16)BUF2U16_BG(&buf[10]));
//	i += sprintf(&str[i], "AVPLR=%d, ", (s16)BUF2U16_BG(&buf[12]));
//	i += sprintf(&str[i], "ModelCount=%d, ", BUF2U16_BG(&buf[20]));
//	i += sprintf(&str[i], "ModelCycle=%d, ", BUF2U16_BG(&buf[22]));
//	i += sprintf(&str[i], "VCTCount=%d, ", BUF2U16_BG(&buf[24]));
//	i += sprintf(&str[i], "VCTCycle=%d, ", BUF2U16_BG(&buf[26]));
//	i += sprintf(&str[i], "RelaxCycle=%d, ", BUF2U16_BG(&buf[28]));
//	i += sprintf(&str[i], "RatioCycle=%d, ", BUF2U16_BG(&buf[30]));
//	j = max(i, j);
//	pr_err("SH366100_GaugeLog: GAUGEBLOCK2 is %s\n", str);
//
//	/* Gauge Block 3 */
//	ret = fg_read_block(sm, CMD_GAUGEBLOCK3, GAUGEINFO_LEN, buf);
//	if (ret < 0) {
//		pr_err("SH366100_GaugeLog: could not read CMD_GAUGEBLOCK3, ret = %d\n", ret);
//		goto fg_read_gaugeinfo_block_end; /* 20220105, Ethan */
//	}

//	memset(str, 0, GAUGESTR_LEN);
//	i = 0;
//	i += sprintf(&str[i], "Tick=%d, ", (u32)jiffies_now);  //20211116, Ethan. In case print twice
//	i += sprintf(&str[i], "SFR_RC=%d, ", (s16)BUF2U16_BG(&buf[0]));
//	i += sprintf(&str[i], "SFR_DCC=%d, ", (s16)BUF2U16_BG(&buf[2]));
//	i += sprintf(&str[i], "SFR_ICC=%d, ", (s16)BUF2U16_BG(&buf[4]));
//	i += sprintf(&str[i], "RCOffset=%d, ", (s16)BUF2U16_BG(&buf[6]));
//	i += sprintf(&str[i], "C0DOD1=%d, ", (s16)BUF2U16_BG(&buf[8]));
//	i += sprintf(&str[i], "PasCol=%d, ", (s16)BUF2U16_BG(&buf[10]));
//	i += sprintf(&str[i], "PasEgy=%d, ", (s16)BUF2U16_BG(&buf[12]));
//	i += sprintf(&str[i], "Qstart=%d, ", (s16)BUF2U16_BG(&buf[14]));
//	i += sprintf(&str[i], "Estart=%d, ", (s16)BUF2U16_BG(&buf[16]));
//	i += sprintf(&str[i], "FastTim=%d, ", buf[18]);
//	i += sprintf(&str[i], "FILFLG=0x%02X, ", buf[19]);
//	i += sprintf(&str[i], "StateTime=%d, ", BUF2U32_BG(&buf[20]));
//	i += sprintf(&str[i], "StateHour=%d, ", BUF2U16_BG(&buf[24]));
//	i += sprintf(&str[i], "StateSec=%d, ", BUF2U16_BG(&buf[26]));
//	i += sprintf(&str[i], "OCVTim=%d, ", BUF2U16_BG(&buf[28]));
//	i += sprintf(&str[i], "RaCalT1=%d, ", BUF2U16_BG(&buf[30]));
//	j = max(i, j);
//	pr_err("SH366100_GaugeLog: GAUGEBLOCK3 is %s\n", str);
//
//	/* Gauge Block 4 */
//	ret = fg_read_block(sm, CMD_GAUGEBLOCK4, GAUGEINFO_LEN, buf);
//	if (ret < 0) {
//		pr_err("SH366100_GaugeLog: could not read CMD_GAUGEBLOCK4, ret = %d\n", ret);
//		goto fg_read_gaugeinfo_block_end; /* 20220105, Ethan */
//	}

//	memset(str, 0, GAUGESTR_LEN);
//	i = 0;
//	i += sprintf(&str[i], "Tick=%d, ", (u32)jiffies_now);  //20211116, Ethan. In case print twice
//	i += sprintf(&str[i], "GUInx=0x%02X, ", buf[0]);
//	i += sprintf(&str[i], "GULoad=0x%02X, ", buf[1]);
//	i += sprintf(&str[i], "GUStatus=0x%08X, ", BUF2U32_BG(&buf[2]));
//	i += sprintf(&str[i], "EodLoad=%d, ", (s16)BUF2U16_BG(&buf[6]));
//	i += sprintf(&str[i], "CRatio=%d, ", (s16)BUF2U16_BG(&buf[8]));
//	i += sprintf(&str[i], "C0DOD0=%d, ", (s16)BUF2U16_BG(&buf[10]));
//	i += sprintf(&str[i], "C0EOC=%d, ", (s16)BUF2U16_BG(&buf[12]));
//	i += sprintf(&str[i], "C0EOD=%d, ", (s16)BUF2U16_BG(&buf[14]));
//	i += sprintf(&str[i], "C0ACV=%d, ", (s16)BUF2U16_BG(&buf[16]));
//	i += sprintf(&str[i], "ThemT=%d, ", (s16)BUF2U16_BG(&buf[18]));
//	i += sprintf(&str[i], "Told=%d, ", (s16)BUF2U16_BG(&buf[20]));
//	i += sprintf(&str[i], "Tout=%d, ", (s16)BUF2U16_BG(&buf[22]));
//	i += sprintf(&str[i], "RCRaw=%d, ", (s16)BUF2U16_BG(&buf[24]));
//	i += sprintf(&str[i], "FCCRaw=%d, ", (s16)BUF2U16_BG(&buf[26]));
//	i += sprintf(&str[i], "RERaw=%d, ", (s16)BUF2U16_BG(&buf[28]));
//	i += sprintf(&str[i], "FCERaw=%d, ", (s16)BUF2U16_BG(&buf[30]));
//	j = max(i, j);
//	pr_err("SH366100_GaugeLog: GAUGEBLOCK4 is %s\n", str);

	/* Gauge Block 5 */
	ret = fg_read_block(sm, CMD_GAUGEBLOCK5, GAUGEINFO_LEN, buf);
	if (ret < 0) {
		pr_err("SH366100_GaugeLog: could not read CMD_GAUGEBLOCK5, ret = %d\n", ret);
		goto fg_read_gaugeinfo_block_end; /* 20220105, Ethan */
	}

	memset(str, 0, GAUGESTR_LEN);
	i = 0;
	i += sprintf(&str[i], "Tick=%d, ", (u32)jiffies_now);  //20211116, Ethan. In case print twice
	i += sprintf(&str[i], "IdealFCC=%d, ", (s16)BUF2U16_BG(&buf[0]));
	i += sprintf(&str[i], "IdealFCE=%d, ", (s16)BUF2U16_BG(&buf[2]));
	i += sprintf(&str[i], "FilRC=%d, ", (s16)BUF2U16_BG(&buf[4]));
	i += sprintf(&str[i], "FilFCC=%d, ", (s16)BUF2U16_BG(&buf[6]));
	i += sprintf(&str[i], "FSOC=%d, ", buf[8]);
	i += sprintf(&str[i], "TrueRC=%d, ", (s16)BUF2U16_BG(&buf[9]));
	i += sprintf(&str[i], "TrueFCC=%d, ", (s16)BUF2U16_BG(&buf[11]));
	i += sprintf(&str[i], "RSOC=%d, ", buf[13]);
	i += sprintf(&str[i], "FilRE=%d, ", (s16)BUF2U16_BG(&buf[14]));
	i += sprintf(&str[i], "FilFCE=%d, ", (s16)BUF2U16_BG(&buf[16]));
	i += sprintf(&str[i], "FSOCW=%d, ", buf[18]);
	i += sprintf(&str[i], "TrueRE=%d, ", (s16)BUF2U16_BG(&buf[19]));
	i += sprintf(&str[i], "TrueFCE=%d, ", (s16)BUF2U16_BG(&buf[21]));
	i += sprintf(&str[i], "RSOCW=%d, ", buf[23]);
	i += sprintf(&str[i], "EquRC=%d, ", (s16)BUF2U16_BG(&buf[24]));
	i += sprintf(&str[i], "EquFCC=%d, ", (s16)BUF2U16_BG(&buf[26]));
	i += sprintf(&str[i], "EquRE=%d, ", (s16)BUF2U16_BG(&buf[28]));
	i += sprintf(&str[i], "EquFCE=%d, ", (s16)BUF2U16_BG(&buf[30]));
	j = max(i, j);
	pr_err("SH366100_GaugeLog: GAUGEBLOCK5 is %s\n", str);

	/* Gauge Block 6 */
//	ret = fg_read_block(sm, CMD_GAUGEBLOCK6, GAUGEINFO_LEN, buf);
//	if (ret < 0) {
//		pr_err("SH366100_GaugeLog: could not read CMD_GAUGEBLOCK6, ret = %d\n", ret);
//		goto fg_read_gaugeinfo_block_end; /* 20220105, Ethan */
//	}
//
//	memset(str, 0, GAUGESTR_LEN);
//	i = 0;
//	i += sprintf(&str[i], "Tick=%d, ", (u32)jiffies_now);  //20211116, Ethan. In case print twice
//	i += sprintf(&str[i], "GaugeS3=0x%04X, ", BUF2U16_BG(&buf[0]));
//	i += sprintf(&str[i], "CCON1=0x%02X, ", buf[2]);
//	i += sprintf(&str[i], "CCON2=0x%02X, ", buf[3]);
//	i += sprintf(&str[i], "ModelS=0x%02X, ", buf[4]);
//	i += sprintf(&str[i], "FGUpdate=0x%02X, ", buf[5]);
//	i += sprintf(&str[i], "FMGrid=0x%02X, ", buf[6]);
//	i += sprintf(&str[i], "ToggleCnt=%d, ", buf[7]);
//	i += sprintf(&str[i], "ORUpdate=0x%02X, ", buf[8]);
//	i += sprintf(&str[i], "UpState=0x%02X, ", buf[9]);
//	i += sprintf(&str[i], "ChgVol=%d, ", (s16)BUF2U16_BG(&buf[10]));
//	i += sprintf(&str[i], "TapCur=%d, ", (s16)BUF2U16_BG(&buf[12]));
//	i += sprintf(&str[i], "ChgCur=%d, ", (s16)BUF2U16_BG(&buf[14]));
//	i += sprintf(&str[i], "ChgRes=%d, ", (s16)BUF2U16_BG(&buf[16]));
//	i += sprintf(&str[i], "PrevI=%d, ", (s16)BUF2U16_BG(&buf[18]));
//	i += sprintf(&str[i], "DeltaC=%d, ", (s16)BUF2U16_BG(&buf[20]));
//	i += sprintf(&str[i], "SOCJmpCnt=%d, ", buf[22]);
//	i += sprintf(&str[i], "SOWJmpCnt=%d, ", buf[23]);
//	i += sprintf(&str[i], "OcvVcell=%d, ", (s16)BUF2U16_BG(&buf[24]));
//	i += sprintf(&str[i], "FGMeas=%d, ", (s16)BUF2U16_BG(&buf[26]));
//	i += sprintf(&str[i], "FGPrid=%d, ", (s16)BUF2U16_BG(&buf[28]));
//	i += sprintf(&str[i], "FastTime=%d, ", (s16)BUF2U16_BG(&buf[30]));
//	j = max(i, j);
//	pr_err("SH366100_GaugeLog: GAUGEBLOCK6 is %s\n", str);

	/* Gauge Fusion Model */
//	ret = fg_read_block(sm, CMD_GAUGEBLOCK_FG, GAUGEINFO_LEN, buf);
//	if (ret < 0) {
//		pr_err("SH366100_GaugeLog: could not read CMD_GAUGEBLOCK_FG, ret = %d\n", ret);
//		goto fg_read_gaugeinfo_block_end; /* 20220105, Ethan */
//	}
//
//	memset(str, 0, GAUGESTR_LEN);
//	i = 0;
//	i += sprintf(&str[i], "Tick=%d, ", (u32)jiffies_now);  //20211116, Ethan. In case print twice
//	i += sprintf(&str[i], "FusionModel=");
//	for (ret = 0; ret < 15; ret++)
//		i += sprintf(&str[i], "0x%04X ", BUF2U16_BG(&buf[ret * 2]));
//	j = max(i, j); //20211116, Ethan
//	pr_err("SH366100_GaugeLog: FusionModel is %s\n", str); //20211116, Ethan

	//20211111, Ethan
	/* CADC Info */
//	ret = fg_read_block(sm, CMD_CADCINFO, GAUGEINFO_LEN, buf);
//	if (ret < 0) {
//		pr_err("SH366100_GaugeLog: could not read CMD_CADCINFO, ret = %d\n", ret);
//		goto fg_read_gaugeinfo_block_end; /* 20220105, Ethan */
//	}

//	memset(str, 0, GAUGESTR_LEN);
//	i = 0;
//	i += sprintf(&str[i], "Tick=%d, ", (u32)jiffies_now);  //20211116, Ethan. In case print twice
//	i += sprintf(&str[i], "TEOFFSET=%d, ", (s16)BUF2U16_LT(&buf[0]));
//	i += sprintf(&str[i], "UserOFFSET=%d, ", (s16)BUF2U16_LT(&buf[2]));
//	i += sprintf(&str[i], "BoardOffset=%d, ", (s16)BUF2U16_LT(&buf[4]));
//	i += sprintf(&str[i], "CADC25DEG=%d, ", (s16)BUF2U16_LT(&buf[6]));
//	i += sprintf(&str[i], "CADCKR=%d, ", (s16)BUF2U16_LT(&buf[8]));
//	i += sprintf(&str[i], "ChosenOffset=%d, ", (s16)BUF2U16_LT(&buf[10]));
//	i += sprintf(&str[i], "TELiner=%d, ", (s16)BUF2U16_LT(&buf[12]));
//	i += sprintf(&str[i], "UserLiner=%d, ", (s16)BUF2U16_LT(&buf[14]));
//	i += sprintf(&str[i], "CurLiner=%d, ", (s16)BUF2U16_LT(&buf[16]));
//	i += sprintf(&str[i], "COR=%d, ", (s16)BUF2U16_LT(&buf[18]));
//	i += sprintf(&str[i], "CADC=%d, ", (s32)BUF2U32_LT(&buf[20]));
//	i += sprintf(&str[i], "Current=%d, ", (s16)BUF2U16_LT(&buf[24]));
//	i += sprintf(&str[i], "PackConfig=0x%04X, ", BUF2U16_LT(&buf[26]));
//	j = max(i, j);
//	pr_err("SH366100_GaugeLog: CADCINFO is %s\n", str);

	//20211116, Ethan
	// j = max(i, j);
	// pr_err("SH366100_GaugeLog: FusionModel is %s", str);
	pr_err("SH366100_GaugeLog: max len=%d\n", j);

	ret = 0;

fg_read_gaugeinfo_block_end: /* 20220105, Ethan */
	mutex_unlock(&sm->cali_lock); /* 20220807, Sinowealth */
	return ret;
}
#endif
int st_fg_read_soc(void)
{
	// if(st_sm == NULL){
	// 	pr_err(" %s %d st_sm == NULL \n", __func__,__LINE__);
	// 	return 0;
	// }
	pr_info("st_fg_read_soc SOC=%d\n", SUNRITEL_CHARGER_UISOC);

	return SUNRITEL_CHARGER_UISOC;
}

EXPORT_SYMBOL(st_fg_read_soc);


int st_fg_read_volt(void)
{
	// if(st_sm == NULL){
	// 	pr_err(" %s %d st_sm == NULL \n", __func__,__LINE__);
	// 	return 0;
	// }
	pr_info("st_fg_read_volt Voltage=%d\n", st_batt_volt);

	return st_batt_volt;
}

EXPORT_SYMBOL(st_fg_read_volt);

int st_fg_read_current(void)
{
	// if(st_sm == NULL){
	// 	pr_err(" %s %d st_sm == NULL \n", __func__,__LINE__);
	// 	return 0;
	// }
	pr_info("st_fg_read_current current=%d\n", (st_batt_curr/1000));
	return (st_batt_curr/1000);
}

EXPORT_SYMBOL(st_fg_read_current);

int  st_SH366100_WriteFirstTimeToSeriaNo(void)
{
	if(st_sm == NULL){
		st_batt_SH366100_WriteFirstTimeToSeriaNo_status = 0;
		return -1;
	}
	st_batt_SH366100_WriteFirstTimeToSeriaNo_status = 1;
	fg_SH366100_Unseal(st_sm);
	SH366100_WriteFirstTimeToSeriaNo(st_sm);
	msleep(1000);

	return 0;
}

EXPORT_SYMBOL(st_SH366100_WriteFirstTimeToSeriaNo);

static int SH366100_Read_SNCode_To_BlockA(struct sh_fg_chip* sm)
{
	int ret;


	ret = fg_read_block(sm, CMD_BLOCKA, 32, st_SNCode_Read);
	if (ret < 0) {
		pr_err(" %s %d could not read SH366100_ReadManufactureData, ret = %d \n", __func__,__LINE__,ret);	
		return ret;
	}

	pr_err("%s %d SNCode_Read = %s \n", __func__,__LINE__,st_SNCode_Read);	
	//mutex_unlock(&sm->data_lock);
	
	return 0;
}


#if 0
static int SH366100_Write_SNCode_To_BlockA(struct sh_fg_chip* sm)
{
	int ret;
	ret = fg_write_block(sm, CMD_BLOCKA, 17, st_SNCode_Write);
	if (ret < 0) {
		pr_err(" %s %d could not write SH366100_Write_SNCode_To_BlockA, ret = %d \n", __func__,__LINE__,ret);	
		return ret;
	}
	pr_err("%s %d SNCode_Write=%s\n", __func__,__LINE__,st_SNCode_Write);	
	
	return 0;
}
#endif


static int SH366100_ReadFirstTimeToSeriaNo(struct sh_fg_chip* sm)
{
	int ret;
	u16 firsttime = 0x00;

	ret = fg_read_sbs_word(sm, CMD_SERIAL_NUMBER, &firsttime);
	if (ret < 0) {
		pr_err(" %s %d could not read SH366100_ReadManufactureData, ret = %d \n", __func__,__LINE__,ret);	
		return ret;
	}

	//mutex_lock(&sm->data_lock);
	st_FirstUse_Day_Read = firsttime &0x001F;
	st_FirstUse_Month_Read = ( (firsttime >> 5) & 0x000F );
	st_FirstUse_Year_Read = ( ( (firsttime >> 9) & 0x007F ) + 1980 );	

	pr_err("%s %d st_FirstUse_Year_Read =%d st_FirstUse_Month_Read =%d st_FirstUse_Day_Read =%d\n", __func__,__LINE__,st_FirstUse_Year_Read,st_FirstUse_Month_Read,st_FirstUse_Day_Read);	
	//mutex_unlock(&sm->data_lock);
	
	return 0;
}



static int SH366100_WriteFirstTimeToSeriaNo(struct sh_fg_chip* sm)
{
	int ret;
	u16 firsttime = 0x00;
	u8 firsttime_buffer[2] = {0x00};
	
	firsttime = firsttime | (st_FirstUse_Day_Write & 0x001F);
	firsttime = firsttime | ( (st_FirstUse_Month_Write & 0x000F) <<5);
	firsttime = firsttime | ( ((st_FirstUse_Year_Write - 1980) & 0x007F) << 9 );
	
	firsttime_buffer[0] = (uint8_t)firsttime;
	firsttime_buffer[1] = (uint8_t)(firsttime >> 8);

	ret = fg_write_block(sm, CMD_SERIAL_NUMBER, 2, firsttime_buffer);
	if (ret < 0) {
		pr_err(" %s %d could not write SH366100_WriteFirstTimeToSeriaNo, ret = %d \n", __func__,__LINE__,ret);	
		return ret;
	}
	pr_err("%s %d firsttime_buffer=%s\n", __func__,__LINE__,firsttime_buffer);	
	
	return 0;
}





static int SH366100_ReadManufactureData(struct sh_fg_chip* sm)
{
	int ret;
	u16 manufacture = 0x00;

	ret = fg_read_sbs_word(sm, CMD_MANUDATE, &manufacture);
	if (ret < 0) {
		pr_err(" %s %d could not read SH366100_ReadManufactureData, ret = %d \n", __func__,__LINE__,ret);	
		return ret;
	}

	//mutex_lock(&sm->data_lock);
	st_Day_Read = manufacture &0x001F;
	st_Month_Read = ( (manufacture >> 5) & 0x000F );
	st_Year_Read = ( ( (manufacture >> 9) & 0x007F ) + 1980 );	

	pr_err("%s %d st_Year_Read =%d st_Month_Read =%d st_Day_Read =%d\n", __func__,__LINE__,st_Year_Read,st_Month_Read,st_Day_Read);	
	//mutex_unlock(&sm->data_lock);
	
	return 0;
}


#if 0
static int SH366100_WriteManufactureData(struct sh_fg_chip* sm)
{
	int ret;
	u16 manufacture = 0x00;
	u8 manufacture_buffer[2] = {0x00};



	manufacture = manufacture | (st_Day_Write & 0x001F);
	manufacture = manufacture | ( (st_Month_Write & 0x000F) <<5);
	manufacture = manufacture | ( ((st_Year_Write - 1980) & 0x007F) << 9 );
	
	manufacture_buffer[0] = (uint8_t)manufacture;
	manufacture_buffer[1] = (uint8_t)(manufacture >> 8);



	ret = fg_write_block(sm, CMD_MANUDATE, 2, manufacture_buffer);
	if (ret < 0) {
		pr_err(" %s %d could not write SH366100_WriteManufactureData, ret = %d \n", __func__,__LINE__,ret);	
		return ret;
	}
	pr_err("%s %d manufacture_buffer=%s\n", __func__,__LINE__,manufacture_buffer);	
	
	return 0;
}
#endif


static int fg_SH366100_Unseal(struct sh_fg_chip* sm)
{
	int ret;
	u16 controlStatus = 0;

	ret = fg_write_sbs_word(sm, CMD_CONTROL, (u16)Unseal);
	if (ret < 0) {
		pr_err(" %s %d could not write Unseal, ret = %d \n", __func__,__LINE__,ret);	
		return ret;
	}
	msleep(5);
	ret = fg_write_sbs_word(sm, (u16)CMD_CONTROL, (u16)(Unseal >> 16));
	if (ret < 0) {
		pr_err(" %s %d could not write Unseal, ret = %d \n", __func__,__LINE__,ret);	
		return ret;
	}
	msleep(5);
	ret = fg_write_sbs_word(sm, CMD_CONTROL, (u16)Fullaccess);
	if (ret < 0) {
		pr_err(" %s %d could not write Fullaccess, ret = %d \n", __func__,__LINE__,ret);	
		return ret;
	}
	msleep(5);
	ret = fg_write_sbs_word(sm, (u16)CMD_CONTROL, (u16)(Fullaccess >> 16));
	if (ret < 0) {
		pr_err(" %s %d could not write Fullaccess, ret = %d \n", __func__,__LINE__,ret);	
		return ret;
	}
	msleep(5);
	ret = fg_read_sbs_word(sm, CMD_CONTROL, &controlStatus);
	if (ret < 0) {
		pr_err(" %s %d could not read controlStatus, ret = %d \n", __func__,__LINE__,ret);	
		return ret;
	}
	msleep(5);
	//mutex_lock(&sm->data_lock);
	
	if( (controlStatus & 0x6000) != 0 ){
		pr_err("%s %d controlStatus=%x\n", __func__,__LINE__,controlStatus);	
		sm->controlStatus = 0;
		return 0;
	}
	else{
		pr_err("%s %d controlStatus=%x\n", __func__,__LINE__,controlStatus);	
		sm->controlStatus = 1;
		return 1;
	}
	// mutex_unlock(&sm->data_lock);
	
	return 0;
}




static int fg_read_ReadFCC(struct sh_fg_chip* sm)//fcc
{
	int ret;
	u16 data = 0;

	ret = fg_read_sbs_word(sm, CMD_FCC, &data);
	if (ret < 0) {
		pr_err(" %s %d could not read FullChargingCapacity, ret = %d \n", __func__,__LINE__,ret);	
		return ret;
	}

	mutex_lock(&sm->data_lock);
	
	sm->FullChargingCapacity = data;
	
	mutex_unlock(&sm->data_lock);
	st_FullChargingCapacity = sm->FullChargingCapacity;
	pr_err("%s %d FullChargingCapacity=%d st_FullChargingCapacity %d\n", __func__,__LINE__,sm->FullChargingCapacity,st_FullChargingCapacity);	
	return 0;
}


static int fg_read_ReadCycleCount(struct sh_fg_chip* sm)//电池循环使用次数
{
	int ret;
	u16 data = 0;

	ret = fg_read_sbs_word(sm, CMD_CYCLE, &data);
	if (ret < 0) {
		pr_err(" %s %d could not read cyclecount, ret = %d \n", __func__,__LINE__,ret);	
		return ret;
	}

	mutex_lock(&sm->data_lock);
	
	sm->cyclecount = data;

	mutex_unlock(&sm->data_lock);
	st_cyclecount = sm->cyclecount;
	pr_err("%s %d cyclecount=%d st_cyclecount %d\n", __func__,__LINE__,sm->cyclecount,st_cyclecount);		
	return 0;
}


static int fg_read_DesignCapacity(struct sh_fg_chip* sm)//电池设计容量
{
	int ret;
	u16 data = 0;

	ret = fg_read_sbs_word(sm, CMD_DESIGNCAPACITY, &data);
	if (ret < 0) {
		pr_err(" %s %d could not read designCapacity, ret = %d \n", __func__,__LINE__,ret);	
		return ret;
	}

	mutex_lock(&sm->data_lock);
	
	sm->designCapacity = data;
	


	
	mutex_unlock(&sm->data_lock);
	st_designCapacity = sm->designCapacity;

	pr_err("%s %d designCapacity=%d st_designCapacity %d\n", __func__,__LINE__,sm->designCapacity,st_designCapacity);
	return 0;
}




static int fg_read_soc_h(struct sh_fg_chip* sm)//电池健康度
{
	int ret;
	u16 data = 0;

	ret = fg_read_sbs_word(sm, CMD_SOH, &data);
	if (ret < 0) {
		pr_err(" %s %d could not read SOC_h, ret = %d \n", __func__,__LINE__,ret);	
		return ret;
	}

	mutex_lock(&sm->data_lock);

	//data = min(data, 96);
	pr_err("fg_read_soc_h SOC_a=%d\n", data);
	
	sm->batt_soc_h = data;
	
	if(sm->batt_soc_h >= 100)
        sm->batt_soc_h = 100;
			
	mutex_unlock(&sm->data_lock);
	st_batt_soc_h = sm->batt_soc_h;			

	pr_err("%s %d SOC_h=%d st_batt_soc_h %d\n", __func__,__LINE__,sm->batt_soc_h,st_batt_soc_h);
//	fg_read_gaugeinfo_block(sm); //20220518, Ethan
	return 0;
}

static int fg_read_soc(struct sh_fg_chip* sm)
{
	int ret;
	u16 data = 0;

	ret = fg_read_sbs_word(sm, sm->regs[SH_FG_REG_SOC], &data);
	if (ret < 0) {
		pr_err("fg_read_soc could not read SOC, ret = %d\n", ret);
		return ret;
	}

	mutex_lock(&sm->data_lock);

	//data = min(data, 96);
	pr_err("fg_read_soc SOC_b=%d\n", data);
	
	//sm->batt_soc = data * 100 / 96;
	sm->batt_soc = data;
	//if(sm->batt_soc >= 100)
	//				sm->batt_soc = 100;

	mutex_unlock(&sm->data_lock);
	SUNRITEL_CHARGER_UISOC = sm->batt_soc;		
	pr_err("fg_read_soc SOC=%d SUNRITEL_CHARGER_UISOC %d \n", sm->batt_soc,SUNRITEL_CHARGER_UISOC);
//	fg_read_gaugeinfo_block(sm); //20220518, Ethan
	return 0;
}

static int fg_read_volt(struct sh_fg_chip* sm)
{
	int ret;
	u16 data = 0;

	ret = fg_read_sbs_word(sm, sm->regs[SH_FG_REG_VOLTAGE], &data);
	if (ret < 0) {
		pr_err("could not read voltage, ret = %d\n", ret);
		return ret;
	}

	mutex_lock(&sm->data_lock);
//	sm->batt_volt = ((int)data * MA_TO_UA);
	sm->batt_volt = ((int)data);

	pr_err("Voltage=%d\n", sm->batt_volt);
	mutex_unlock(&sm->data_lock);
	st_batt_volt = sm->batt_volt;
	return 0;
}

static int fg_read_current(struct sh_fg_chip* sm)
{
	int ret;
	u16 data = 0;

	ret = fg_read_sbs_word(sm, sm->regs[SH_FG_REG_CURRENT], &data);
	if (ret < 0) {
		pr_err("could not read current, ret = %d\n", ret);
		return ret;
	}

	mutex_lock(&sm->data_lock);
	sm->batt_curr = (u32)((s16)data * MA_TO_UA);
	pr_err("Current=%d\n", sm->batt_curr);
	mutex_unlock(&sm->data_lock);
	st_batt_curr = sm->batt_curr;
	return 0;
}

static int  fg_read_ExtTemperature(struct sh_fg_chip* sm)
{
	u16 data;
	int ret;
	int  value;

	ret = fg_read_sbs_word(sm, sm->regs[SH_FG_REG_TEMPERATURE_EX], &data);
	if (ret < 0) {
		pr_err("could not read ExtTemperature, ret = %d\n", ret);	
		goto fg_read_DEVICE_TYPE_end;
	}
	else{

		value = (s16)data- TEMPER_OFFSET;

		st_charge_temp = value/10;

		pr_err("ExtTemperature= %d value %d st_charge_temp %d \n", (s16)data- TEMPER_OFFSET,value,st_charge_temp);
	}
	ret = 0;
fg_read_DEVICE_TYPE_end:
	return ret;
	
}

static int  fg_read_IntTemperature(struct sh_fg_chip* sm)
{
	u16 data;
	int ret;

	ret = fg_read_sbs_word(sm, sm->regs[SH_FG_REG_TEMPERATURE_IN], &data);
	if (ret < 0) {
		pr_err("could not read IntTemperature, ret = %d\n", ret);	
		goto fg_read_DEVICE_TYPE_end;
	}
	else{
		pr_err("IntTemperature= %d\n", (s16)data- TEMPER_OFFSET);
	}
	
	ret = 0;
fg_read_DEVICE_TYPE_end:
	return ret;
	
}




/* 20211112, Ethan. Termniate Voltage */
#if 0
static int fg_read_terminate_voltage(struct sh_fg_chip* sm)
{
	int ret;
	u8 buf[LEN_TERMINATEVOLT];

	ret = fg_read_block(sm, CMD_TERMINATEVOLT, LEN_TERMINATEVOLT, buf);
	if (ret < 0) {
		pr_err("could not read Terminate Voltage, ret=%d\n", ret);
		return ret;
	}

	ret = (int)(buf[0] | ((u16)buf[1] << 8));
	mutex_lock(&sm->data_lock);
	sm->terminate_voltage = ret * MA_TO_UA; /* 20211208, Ethan */
	mutex_unlock(&sm->data_lock);
	return 0;
}

static int fg_gauge_unseal(struct sh_fg_chip* sm) /* 20220517, Ethan */
{
	int ret;
	int i;
	u16 oemflag;

	for (i = 0; i < 5; i++) {
		ret = fg_write_sbs_word(sm, CMD_ALTMAC, (u16)CMD_UNSEALKEY);
		if (ret < 0)
			goto fg_gauge_unseal_End;
		HOST_DELAY(CMD_SBS_DELAY);

		ret = fg_write_sbs_word(sm, CMD_ALTMAC, (u16)(CMD_UNSEALKEY >> 16));
		if (ret < 0)
			goto fg_gauge_unseal_End;
		HOST_DELAY(CMD_SBS_DELAY);

		ret = fg_read_sbs_word(sm, CMD_OEMFLAG, &oemflag);
		if (ret < 0)
			goto fg_gauge_unseal_End;
		HOST_DELAY(CMD_SBS_DELAY);

		if ((oemflag & 0x0003) == 0)
			break;
		HOST_DELAY(CMD_SBS_DELAY);
	}

	ret = (i < 5) ? 0 : -1;
fg_gauge_unseal_End:
	pr_err("fg_gauge_unseal: ret=%d", ret);
	return ret;
}

static int fg_gauge_seal(struct sh_fg_chip* sm) /* 20211122, Ethan. Gauge Enable */
{
	return fg_write_sbs_word(sm, CMD_ALTMAC, CMD_SEAL);
}

//20220803, Sinowealth. Calibration

static int fg_enter_calibration_mode(struct sh_fg_chip* sm)
{
#define CMD_ENTER_CALI (CMDMASK_ALTMAC_W | 0xE014)
#define LEN_ENTER_CALI 6
#define CMD_ENTER_OEM (CMDMASK_ALTMAC_W | 0xE001)
#define LEN_ENTER_OEM 8
#define PACKCON_MASK 
	u8 bufcmd[LEN_ENTER_CALI] = { 0x45, 0x54, 0x43, 0x41, 0x4C, 0x49 };
	u8 bufoem[LEN_ENTER_OEM] = { 0x65, 0x4E, 0x54, 0x45, 0x52, 0x4F, 0x45, 0x4D };
	int retry;
	u16 temp = 0;
	int ret;

	for (retry = 0; retry < 5; retry++) {
		ret = fg_read_sbs_word(sm, CMD_OEMFLAG, &temp);
		HOST_DELAY(CMD_SBS_DELAY);
		if (ret >= 0) {
			if ((temp & CMD_MASK_OEM_CALI) == CMD_MASK_OEM_CALI) {
				ret = 0;
				goto fg_check_calibration_mode_end;
			}

			ret = fg_gauge_unseal(sm);
			if (ret < 0)
				continue;
			msleep(CMD_SBS_DELAY);

			ret = fg_write_block(sm, CMD_ENTER_OEM, LEN_ENTER_OEM, bufoem);
			if (ret < 0)
				continue;
			msleep(CMD_SBS_DELAY);
			
			ret = fg_write_block(sm, CMD_ENTER_CALI, LEN_ENTER_CALI, bufcmd);
			if (ret >= 0)
				break;
		}
		HOST_DELAY(200);
	}

fg_check_calibration_mode_end:

	ret = fg_read_sbs_word(sm, CMD_OEMFLAG, &temp);
	if (ret >= 0)
		ret = ((temp & CMD_MASK_OEM_CALI) == CMD_MASK_OEM_CALI) ? 0 : -1;

	pr_err("fg_enter_calibration_mode: ret = %d, oemFlag=0x%04X\n", ret, temp);
	return ret;
}

static int fg_exit_calibration_mode(struct sh_fg_chip* sm)
{
#define CMD_EXIT_CALI (CMDMASK_ALTMAC_W | 0xE015)
#define LEN_EXIT_CALI 6
	u8 bufcmd[LEN_EXIT_CALI] = { 0x65, 0x78, 0x63, 0x61, 0x6C, 0x69 };
	int retry;
	int ret;
#define CMD_EXIT_OEM (CMDMASK_ALTMAC_W | 0xE018)
#define LEN_EXIT_OEM 7
	u8 bufoem[LEN_EXIT_OEM] = { 0x45, 0x78, 0x69, 0x74, 0x6F, 0x65, 0x6D };
	u16 temp;

	for (retry = 0; retry < 2; retry++) {
		ret = fg_write_block(sm, CMD_EXIT_CALI, LEN_EXIT_CALI, bufcmd);
		msleep(CMD_SBS_DELAY);

		ret |= fg_write_block(sm, CMD_EXIT_OEM, LEN_EXIT_OEM, bufoem);
		if (ret >= 0)
			break;

		HOST_DELAY(200);
	}

fg_check_calibration_mode_end:
	fg_gauge_seal(sm);
	msleep(CMD_SBS_DELAY);

	fg_read_sbs_word(sm, CMD_OEMFLAG, &temp);
	ret = !!((temp & (CMD_MASK_OEM_CALI | CMD_MASK_OEM_OEM | CMD_MASK_OEM_SEAL)) == CMD_MASK_OEM_SEAL);
	pr_err("fg_exit_calibration_mode: ret = %d, oemFlag=0x%04X\n", ret, temp);
	return ret;
}
#endif
#define CMD_FORCE_UPDATE_E2ROM (CMDMASK_ALTMAC_W | 0xE012)
#define LEN_FORCE_UPDATE_E2ROM 4
#define CMD_E2ROM_BOARDOFFSET (CMDMASK_ALTMAC_W | 0x4044)
#define CMD_E2ROM_CURRENTRATIO (CMDMASK_ALTMAC_W | 0x4040)

#define LEN_CALIBOARD 9
#define CMD_CALIBOARD (CMDMASK_ALTMAC_W | 0xE016)
	
#if 0	
static int fgcali_force_e2rom_update(struct sh_fg_chip* sm)
{
	u8 buf_ForceE2romUpdate[LEN_FORCE_UPDATE_E2ROM] = { 0x45, 0x32, 0x55, 0x50 };
	int ret;

	ret = fg_write_block(sm, CMD_FORCE_UPDATE_E2ROM, LEN_FORCE_UPDATE_E2ROM, buf_ForceE2romUpdate);
	msleep(1500);
    pr_err("fgcali_force_e2rom_update ret=%d", ret);
	return ret;
}

static int fgcali_set_e2rom_offset(struct sh_fg_chip* sm, int offset_input)
{
	int ret;
	u8 buf[2];

    buf[0] = (offset_input >> 8) & 0xFF;
    buf[1] = offset_input & 0xFF;
    
	ret = fg_write_block(sm, CMD_E2ROM_BOARDOFFSET, 2, buf);
	msleep(CMD_SBS_DELAY);
    pr_err("fgcali_set_e2rom_offset ret=%d, offset_input=%d", ret, offset_input);
	return ret;
}
#endif
/*
static int fgcali_set_e2rom_ratio(struct sh_fg_chip* sm, int ratio_input)
{
	int ret;
	u8 buf[2];

	buf[0] = (ratio_input >> 8) & 0xFF;
	buf[1] = ratio_input & 0xFF;

	ret = fg_write_block(sm, CMD_E2ROM_CURRENTRATIO, 2, buf);
	msleep(CMD_SBS_DELAY);
    pr_err("fgcali_set_e2rom_ratio ret=%d, ratio_input=%d", ret, ratio_input);
	return ret;
}
*/
#if 0
static int fg_gauge_calibrate_board(struct sh_fg_chip* sm)
{
#define CALI_MAX_CURRENT (100)
#define CALI_MAX_CURRENT_DELTA (50)
#define DEFAULT_SENSOR_RATIO (1000)
#define CALI_MAX_CNT   5
#define CALI_DELAY_MS  250
#define CALIINFO_INDEX_CHOSEOFFSET 10
#define CALIINFO_INDEX_CURRENT 24
#define CALIINFO_INDEX_CADC 20
#define CALIINFO_INDEX_FLASHOFFSET 2

	int ret;
	u8 buf[32];
	int i;
	int cali_cur;
	int cali_cadc;
	int offset;
	int cur_max, cur_min, cadc_max, cadc_min;
    int te_offset;

	//20220807, Sinowealth
	ret = -1;
	for (i = 0; i < 2; i++) {
		if (mutex_trylock(&sm->cali_lock)) {
			ret = 0;
			break;
		}
		msleep(200);
	}
	if (ret < 0) {
		pr_err("fg_gauge_calibrate_board fail: could not get mutex!");
		return -1;
	}

	ret = fg_enter_calibration_mode(sm);
	if (ret < 0) {
		pr_err("fg_calibrate_boardoffset error! cannot enter cali mode!\n");
		goto fg_calibrate_boardoffset_end;
	}

	offset = 0;
	cur_max = 0x80000000;
	cur_min = 0x7FFFFFFF;
	cadc_max = 0x80000000;
	cadc_min = 0x7FFFFFFF;
	for (i = 0; i < CALI_MAX_CNT; i++) {
		msleep(CALI_DELAY_MS);
		ret = fg_read_block(sm, CMD_CADCINFO, 32, buf);
		if (ret < 0) {
			pr_err("fg_calibrate_boardoffset %d comm error!\n", i);
			goto fg_calibrate_boardoffset_end;
		}

		cali_cur = (int)((s16)BUF2U16_LT(&buf[CALIINFO_INDEX_CURRENT]));
		cur_max = max(cur_max, cali_cur);
		cur_min = min(cur_min, cali_cur);
		
		cali_cadc = (int)BUF2U32_LT(&buf[CALIINFO_INDEX_CADC]);
		offset += cali_cadc;
		cadc_max = max(cadc_max, cali_cadc);
		cadc_min = min(cadc_min, cali_cadc);
        
        te_offset = 0;
        if (buf[27] & 0x40) //TE_Offset
            te_offset = (int)((s16)BUF2U16_LT(&buf[0]));

		pr_err("fg_calibrate_boardoffset raw-data %d: cadc=%d, cur=%d\n", i, cali_cadc, cali_cur);
	}

	offset = offset - cadc_max - cadc_min;
	offset /= (CALI_MAX_CNT - 2);
	ret = 0;
	if ((abs(cur_max) >= CALI_MAX_CURRENT) || (abs(cur_min) >= CALI_MAX_CURRENT) || (abs(cur_max - cur_min) >= CALI_MAX_CURRENT_DELTA))
		ret = -1;

	if (ret < 0) {
		pr_err("fg_calibrate_boardoffset error! Raw-data exceed! max cur=%d, min cur=%d, delta cur=%d, avg_offset=%d, te_offset=%d\n", cur_max, cur_min, cur_max - cur_min, offset, te_offset);
		goto fg_calibrate_boardoffset_end;
	}
	else
		pr_err("fg_calibrate_boardoffset raw-data summary: max cur=%d, min cur=%d, delta cur=%d, avg_offset=%d, te_offset=%d\n", cur_max, cur_min, cur_max - cur_min, offset, te_offset);
    offset -= te_offset;

	ret = fgcali_set_e2rom_offset(sm, offset);
	if (ret < 0) {
		pr_err("fg_calibrate_boardoffset error! cannot write offset!\n");
		goto fg_calibrate_boardoffset_end;
	}

	ret = fgcali_force_e2rom_update(sm);
	if (ret < 0) {
		pr_err("fg_calibrate_boardoffset error! cannot update e2rom!\n");
		goto fg_calibrate_boardoffset_end;
	}

fg_calibrate_boardoffset_end:
	msleep(CALI_DELAY_MS);
	ret |= fg_read_block(sm, CMD_CADCINFO, 32, buf);
	offset = (int)((s16)BUF2U16_LT(&buf[CALIINFO_INDEX_CHOSEOFFSET]));
	cali_cur = (int)((s16)BUF2U16_LT(&buf[CALIINFO_INDEX_CURRENT]));
	cali_cadc = (int)BUF2U32_LT(&buf[CALIINFO_INDEX_CADC]);
	cur_max = (int)((s16)BUF2U16_LT(&buf[CALIINFO_INDEX_FLASHOFFSET]));

	ret |= fg_exit_calibration_mode(sm);
	
	pr_err("fg_calibrate_boardoffset end: ret=%d, read ic chose_offset=%d, flash_offset=%d, current=%d, cadc=%d\n", ret, offset, cur_max, cali_cur, cali_cadc);

	mutex_unlock(&sm->cali_lock); //20220807, Sinowealth
	return ret;
}

static int fg_gauge_runstate_check(struct sh_fg_chip* sm) /* 20211126, Ethan */
{
	int ret;
	u16 oemflag;
	int retry_cnt;
	u32 socFlag = 0, qenFlag = 0, ltFlag = 0;
	u16 temperature;

	/* 20220518, Ethan. In case por with poor connection */
	for (retry_cnt = 0; retry_cnt < 5; retry_cnt++) {
		ret = fg_read_soc(sm);
		if (ret < 0)
			goto fg_gauge_runstate_check_End;
		HOST_DELAY(CMD_SBS_DELAY);

		ret = fg_read_volt(sm);
		if (ret < 0)
			goto fg_gauge_runstate_check_End;
		HOST_DELAY(CMD_SBS_DELAY);

		ret = fg_read_current(sm);
		if (ret < 0)
			goto fg_gauge_runstate_check_End;
		HOST_DELAY(CMD_SBS_DELAY);

		//20220106, Ethan
		ret = fg_read_sbs_word(sm, sm->regs[SH_FG_REG_TEMPERATURE_EX], &temperature);
		if (ret < 0)
			goto fg_gauge_runstate_check_End;
		temperature -= 2731;

		ret = fg_read_terminate_voltage(sm);
		if (ret < 0)
			goto fg_gauge_runstate_check_End;
		HOST_DELAY(CMD_SBS_DELAY);

		ret = fg_read_sbs_word(sm, CMD_OEMFLAG, &oemflag);
		if (ret < 0)
			goto fg_gauge_runstate_check_End;

		//20220106, Ethan
		//socFlag = (u16)!!(temperature < TEMPER_MIN_RESET);
		if (sm->batt_curr <= 0) {
			socFlag |= !!((sm->batt_volt > VOLT_MIN_RESET) && (sm->batt_soc < SOC_MIN_RESET));
			socFlag |= !!((sm->batt_volt > (3850 * MA_TO_UA)) && (sm->batt_soc < 3));
			socFlag |= !!((sm->batt_soc == 0) && ((sm->batt_volt - sm->terminate_voltage) > DELTA_VOLT));
		}

		if (socFlag) { 
			ret = fg_gauge_unseal(sm);
			if (ret < 0)
				goto fg_gauge_runstate_check_End;

			ret = fg_write_sbs_word(sm, CMD_ALTMAC, (u16)CMD_RESET);
			if (ret < 0)
				goto fg_gauge_runstate_check_End;
			HOST_DELAY(DELAY_RESET);
		}

		 /* por with poor connection */
		qenFlag = !!((oemflag & CMD_MASK_OEM_GAUGEEN) != CMD_MASK_OEM_GAUGEEN); /* Gauge Un-enable */
		if (qenFlag) {  //Gauge Disable. Re-enable gauge
			ret = fg_gauge_unseal(sm);
			if (ret < 0)
				goto fg_gauge_runstate_check_End;

			ret = fg_write_sbs_word(sm, CMD_ALTMAC, (u16)CMD_ENABLE_GAUGE);
			if (ret < 0)
				goto fg_gauge_runstate_check_End;
			HOST_DELAY(DELAY_ENABLE_GAUGE);
		}

		ltFlag = !!((oemflag & CMD_MASK_OEM_LIFETIMEEN) != CMD_MASK_OEM_LIFETIMEEN);
		if (ltFlag) { //Lifetime Disable. Re-enable lifetime
			ret = fg_gauge_unseal(sm);
			if (ret < 0)
				goto fg_gauge_runstate_check_End;

			ret = fg_write_sbs_word(sm, CMD_ALTMAC, (u16)CMD_ENABLE_LIFETIME);
			if (ret < 0)
				goto fg_gauge_runstate_check_End;
			HOST_DELAY(DELAY_ENABLE_GAUGE);
		}

		ret = 0;
		break;

fg_gauge_runstate_check_End:
		HOST_DELAY(CMD_SBS_DELAY << 1);
	}

	pr_err("fg_gauge_runstate_check: soc=%d, volt=%d, termVolt=%d, OEMFlag=0x%04X, QEN_FLAG=%u, SOC_FLAG=%u, LifeTime_Flag=%u\n", 
		sm->batt_soc, sm->batt_volt, sm->terminate_voltage, oemflag, qenFlag, socFlag, ltFlag);

	fg_gauge_seal(sm);
	return ret;
}

static int Check_Chip_Version(struct sh_fg_chip* sm)
{
	struct device* dev = &sm->client->dev;
	struct device_node* np = dev->of_node;
	u32 version_main, version_date, version_afi, version_ts;
	int ret = CHECK_VERSION_ERR;
	u16 temp;
	int date;
	/* 20211025, Ethan. IAP Fail Check */
	struct sh_decoder decoder;
	u8 iap_read[IAP_READ_LEN];

	/* 20220216, Ethan. AFI Version 2 */
	u8 buf[32];
	u32 version_afi2 = 0, chip_afi, chip_afi2;
	u16 sector_flag = 0; //20220315, Ethan

	/* battery_params node*/
	np = of_find_node_by_name(of_node_get(np), "battery_params");
	if (np == NULL) {
		pr_err("Check_Chip_Version: Cannot find child node \"battery_params\"\n");
		return CHECK_VERSION_ERR;
	}

	of_property_read_u32(np, "version_main", &version_main);
	of_property_read_u32(np, "version_date", &version_date);
	of_property_read_u32(np, "version_afi", &version_afi);
	of_property_read_u32(np, "version_afi2", &version_afi2); /* 20220216, Ethan. AFI Version 2 */
	of_property_read_u32(np, "version_ts", &version_ts);
	of_property_read_u8(np, "iap_twiadr", &decoder.addr); /* 20211025, Ethan */

	/* 20220216, Ethan. AFI Version 2 */
	pr_err("Check_Chip_Version: main=0x%04X, date=0x%08X, afi=0x%04X, afi2=0x%04X, ts=0x%04X\n", version_main, version_date, version_afi, version_afi2, version_ts);

	/* 20211025, Ethan. IAP Fail Check. iap addr may differ from normal addr */
	decoder.reg = (u8)CMD_IAPSTATE_CHECK;
	decoder.length = IAP_READ_LEN;
	if ((fg_decode_iic_read(sm, &decoder, iap_read) >= 0) && (iap_read[0] != 0) && (iap_read[1] != 0)) {
		pr_err("Check_Chip_Version: ic is in iap mode, force update all\n");
		ret = CHECK_VERSION_WHOLE_CHIP;
		goto Check_Chip_Version_End;
	}
	HOST_DELAY(CMD_SBS_DELAY); /* 20211029, Ethan */

	if (fg_gauge_unseal(sm) < 0) { /* 20211122, Ethan. Gauge Enable */
		ret = CHECK_VERSION_ERR;
		goto Check_Chip_Version_End;
	}

	/* check fw version. FW update must update afi(for iap check flag) */
	if (fg_read_sbs_word(sm, CMD_FWVERSION_MAIN, &temp) < 0) {
		ret = CHECK_VERSION_ERR;
		goto Check_Chip_Version_End;
	}
	pr_err(" Chip_Version: ic main=0x%04X \n", temp);

	if (temp < version_main) {
		ret = CHECK_VERSION_WHOLE_CHIP;
		goto Check_Chip_Version_End;
	} else if (temp > version_main)
		ret = CHECK_VERSION_OK;
	else { /* version equal, check date */
		if (fg_read_sbs_word(sm, CMD_FWDATE1, &temp) < 0) {
			ret = CHECK_VERSION_ERR;
			goto Check_Chip_Version_End;
		}
		msleep(CMD_SBS_DELAY);
		date = (u32)temp << 16;

		if (fg_read_sbs_word(sm, CMD_FWDATE2, &temp) < 0) {
			ret = CHECK_VERSION_ERR;
			goto Check_Chip_Version_End;
		}
		date |= (temp & FW_DATE_MASK);
		pr_err(" Chip_Version: ic date=0x%08X \n", date);
		if (date < version_date) {
			ret = CHECK_VERSION_WHOLE_CHIP;
			goto Check_Chip_Version_End;
		} else
			ret = CHECK_VERSION_OK;
	}

	/* check afi. 20220216, Ethan. AFI Version 2 */
	if (fg_read_block(sm, CMD_AFI_STATIC_SUM, 32, buf) < 0) {
		ret = CHECK_VERSION_ERR;
		goto Check_Chip_Version_End;
	}
	//20220315, Ethan
	if (fg_read_sbs_word(sm, CMD_SECTOR_FLAG, &sector_flag) < 0) {
		ret = CHECK_VERSION_ERR;
		goto Check_Chip_Version_End;
	}

	chip_afi = BUF2U16_LT(&buf[0]);
	chip_afi2 = BUF2U16_LT(&buf[6]);
	pr_err(" Chip_Version: ic afi=0x%04X, ic afi2=0x%04X, sector_flag=0x%04X\n ", chip_afi, chip_afi2, sector_flag); //20220315, Ethan
	if ((sector_flag & MASK_SECTOR_FLAG) != 0) //20220315, Ethan
		ret |= CHECK_VERSION_AFI;
	if (chip_afi2 == 0)
	{
		if (chip_afi != version_afi)
			ret |= CHECK_VERSION_AFI;
	}
	else if (chip_afi2 != version_afi2)
		ret |= CHECK_VERSION_AFI;

	/* check TS */
	if (fg_read_sbs_word(sm, CMD_TS_VER, &temp) < 0) {
		ret = CHECK_VERSION_ERR;
		goto Check_Chip_Version_End;
	}
	pr_err(" Chip_Version: ic ts=0x%04X \n", temp);
	if (temp != version_ts)
		ret |= CHECK_VERSION_TS;

Check_Chip_Version_End:
	fg_gauge_seal(sm); /* 20211122, Ethan. Gauge Enable */
	return ret;
}
#endif
int file_decode_process(struct sh_fg_chip* sm, char* profile_name)
{
	struct device* dev = &sm->client->dev;
	struct device_node* np = dev->of_node;
	u8* pBuf = NULL;
	u8* pBuf_Read = NULL;
	char strDebug[FILEDECODE_STRLEN];
	int buflen;
	int wait_ms;
	int i, j;
	int line_length;
	int result = -1;
	int retry;

	pr_err("file_decode_process: start\n");

	/* battery_params node*/
	np = of_find_node_by_name(of_node_get(np), "battery_params");
	if (np == NULL) {
		pr_err("file_decode_process: Cannot find child node \"battery_params\"\n");
		return -EINVAL;
	}

	buflen = of_property_count_u8_elems(np, profile_name);
	pr_err("file_decode_process: ele_len=%d, key=%s \n", buflen, profile_name);

	pBuf = (u8*)devm_kzalloc(dev, buflen, 0);
	pBuf_Read = (u8*)devm_kzalloc(dev, BUF_MAX_LENGTH, 0);

	if ((pBuf == NULL) || (pBuf_Read == NULL)) {
		result = ERRORTYPE_ALLOC;
		pr_err("file_decode_process: kzalloc error\n");
		goto main_process_error;
	}

	result = of_property_read_u8_array(np, profile_name, pBuf, buflen);
	if (result) {
		pr_err("file_decode_process: read dts fail %s\n", profile_name);
		goto main_process_error;
	}
	print_buffer(strDebug, sizeof(char) * FILEDECODE_STRLEN, pBuf, 32);
	pr_err("file_decode_process: first data=%s\n", strDebug);

	i = 0;
	j = 0;
	while (i < buflen) {
		/* delay: b0: operate, b1: 2, b2-b3: time, big-endian */
		/* other: b0: operate, b1: TWIADR, b2: reg, b3: data_length, b4...end: item */
		if (pBuf[i + INDEX_TYPE] == OPERATE_WAIT) {
			pr_err("OPERATE_WAIT\n");
			wait_ms = ((int)pBuf[i + INDEX_WAIT_HIGH] * 256) + pBuf[i + INDEX_WAIT_LOW];

			if (pBuf[i + INDEX_WAIT_LENGTH] == 2) {
				HOST_DELAY(wait_ms); /* 20211029, Ethan */
				i += LINELEN_WAIT;
			} else {
				print_buffer(strDebug, sizeof(char) * FILEDECODE_STRLEN, &pBuf[i + INDEX_TYPE], 32);
				pr_err("file_decode_process wait error! index=%d, str=%s\n", i, strDebug);
				result = ERRORTYPE_LINE;
				goto main_process_error;
			}
		} else if (pBuf[i + INDEX_TYPE] == OPERATE_READ) {
			pr_err("OPERATE_READ\n");
			line_length = pBuf[i + INDEX_LENGTH];
			if (line_length <= 0) {
				result = ERRORTYPE_LINE;
				goto main_process_error;
			}

			/* 20211026, Ethan. IAP addr may differ from default addr */
			/*if (fg_read_block(sm, pBuf[i + INDEX_REG], line_length, pBuf_Read) < 0) { */
			if (fg_decode_iic_read(sm, (struct sh_decoder*)&pBuf[i + INDEX_ADDR], pBuf_Read) < 0) {
				print_buffer(strDebug, sizeof(char) * FILEDECODE_STRLEN, &pBuf[i + INDEX_TYPE], 32);
				pr_err("file_decode_process read error! index=%d, str=%s\n", i, strDebug);
				result = ERRORTYPE_COMM;
				goto main_process_error;
			}

			i += LINELEN_READ;
		} else if (pBuf[i + INDEX_TYPE] == OPERATE_COMPARE) {
			pr_err("OPERATE_COMPARE\n");
			line_length = pBuf[i + INDEX_LENGTH];
			if (line_length <= 0) {
				result = ERRORTYPE_LINE;
				goto main_process_error;
			}

			for (retry = 0; retry < COMPARE_RETRY_CNT; retry++) {
				/* 20211026, Ethan. IAP addr may differ from default addr */
				/*if (fg_read_block(sm, pBuf[i + INDEX_REG], line_length, pBuf_Read) < 0) { */
				if (fg_decode_iic_read(sm, (struct sh_decoder*)&pBuf[i + INDEX_ADDR], pBuf_Read) < 0) {
					print_buffer(strDebug, sizeof(char) * FILEDECODE_STRLEN, &pBuf[i + INDEX_TYPE], 32);
					pr_err("file_decode_process compare_read error! index=%d, str=%s\n", i, strDebug);
					result = ERRORTYPE_COMM;
					goto file_decode_process_compare_loop_end;
				}

				print_buffer(strDebug, sizeof(char) * FILEDECODE_STRLEN, pBuf_Read, line_length);
				pr_debug("file_decode_process loop compare: IC read=%s\n", strDebug);

				result = 0;
				for (j = 0; j < line_length; j++) {
					if (pBuf[INDEX_DATA + i + j] != pBuf_Read[j]) {
						result = ERRORTYPE_COMPARE;
						break;
					}
				}

				if (result == 0)
					break;

				/* compare fail */
				print_buffer(strDebug, sizeof(char) * FILEDECODE_STRLEN, &pBuf[i + INDEX_TYPE], 32);
				pr_err("file_decode_process compare error! index=%d, retry=%d, host=%s\n", i, retry, strDebug);
				print_buffer(strDebug, sizeof(char) * FILEDECODE_STRLEN, pBuf_Read, 32);
				pr_err("ic=%s\n", strDebug);

			file_decode_process_compare_loop_end:
				HOST_DELAY(COMPARE_RETRY_WAIT); /* 20211029, Ethan */
			}

			if (retry >= COMPARE_RETRY_CNT) {
				result = ERRORTYPE_COMPARE; /* 20211125, Ethan */
				goto main_process_error;
			}

			i += LINELEN_COMPARE + line_length;
		} else if (pBuf[i + INDEX_TYPE] == OPERATE_WRITE) {
			pr_err("OPERATE_WRITE\n");
			line_length = pBuf[i + INDEX_LENGTH];
			if (line_length <= 0) {
				result = ERRORTYPE_LINE;
				goto main_process_error;
			}

			/* 20211026, Ethan. IAP addr may differ from default addr */
			/* if (fg_write_block(sm, pBuf[i + INDEX_REG], line_length, &pBuf[i + INDEX_DATA]) != 0) { */
			if (fg_decode_iic_write(sm, (struct sh_decoder*)&pBuf[i + INDEX_ADDR]) != 0) {
				print_buffer(strDebug, sizeof(char) * FILEDECODE_STRLEN, &pBuf[i + INDEX_TYPE], 32);
				pr_err("file_decode_process write error! index=%d, str=%s\n", i, strDebug);
				result = ERRORTYPE_COMM;
				goto main_process_error;
			}

			i += LINELEN_WRITE + line_length;
		} else {
			pr_err("OPERATE_ no active\n");
			result = ERRORTYPE_LINE;
			goto main_process_error;
		}
	}
	result = ERRORTYPE_NONE;

main_process_error:
	pr_err("file_decode_process end: result=%d\n", result);
	return result;
}

static int fg_battery_parse_dt(struct sh_fg_chip* sm)
{
	struct device* dev = &sm->client->dev;
	struct device_node* np = dev->of_node;

	BUG_ON(dev == 0);
	BUG_ON(np == 0);

	/* battery_params node*/
	np = of_find_node_by_name(of_node_get(np), "battery_params");
	if (np == NULL) {
		pr_info("Cannot find child node \"battery_params\"\n");
		return -EINVAL;
	}

	return 0;
}

static void fg_monitor_workfunc(struct work_struct* work) //20220727, Sinowealth
{
	struct sh_fg_chip* sm = container_of(work, struct sh_fg_chip, monitor_work.work);
	int interval = GAUGE_REFRESH_SPAN;

	pr_err("fg_monitor_workfunc invoke!\n");

//	fg_read_gaugeinfo_block(sm);
	fg_read_soc(sm);
	fg_read_volt(sm);
	fg_read_current(sm);
	fg_read_IntTemperature(sm);
	fg_read_ExtTemperature(sm);
	fg_read_soc_h(sm);
	fg_read_DesignCapacity(sm);
	fg_read_ReadCycleCount(sm);

	fg_read_ReadFCC(sm);
	//fg_SH366100_Unseal(sm);
	SH366100_ReadManufactureData(sm);
	//msleep(5);
	SH366100_ReadFirstTimeToSeriaNo(sm);
	//msleep(5);
	SH366100_Read_SNCode_To_BlockA(sm);
	//msleep(5);
	pr_err("\r\n");
	schedule_delayed_work(&sm->monitor_work, interval * HZ);
}

bool hal_fg_init(struct i2c_client* client)
{
	struct sh_fg_chip* sm = i2c_get_clientdata(client);

	pr_info("sh366100 hal_fg_init...\n");
	mutex_lock(&sm->data_lock);
	if (client->dev.of_node) {
		/* Load battery data from DTS*/
		fg_battery_parse_dt(sm);
	}

	mutex_unlock(&sm->data_lock);
	pr_info("hal fg init OK\n");
	return true;
}

//static int sh_fg_probe(struct i2c_client* client, const struct i2c_device_id* id)
static int sh_fg_probe(struct i2c_client* client)
{
	//int ret = -1;
	//int version_ret;
	//int retry;
	const struct i2c_device_id *id = i2c_client_get_device_id(client);
	struct sh_fg_chip* sm;
	u32* regs;
	pr_err("2022.10.17 %s: start,i2c addr is 0x%02x\n", __func__,client->addr);

	client->addr = 0x55;
	pr_info("enter , sh_fg_probe  i2c addr is 0x%02x\n",client->addr);
	sm = devm_kzalloc(&client->dev, sizeof(*sm), GFP_KERNEL);

	if (!sm)
		return -ENOMEM;

	sm->dev = &client->dev;
	sm->client = client;
	sm->chip = id->driver_data;

	sm->batt_soc = -ENODATA;
	sm->batt_volt = -ENODATA;
	sm->batt_curr = -ENODATA;
	/* 20211029, Ethan. */
	/* sm->fake_soc = -EINVAL; */
	/* sm->fake_temp = -EINVAL; */

	regs = sh366100_regs;

	memcpy(sm->regs, regs, NUM_REGS * sizeof(u32));

	i2c_set_clientdata(client, sm);

	mutex_init(&sm->i2c_rw_lock);
	mutex_init(&sm->data_lock);
	mutex_init(&sm->cali_lock); //20220807, Sinowealth

	fg_read_soc(sm);
	fg_read_volt(sm);
	fg_read_current(sm);
	fg_read_IntTemperature(sm);
	fg_read_ExtTemperature(sm);

//	fg_read_gaugeinfo_block(sm); /* 20211115. LJQ Debug */

	/* 20211013, Ethan. Firmware Update */
//	version_ret = Check_Chip_Version(sm);
//	if (version_ret == CHECK_VERSION_ERR) {
//		pr_err("Probe: Check version error!\n");
//	} else if (version_ret == CHECK_VERSION_OK) {
//		pr_err("Probe: Check version ok!\n");
//	} else {
//		pr_err("Probe: Check version update: %X\n", version_ret);
//
//		ret = ERRORTYPE_NONE; /* 20220517, Ethan */
//		if (version_ret & CHECK_VERSION_FW) {
//			pr_err("Probe: Firmware Update start\n");
//			for (retry = 0; retry < FILE_DECODE_RETRY; retry++) {
//				ret = file_decode_process(sm, "sinofs_image_data");
//				if (ret == ERRORTYPE_NONE)
//					break;
//				HOST_DELAY(FILE_DECODE_DELAY); /* 20211029, Ethan */
//			}
//			pr_err("Probe: Firmware Update end, ret=%d\n", ret);
//		}
//		if (ret != ERRORTYPE_NONE) /* 20220517, Ethan */
//			goto sh_fg_probe_fwupdate_end;
//
//		if (version_ret & CHECK_VERSION_TS) {
//			pr_err("Probe: TS Update start\n");
//			for (retry = 0; retry < FILE_DECODE_RETRY; retry++) {
//				ret = file_decode_process(sm, "sinofs_ts_data");
//				if (ret == ERRORTYPE_NONE)
//					break;
//				HOST_DELAY(FILE_DECODE_DELAY); /* 20211029, Ethan */
//			}
//			pr_err("Probe: TS Update end, ret=%d\n", ret);
//		}
//		if (ret != ERRORTYPE_NONE) /* 20220517, Ethan */
//			goto sh_fg_probe_fwupdate_end;
//
//		if (version_ret & CHECK_VERSION_AFI) {
//			pr_err("Probe: AFI Update start\n");
//			for (retry = 0; retry < FILE_DECODE_RETRY; retry++) {
//				ret = file_decode_process(sm, "sinofs_afi_data");
//				if (ret == ERRORTYPE_NONE)
//					break;
//				HOST_DELAY(FILE_DECODE_DELAY); /* 20211029, Ethan */
//			}
//			pr_err("Probe: AFI Update end, ret=%d\n", ret);
//		}
//		if (ret != ERRORTYPE_NONE) /* 20220517, Ethan */
//			goto sh_fg_probe_fwupdate_end;
//	}
//sh_fg_probe_fwupdate_end: /* 20220517, Ethan */
//
//	if (fg_gauge_runstate_check(sm) < 0) { /* 20211122, Ethan. Gauge Enable */
//		pr_err("Failed to Enable Gauge\n");
//		goto err_0;
//	}
//
//	if (!hal_fg_init(client)) {
//		pr_err("Failed to Initialize Fuelgauge\n");
//		goto err_0;
//	}
//
	INIT_DELAYED_WORK(&sm->monitor_work, fg_monitor_workfunc); //20220727, Sinowealth
	schedule_delayed_work(&sm->monitor_work, 1 * HZ); //20220727, Sinowealth

	st_sm = sm;

	//fg_SH366100_Unseal(sm);
	// SH366100_WriteManufactureData(sm);
	// msleep(1000);
	//SH366100_WriteFirstTimeToSeriaNo(sm);
//	msleep(1000);
	// SH366100_Write_SNCode_To_BlockA(sm);
	// msleep(1000);
	pr_info("sh sh_fg_probe fuel gauge probe successfully, %s\n", device2str[sm->chip]);
	pr_err("2022.10.17 %s: end\n", __func__);

	return 0;

//err_0:
//	return ret;
}

//static int sh_fg_remove(struct i2c_client* client)
static void sh_fg_remove(struct i2c_client* client)
{
	struct sh_fg_chip* sm = i2c_get_clientdata(client);

	mutex_destroy(&sm->cali_lock); //20220807, Sinowealth
	mutex_destroy(&sm->data_lock);
	mutex_destroy(&sm->i2c_rw_lock);

	//return 0;
	return;
}

static void sh_fg_shutdown(struct i2c_client* client)
{
	pr_info("sm fuel gauge driver shutdown!\n");
}

static const struct of_device_id sh_fg_match_table[] = {
    {
	.compatible = "sh,sh366100",
    },
    {},
};
MODULE_DEVICE_TABLE(of, sh_fg_match_table);

static const struct i2c_device_id sh_fg_id[] = {
    {"sh366100", 0},
    {},
};
MODULE_DEVICE_TABLE(i2c, sh_fg_id);

static struct i2c_driver sh_fg_driver = {
    .driver = {
	.name = "sh366100",
	.owner = THIS_MODULE,
	.of_match_table = sh_fg_match_table,
    },
    .id_table = sh_fg_id,
    .probe = sh_fg_probe,
    .remove = sh_fg_remove,
    .shutdown = sh_fg_shutdown,
};

module_i2c_driver(sh_fg_driver);

MODULE_DESCRIPTION("SH SH366100 Gauge Driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Sinowealth");
