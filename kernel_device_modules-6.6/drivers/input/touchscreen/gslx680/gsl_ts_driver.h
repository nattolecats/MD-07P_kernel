#ifndef __GSL_TS_DRIVER_H__
#define __GSL_TS_DRIVER_H__
/*********************************/
//#define TPD_HAVE_BUTTON		//按键宏
#define GSL_ALG_ID			//有没有id算法
#define GSL_COMPATIBLE_CHIP	//芯片兼容宏
#define GSL_THREAD_EINT		//线程中断
#define GSL_DEBUG			//调试
#define TPD_PROC_DEBUG		//adb调试
#define GSL_TIMER			//定时器宏

#ifdef CONFIG_TOUCHSCREEN_MTK_GSLX680_64_PROXIMITY
#define TPD_PROXIMITY		//距离传感器宏
#endif
#ifdef VANZO_TOUCHPANEL_GESTURES_SUPPORT
#define GSL_GESTURE_WAKEUP 1
#else
#define GSL_GESTURE_WAKEUP 0
#endif

//#define GSL_GESTURE

//#define GSL_GPIO_IDT_TP	//GPIO兼容	
//#define GSL_DRV_WIRE_IDT_TP	//驱动线兼容

#define GSL9XX_VDDIO_1800    0

#define TPD_POWER_VTP28_USE_VCAMA   //[add for mx2116 2015-11-03]


#define GSL_PAGE_REG    0xf0
#define GSL_CLOCK_REG   0xe4
#define GSL_START_REG   0xe0
#define POWE_FAIL_REG   0xbc
#define TOUCH_INFO_REG  0x80
#define TPD_DEBUG_TIME	0x20130424
struct gsl_touch_info
{
	int x[10];
	int y[10];
	int id[10];
	int finger_num;	
};

struct gsl_ts_data {
	struct i2c_client *client;
	struct workqueue_struct *wq;
	struct work_struct work;
	unsigned int irq;
	//struct early_suspend pm;
};

/*button*/
#ifdef TPD_HAVE_BUTTON
	#ifdef JZHK_CTP_CTL_J517_FWVGA_BP_P010
		#define TPD_KEY_COUNT           3
		#define TPD_KEYS                {KEY_MENU,KEY_HOMEPAGE,KEY_BACK}
		#define TPD_KEYS_DIM            {{120,1000,20,20},{240,1000,20,20},{360,1000,20,20}}
	#else
		#define TPD_KEY_COUNT           3
		#define TPD_KEYS                {KEY_MENU,KEY_HOMEPAGE,KEY_BACK}
		#define TPD_KEYS_DIM            {{80,1030,20,20},{240,1030,20,20},{460,1030,20,20}}
	#endif
#endif


#ifdef GSL_ALG_ID 
extern unsigned int gsl_mask_tiaoping(void);
extern unsigned int gsl_version_id(void);
extern void gsl_alg_id_main(struct gsl_touch_info *cinfo);
extern void gsl_DataInit(int *ret);


#endif
/* Fixme mem Alig */
struct fw_data
{
    u32 offset : 8;
    u32 : 0;
    u32 val;
};
#ifdef GSL_DRV_WIRE_IDT_TP
#include "gsl_idt_tp.h"
#endif

#ifdef JZHK_CTP_CTL_XXXXXXXXX
#include "xxxxxxx.h"
#elif defined(JZHK_CTP_CTL_J527_62_HD720_1600_TUGAO_Y22)
#include "gsl_ts_fw_j527_62_hd720_1600_tugao_y22.h"
#elif defined(JZHK_CTP_CTL_S225_HD720_1600_KXD_X85)
#include "gsl_ts_fw_s225_hd720_1600_kxd_x85.h"
#elif defined(JZHK_CTP_CTL_J527_HD720_1520_BOPAI_EB01C)
#include "gsl_ts_fw_j527b_hd720_1520_bopai_eb01c.h"
#elif defined(JZHK_CTP_CTL_J527_HD720_1600_BOPAI_EB03)
#include "gsl_ts_fw_j527b_hd720_1600_bopai_eb03.h"
#elif defined(JZHK_CTP_CTL_J527_QHD_480_1014_CHITENG_S62401C)
#include "gsl_ts_fw_j527b_qhd480_1014_chiteng_s62401c.h"
#elif defined(JZHK_CTP_CTL_J527_HD720_1520_CHITENG_S62401C)
#include "gsl_ts_fw_j527b_hd720_1520_chiteng_s62401c.h"
#elif defined(JZHK_CTP_CTL_J517_HD720_1560_LHTC_TC6083C)
#include "gsl_ts_fw_j517_hd720_1560_lhtc_tc6083c.h"
#elif defined(JZHK_CTP_CTL_J517_HD720_1520_LHTC_TC6283D)
#include "gsl_ts_fw_j517_hd720_1520_lhtc_tc6283d.h"
#elif defined(JZHK_CTP_CTL_J527B_HD720_1560_YINGMAI_S61491H)
#include "gsl_ts_fw_j527b_hd720_1560_yingmai_s61491h.h"
#elif defined(JZHK_CTP_CTL_J518A_HD720_1600_TENGZHI_ML656U)
#include "gsl_ts_fw_j518a_hd720_1600_tengzhi_ml656u.h"
#elif defined(JZHK_CTP_CTL_J518AIF_HD720_1520_YST_YS10)
#include "gsl_ts_fw_j518aif_hd720_1520_yst_ys10.h"
#elif defined(JZHK_CTP_CTL_J518AIF_HD720_1440_YST_YS12)
#include "gsl_ts_fw_j518aif_hd720_1440_yst_ys12.h"
#elif defined(JZHK_CTP_CTL_J527_62_HD720_1600_BOPAI_EB02BZ)
#include "gsl_ts_fw_j527_62_hd720_1600_bopai_eb02bz.h"
#elif defined(JZHK_CTP_CTL_J527_HD720_1560_BOPAI_EB06)
#include "gsl_ts_fw_j527b_hd720_1560_bopai_eb06.h"
#else
#include "gsl_ts_fw.h"
#endif



//static unsigned char gsl_cfg_index = 0;

struct fw_config_type
{
	const struct fw_data *fw;
	unsigned int fw_size;
	unsigned int *data_id;
	unsigned int data_size;
};
#if 0
static struct fw_config_type gsl_cfg_table[9] = {
/*0*/{GSLX680_FW,(sizeof(GSLX680_FW)/sizeof(struct fw_data)),gsl_config_data_id,(sizeof(gsl_config_data_id)/4)},
/*1*///{GSLX68X_FW_ZHENGHAI,(sizeof(GSLX68X_FW_ZHENGHAI)/sizeof(struct fw_data)),gsl_config_data_id_zhenghai,(sizeof(gsl_config_data_id_zhenghai)/4)},
/*2*///{GSLX68X_FW_DUNZHENG,(sizeof(GSLX68X_FW_DUNZHENG)/sizeof(struct fw_data)),gsl_config_data_id_denzheng,(sizeof(gsl_config_data_id_denzheng)/4)},
};
#endif
#endif
