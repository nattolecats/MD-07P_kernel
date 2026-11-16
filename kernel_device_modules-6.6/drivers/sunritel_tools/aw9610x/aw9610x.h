#ifndef _AW9610X_H_
#define _AW9610X_H_

#include <linux/regmap.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/hrtimer.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#include <linux/leds.h>
#include <linux/regulator/consumer.h>

/*Open the macro definition using the regulator power mode*/
//#define AW_POWER_ON

//#define AW_USB_PLUG_CAIL

#define	AW_INPUT_TRIGGER_TH1
#define	AW_INPUT_TRIGGER_TH2
#define	AW_INPUT_TRIGGER_TH3

#define AW9610X_CHIP_ID				(0xa961)
#define AW_SAR_SUCCESS				(0)
#define AW_VCC_MIN_UV				(1700000)
#define AW_VCC_MAX_UV				(3600000)
#define AW_SPE_REG_NUM				(8)
#define AW_CLA1_SPE_REG_NUM			(6)
#define AW_SPE_REG_DWORD			(8)
#define AW_DATA_PROCESS_FACTOR			(1024)
#define AW9610X_CPU_WORK_MASK			(1)
#define AW9610X_CHIP_MIN_VOLTAGE		(1600000)
#define AW_TURE					(1)
#define AW_FALSE				(0)
#define	 AW_ERR					(1)
#define	 AW_OK					(0)

#define AW_CHIP_AW9610XA			(0x03000b00)

#define FW_ACTION_HOTPLUG 1


#define AWLOGD(dev, format, arg...) \
	do {\
		 dev_printk(KERN_DEBUG, dev, \
			"[%s:%d] "format"\n", __func__, __LINE__, ##arg);\
	} while (0)

#define AWLOGI(dev, format, arg...) \
	do {\
		dev_printk(KERN_INFO, dev, \
			"[%s:%d] "format"\n", __func__, __LINE__, ##arg);\
	} while (0)

#define AWLOGE(dev, format, arg...) \
	do {\
		 dev_printk(KERN_ERR, dev, \
			"[%s:%d] "format"\n", __func__, __LINE__, ##arg);\
	} while (0)

/**********************************************
* cfg load situation
**********************************************/
enum aw9610x_cfg_situ {
	AW_CFG_UNLOAD,
	AW_CFG_LOADED,
};

/**********************************************
*spereg cali flag
**********************************************/
enum aw9610x_cali_flag {
	AW_NO_CALI,
	AW_CALI,
};

enum aw9610x_sar_vers {
	AW9610X = 2,
	AW9610XA = 6,
	AW9610XB = 0xa,
};

enum aw9610x_operation_mode {
	AW9610X_ACTIVE_MODE = 1,
	AW9610X_SLEEP_MODE,
	AW9610X_DEEPSLEEP_MODE,
	AW9610XB_DEEPSLEEP_MODE,
};

enum aw9610x_progxirq_stat {
	PROG0IRQ = 1,
	PROG1IRQ = 2,
	PROG2IRQ = 4,
	PROG3IRQ = 8,
};

/**********************************************
*spereg addr offset
**********************************************/
enum aw9610x_spereg_addr_offset {
	AW_CL1SPE_CALI_OS = 20,
	AW_CL1SPE_DEAL_OS = 60,
	AW_CL2SPE_CALI_OS = 4,
	AW_CL2SPE_DEAL_OS = 4,
};

/**********************************************
 *the flag of i2c read/write
 **********************************************/
enum aw9610x_function_flag {
	AW9610X_FUNC_OFF,
	AW9610X_FUNC_ON,
};

/**********************************************
 *the flag of i2c read/write
 **********************************************/
enum aw9610x_i2c_flags {
	AW9610X_I2C_WR,
	AW9610X_I2C_RD,
};

/*********************************************************
* aw9610x error flag:
* @AW_MALLOC_FAILED: malloc space failed.
* @AW_CHIPID_FAILED: the chipid is error.
* @AW_IRQIO_FAILED: irq gpio invalid.
* @AW_IRQ_REQUEST_FAILED: irq requst failed.
* @AW_INPUT_ALLOCATE_FILED : inpt malloc failed.
* @AW_INPUT_REGISTER_FAILED : inpt_dev register failed.
* @AW_CFG_LOAD_TIME_FAILED : cfg situation not confirmed.
* @AW_TRIM_ERROR : the chip no trim.
* @AW_IRQGPIO_FAILED : irq gpio is invalid.
* @AW_MULTIPLE_SAR_FAILED : sar-num is error in aw9610x.dtsi
**********************************************************/
enum aw9610x_err_flags {
	AW_MALLOC_FAILED = 200,
	AW_CHIPID_FAILED,
	AW_IRQIO_FAILED,
	AW_IRQ_REQUEST_FAILED,
	AW_INPUT_ALLOCATE_FILED,
	AW_INPUT_REGISTER_FAILED,
	AW_CFG_LOAD_TIME_FAILED,
	AW_TRIM_ERROR,
	AW_IRQGPIO_FAILED,
	AW_MULTIPLE_SAR_FAILED,
	AW_VERS_ERR,
};

/**********************************************
* mutiple sar define
**********************************************/
enum aw9610x_multiple_sar {
	AW_SAR0,
	AW_SAR1,
	AW_SAR_MAX,
};

/**********************************************
* mutiple channel define
**********************************************/
enum aw9610x_ichannel {
	AW_CHANNEL0,
	AW_CHANNEL1,
	AW_CHANNEL2,
	AW_CHANNEL3,
	AW_CHANNEL4,
	AW_CHANNEL5,
	AW_CHANNEL_MAX,
};

/************************************************************
* Interrupts near or far from the threshold will be triggered
*************************************************************/
enum aw9610x_irq_trigger_position {
	FAR,
	TRIGGER_TH0,
	TRIGGER_TH1 = 0x03,
	TRIGGER_TH2 = 0x07,
	TRIGGER_TH3 = 0x0f,
};


struct aw_i2c_package {
	uint8_t addr_bytes;
	uint8_t data_bytes;
	uint8_t reg_num;
	uint8_t init_addr[4];
	uint8_t *p_reg_data;
};

struct aw_channels_info {
	uint8_t used;
	uint32_t last_channel_info;
	struct input_dev *input;
	uint8_t name[20];
};

#ifdef AW_PINCTRL_ON
struct aw9610x_pinctrl {
	struct pinctrl *pinctrl;
	struct pinctrl_state *default_sta;
	struct pinctrl_state *int_out_high;
	struct pinctrl_state *int_out_low;
};
#endif

struct aw9610x {
	uint8_t cali_flag;
	uint8_t power_prox;
	uint8_t vers;
	uint8_t channel;

	uint32_t sar_num;
	int32_t irq_gpio;
	int32_t ldo_en;
	int32_t to_irq;
	uint32_t irq_status;
	uint32_t hostirqen;
	uint32_t first_irq_flag;
	uint32_t mode;
	uint32_t old_mode;
	uint32_t channel_use_flag;

	bool pwprox_dete;
	bool firmware_flag;
	bool satu_release;
	bool power_enable;

	struct work_struct ps_notify_work;
	struct notifier_block ps_notif;
	bool ps_is_present;
#ifdef AW_PINCTRL_ON
	struct aw9610x_pinctrl pinctrl;
#endif

	struct delayed_work cfg_work;
	struct i2c_client *i2c;
	struct device *dev;
	struct delayed_work dworker; /* work struct for worker function */
	struct aw_bin *aw_bin;
	struct aw_i2c_package aw_i2c_package;
	struct aw_channel *aw_channel;
	struct regulator *vcc;
	struct aw_channels_info *channels_arr;

	uint8_t satu_flag[6];
	uint32_t spedata[8];
	uint32_t satu_data[6];
	uint32_t nvspe_data[8];
	uint8_t chip_name[9];
	uint8_t cfg_name[20];
	uint8_t cfg_vers[2];
	uint8_t chip_type[9];
};

struct aw9610x_cfg {
	int len;
	unsigned int data[];
};

/******************************************************
* Register Detail
******************************************************/
uint32_t aw9610x_reg_default[] = {
	0x0000,0x00000707,
	0x0004,0x00000014,
	0x0008,0x0017C11E,
	0x000C,0x05000000,
	0x0010,0x00086FFD,
	0x0014,0x24380009,
	0x0018,0xD81C0207,
	0x001C,0xFF000000,
	0x0020,0x00382400,
	0x0024,0x00086FF7,
	0x0028,0x0C3D0009,
	0x002C,0xD81C0207,
	0x0030,0xFF000000,
	0x0034,0x003D0C00,
	0x0038,0x00086FDF,
	0x003C,0x7D3B0009,
	0x0040,0xD81C0207,
	0x0044,0xFF000000,
	0x0048,0x003B7D00,
	0x004C,0x00066F7F,
	0x0050,0xE9310009,
	0x0054,0xD81C0207,
	0x0058,0xFF000000,
	0x005C,0x0031E900,
	0x0060,0x00050000,
	0x0064,0x00000009,
	0x0068,0xD81C0207,
	0x006C,0xFF000000,
	0x0070,0x00000000,
	0x0074,0x00050000,
	0x0078,0x00000009,
	0x007C,0xD81C0207,
	0x0080,0xFF000000,
	0x0084,0x00000000,
	0x00A0,0xE2400000,
	0x00A4,0x00000000,
	0x00A8,0x0003E8EE,
	0x00AC,0x00000000,
	0x00B0,0x00000500,
	0x00B8,0x00003A98,
	0x00BC,0x00000000,
	0x00C0,0x00000000,
	0x00C4,0x00000000,
	0x00C8,0x00000000,
	0x00CC,0x00000000,
	0x00D0,0x00000000,
	0x00D4,0x00000000,
	0x00D8,0x00000000,
	0x00DC,0xE2400000,
	0x00E0,0x00000000,
	0x00E4,0x000008EE,
	0x00E8,0x00000000,
	0x00EC,0x00000500,
	0x00F4,0x00003A98,
	0x00F8,0x00000000,
	0x00FC,0x00000000,
	0x0100,0x00000000,
	0x0104,0x00000000,
	0x0108,0x00000000,
	0x010C,0x00000000,
	0x0110,0x00000000,
	0x0114,0x00000000,
	0x0118,0xE2400000,
	0x011C,0x00000000,
	0x0120,0x000008EE,
	0x0124,0x00000000,
	0x0128,0x00000500,
	0x0130,0x00003A98,
	0x0134,0x00000000,
	0x0138,0x00000000,
	0x013C,0x00000000,
	0x0140,0x00000000,
	0x0144,0x00000000,
	0x0148,0x00000000,
	0x014C,0x00000000,
	0x0150,0x00000000,
	0x0154,0xE4400000,
	0x0158,0x00000000,
	0x015C,0x000008D2,
	0x0160,0x00000000,
	0x0164,0x00001500,
	0x016C,0x00005FFF,
	0x0170,0x00000000,
	0x0174,0x00000000,
	0x0178,0x00000000,
	0x017C,0x00000000,
	0x0180,0x00000000,
	0x0184,0x00000000,
	0x0188,0x00000000,
	0x018C,0x00000000,
	0x0190,0xE0400000,
	0x0194,0x00000000,
	0x0198,0x000008D2,
	0x019C,0x00000000,
	0x01A0,0x00000000,
	0x01A8,0x00000000,
	0x01AC,0x00000000,
	0x01B0,0x00000000,
	0x01B4,0x00000000,
	0x01B8,0x00000000,
	0x01BC,0x00000000,
	0x01C0,0x00000000,
	0x01C4,0x00000000,
	0x01C8,0x00000000,
	0x01CC,0xE0400000,
	0x01D0,0x00000000,
	0x01D4,0x000008D2,
	0x01D8,0x00000000,
	0x01DC,0x00000000,
	0x01E4,0x00000000,
	0x01E8,0x00000000,
	0x01EC,0x00000000,
	0x01F0,0x00000000,
	0x01F4,0x00000000,
	0x01F8,0x00000000,
	0x01FC,0x00000000,
	0x0200,0x00000000,
	0x0204,0x00000000,
	0x0208,0x00000008,
	0x020C,0x0000000D,
	0x41FC,0x00000000,
	0x4400,0x00000000,
	0x4410,0x00000000,
	0x4420,0x00000000,
	0x4430,0x00000000,
	0x4440,0x00000000,
	0x4450,0x00000000,
	0x4460,0x00000000,
	0x4470,0x00000000,
	0xF084,0x00000206,
	0xF800,0x00000000,
	0xF804,0x00002E00,
	0xF8C0,0x00000000,
	0xF8D0,0x00000001,
	0xF8D4,0x00000000,
	0xFF00,0x00000301,
	0xFF0C,0x01000000,
	0xFFE0,0x00000000,
	0xFFF0,0x00000000,
	0xFFF4,0x00004011,
	0xFFF8,0x00000000,
	0xFFFC,0x00000000,
	0x009C,0x00070700,
};
#endif
