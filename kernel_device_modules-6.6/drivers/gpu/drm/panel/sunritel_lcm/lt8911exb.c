
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <linux/completion.h>

#define I2C_ID_NAME "lt8911"
/*static struct i2c_client *lt8911_i2c_client;*/
static struct i2c_client *lt8911_i2c_client;
static struct workqueue_struct *lt8911_config_wq;
static struct delayed_work lt8911_config_work;
/*****************************************************************************
 * Function Prototype
 *****************************************************************************/
static int lt8911_probe(struct i2c_client *client);
static void lt8911_remove(struct i2c_client *client);
/*****************************************************************************
 * Data Structure
 *****************************************************************************/


static const struct of_device_id lcm_of_match[] = {
		{.compatible = "mediatek,lt8911"},
		{},
};

struct lt8911_dev {
	struct i2c_client *client;

};

/*static int lt8911_write_bytes(unsigned char addr, unsigned char value)*/
static int lt8911_write_bytes(unsigned char addr, unsigned char value)
{
	int ret = 0;
	struct i2c_client *client = lt8911_i2c_client;
	char write_data[2] = { 0 };

	write_data[0] = addr;
	write_data[1] = value;
	ret = i2c_master_send(client, write_data, 2);
	if (ret < 0)
		printk("lt8911 write data fail !!\n");
	
	// printk("I2C Write: %s, addr: 0x%02X, data: 0x%02X, ret: %d\n", __func__, addr, value, ret);
	return ret;
}

static int lt8911_write_block(unsigned char *data, int len)
{
    int ret = 0;
    struct i2c_client *client = lt8911_i2c_client;
    
    ret = i2c_master_send(client, data, len);
    if (ret < 0)
        printk("lt8911 write block data fail !!\n");
     printk("I2C Write: %s, ret: %d\n", __func__,ret);
    return ret;
}

static int lt8911_read_bytes(unsigned char addr)
{
	int ret = 0;
	struct i2c_client *client = lt8911_i2c_client;
    char buffer[2] = { 0x00, 0x00 };
	buffer[0] = addr;
	ret = i2c_master_send(client, &buffer[0], 1);
	if (ret < 0)
		printk("lt8911 write data fail !!\n");
	
	ret = i2c_master_recv(client, &buffer[1], 1);
	if (ret < 0)
		printk("lt8911 read data fail !!\n");
	
	// printk("I2C Read: %s, addr: 0x%02X, data: 0x%02X, ret: %d\n", __func__, addr, buffer[1], ret);
	return buffer[1];
}

/*******************************************************

   1、LT8911EXB的IIC地址：
   1. IIC address of lt8911exb:

   a)如果LT8911EXB的第31脚（S_ADR）为低，则LT8911EXB的I2C 地址为0x52; // bit0 是读写标志位；如果是Linux系统，IIC address 的 bit7作为读写标志位，则I2C地址 应该是 0x29
   A) if the 31st pin (s_adr) of lt8911exb is low, the I2C address of lt8911exb is 0x52; // bit0 is the read-write flag ; if it is Linux system, the bit7 of IIC address is the read-write flag, then the I2C address should be 0x29

   b)如果LT8911EXB的第31脚（S_ADR）为高，则LT8911EXB的I2C 地址为0x5a; // bit0 是读写标志位；如果是Linux系统，IIC address 的 bit7作为读写标志位，则I2C地址 应该是 0x2d
   b) if the 31st pin (s_adr) of lt8911exb is high, the I2C address of lt8911exb is 0x5a; // bit0 is the read-write flag ; if it is a Linux system, the bit7 of IIC address is the read-write flag, then the I2C address should be 0x2d

   2、IIC速率不要超过100KHz。
   2. IIC speed shall not exceed 100kHz.

   3、要确定MIPI信号给到LT8911EXB之后，再初始化LT8911EXB。
   3. Confirm that Mipi signal is sent to lt8911exb, and then initialize lt8911exb.

   4、必须由前端主控GPIO来复位LT8911EXB；刷寄存器之前，先Reset LT8911EXB ,用GPIO 先拉低LT8911EXB的复位脚 100ms左右，再拉高，保持100ms。
   4. The lt8911exb must be reset by the Master GPIO; before write the register, reset the lt8911exb first, pull down the reset pin by GPIO for about 100ms, and then pull up for 100ms.

   5、LT8911EXB 对MIPI输入信号的要求：
   5. LT8911EXB MIPI input signal requirements:
   a) MIPI DSI
   b) Video mode

   MIPI 输入需要关闭展频(SSC - Spread-Spectrum Clock)，打开EOTP(End Of Transmite Packet)
   Mipi signal needs to turn off SSC(Spread-Spectrum Clock) and turn on eotp(End Of Transmite Packet)
 *********************************************************/

// #define _Test_Pattern_ // Output test pattern

#define  _read_edid_ // read eDP panel EDID

//#define _Msa_Active_Only_

#define _eDP_2G7_
//#define _eDP_1G62_

#define _link_train_enable_

//********************************************************//

enum {
	hfp = 0,
	hs,
	hbp,
	hact,
	htotal,
	vfp,
	vs,
	vbp,
	vact,
	vtotal,
	pclk_10khz
};

u8		Read_DPCD010A = 0x00;

bool	ScrambleMode = 0;

bool	flag_mipi_on = 0;

#ifdef _read_edid_ // read eDP panel EDID

u8		EDID_DATA[128] = { 0 };
u16		EDID_Timing[11] = { 0 };

bool	EDID_Reply = 0;
#endif

//////////////////////LT8911EXB Config////////////////////////////////
//#define _1920x1200_eDP_Panel_
#define _1080P_eDP_Panel_
//#define _1366x768_eDP_Panel_
//#define _1280x800_eDP_Panel_
//#define _1280x720_eDP_Panel_

//#define _1600x900_eDP_Panel_
//#define _1920x1200_eDP_Panel_

//===========================================//

#define _MIPI_Lane_ 4   // 3 /2 / 1

#define _MIPI_data_PN_Swap_En	0xF0
#define _MIPI_data_PN_Swap_Dis	0x00

#define _MIPI_data_PN_ _MIPI_data_PN_Swap_Dis

//------------------------------------------//

#define _No_swap_			0x00    // 3210 default
#define _MIPI_data_3210_	0       // default
#define _MIPI_data_0123_	21
#define _MIPI_data_2103_	20

#define _MIPI_data_sequence_ _No_swap_


/*
   LT8911EXB pin		MIPI RX
   D3（37、38）		D3	2	3	2	1	1	3	2	3	2	0	0	3	0	3	0	1	1	2	0	2	0	1	1
   D2（40、41）		D2	3	1	1	2	3	2	3	0	0	2	3	0	3	1	1	0	3	0	2	1	1	0	2
   D1（44、45）		D1	1	2	3	3	2	0	1	2	3	3	2	1	1	0	3	3	0	1	1	0	2	2	0
   D0（47、48）		D0	0	0	0	0	0	1	0	1	1	1	1	2	2	2	2	2	2	3	3	3	3	3	3

   0xD003 Reg value	0	1	2	3	4	5	6	7	8	9	10	11	12	13	14	15	16	17	18	19	20	21	22	23
   //*/
//===========================================//

#define _eDP_data_PN_Swap_En	0xF0 // Please refer to the notes below
#define _eDP_data_PN_Swap_Dis	0x00

#define _eDP_data_PN_ _eDP_data_PN_Swap_Dis // defailt ; disable

/* eDP data P/N polarity swap 
bit7	RGD_MLCTRL_LANE3_RVSD_EN
		1 = data of lane3 polarity swap;
		0 = normal.

bit6	RGD_MLCTRL_LANE2_RVSD_EN
		1 = data of lane2 polarity swap;
		0 = normal.

bit5	RGD_MLCTRL_LANE1_RVSD_EN
		1 = data of lane1 polarity swap;
		0 = normal.

bit4	RGD_MLCTRL_LANE0_RVSD_EN
		1 = data of lane0 polarity swap;
		0 = normal.
//*/

//------------------------------------------//

#define _Lane0_data_	0
#define _Lane1_data_	1
#define _Lane2_data_	2
#define _Lane3_data_	3

#define _eDP_data_No_swap_	 0xe4 // default

#define _eDP_data3_select_	(_Lane3_data_ << 6) // default; _Lane3_data_select_ is lane3
#define _eDP_data2_select_	(_Lane2_data_ << 4) // default; _Lane2_data_select_ is lane2

#define _eDP_data1_select_	(_Lane1_data_ << 2)	// default; _Lane1_data_select_ is lane1
#define _eDP_data0_select_	(_Lane0_data_ << 0) // default; _Lane0_data_select_ is lane0

// example:lane1 and lane0 swap
//#define _eDP_data1_select_	(_Lane0_data_ << 2)	// default _Lane1_data_select_ is lane0
//#define _eDP_data0_select_	(_Lane1_data_ << 0) // default _Lane0_data_select_ is lane1


#define _eDP_data_sequence_ (_eDP_data3_select_ + _eDP_data2_select_ + _eDP_data1_select_ + _eDP_data0_select_)// _eDP_data_No_swap_


//===========================================//



//#define _eDP_scramble_ // eDP scramble mode

#define _Nvid 0         // 0: 0x0080,default
static int Nvid_Val[] = { 0x0080, 0x0800 };

#ifdef _1920x1200_eDP_Panel_

#define eDP_lane		2
#define PCR_PLL_PREDIV	0x40

#define _6bit_
//#define _8bit_

//const struct video_timing video[] =
static int MIPI_Timing[] =
// hfp, hs,	hbp,	hact,	htotal,	vfp,	vs,	vbp,	vact,	vtotal,	pixel_CLK/10000
//-----|---|------|-------|--------|-----|-----|-----|--------|--------|---------------
{ 48, 32, 80, 1920, 2080, 5, 5, 20, 1200, 1230, 15350 };                                //


/*******************************************************************
   全志平台的屏参 lcd_hbp 是屏参数 h_backporch + hs_width 的和；
   The lcd_hbp of Allwinner platform is the sum of the above parameters hbp + hs;

   同样的，全志平台的屏参 lcd_vbp 是 屏参数 v_backporch + vs_width 的和；这点要注意。
   In the same, lcd_vbp are the sum of the above parameters vbp + vs, which should be noted.
   //-------------------------------------------------------------------

   EOTP(End Of Transmite Packet，hs 传完了，会发这样一个包) 要打开，(之前的LT8911B 需要关闭EOTP，这里LT8911EXB 需要打开)
   Eotp (end of transmit packet, HS will send such a packet) must be turn-on (lt8911b needs to turn-off eotp, here lt8911exb needs to turn-on)

   1、MTK平台 的 dis_eotp_en 的值，改成 false;   LK和kernel都需要修改,改成false。
   1. The value of dis_eotp_en of MTK platform should be changed to 'false'; LK and kernel need to be changed to 'false'.

   2、展讯平台的 tx_eotp 的值置 1 。
   2. The value of tx_eotp of Spreadtrum platform is set to 1.

   3、RK平台 EN_EOTP_TX 置1.
   3. The value of EN_EOTP_TX of RK platform is set to 1.

   4、高通平台 找到 dsi_ctrl_hw_cmn.c -->void dsi_ctrl_hw_cmn_host_setup(struct dsi_ctrl_hw *ctrl,struct dsi_host_common_cfg *cfg)-->增加cfg->append_tx_eot = true;
   4、Qualcomm: dsi_ctrl_hw_cmn.c -->void dsi_ctrl_hw_cmn_host_setup(struct dsi_ctrl_hw *ctrl,struct dsi_host_common_cfg *cfg)-->add cfg->append_tx_eot = true;
*******************************************************************/

#endif

#ifdef _1080P_eDP_Panel_

#define eDP_lane		2
#define PCR_PLL_PREDIV	0x40

// 根据前端MIPI信号的Timing，修改以下参数：
//According to the timing of the Mipi signal, modify the following parameters:
static int MIPI_Timing[] =
// hfp,	hs,	hbp,	hact,	htotal,	vfp,	vs,	vbp,	vact,	vtotal,	pixel_CLK/10000
//-----|---|------|-------|--------|-----|-----|-----|--------|--------|---------------
{ 88, 	44, 148, 	1920, 	2200, 	4, 		5, 	36, 	1080, 	1125, 		14850 };   // VESA

//  { 48, 32, 80,   1920,   2080,   3,  5,  23,    1080,   1111,   13850 };    // SL156PP36


/*******************************************************************
   全志平台的屏参 lcd_hbp 是 上面参数 hbp + hs 的和；
   The lcd_hbp of Allwiner platform is the sum of the above parameters hbp + hs;

   同样的，全志平台的屏参 lcd_vbp 是 上面参数 vbp + vs的 和，这点要注意。
   In the same, lcd_vbp are the sum of the above parameters vbp + vs, which should be noted.
   //-------------------------------------------------------------------

   EOTP(End Of Transmite Packet，hs 传完了，会发这样一个包) 要打开，(之前的LT8911B 需要关闭EOTP，这里LT8911EXB 需要打开)
   Eotp (end of transmit packet, HS will send such a packet) must be turn-on (lt8911b needs to turn-off eotp, here lt8911exb needs to turn-on)

   1、MTK平台 的 dis_eotp_en 的值，改成 false;   LK和kernel都需要修改,改成false。
   1. The value of dis_eotp_en of MTK platform should be changed to 'false'; LK and kernel need to be changed to 'false'.

   2、展讯平台的 tx_eotp 的值置 1 。
   2. The value of tx_eotp of Spreadtrum platform is set to 1.

   3、RK平台 EN_EOTP_TX 置1.
   3. The value of EN_EOTP_TX of RK platform is set to 1.

   4、高通平台 找到 dsi_ctrl_hw_cmn.c -->void dsi_ctrl_hw_cmn_host_setup(struct dsi_ctrl_hw *ctrl,struct dsi_host_common_cfg *cfg)-->增加cfg->append_tx_eot = true;
   4、Qualcomm: dsi_ctrl_hw_cmn.c -->void dsi_ctrl_hw_cmn_host_setup(struct dsi_ctrl_hw *ctrl,struct dsi_host_common_cfg *cfg)-->add cfg->append_tx_eot = true;

*******************************************************************/

//#define _6bit_ // eDP panel Color Depth，262K color
#define _8bit_                                              // eDP panel Color Depth，16.7M color

#endif

//-----------------------------------------

#ifdef _1366x768_eDP_Panel_

#define eDP_lane		1
#define PCR_PLL_PREDIV	0x40

// 根据前端MIPI信号的Timing，修改以下参数：
static int MIPI_Timing[] =
// hfp,	hs,	hbp,	hact,	htotal,	vfp,	vs,	vbp,	vact,	vtotal,	pixel_CLK/10000
//-----|---|------|-------|--------|-----|-----|-----|--------|--------|---------------
{ 14, 56, 64, 1366, 1500, 1, 3, 28, 768, 800, 7200 };


/*******************************************************************
   全志平台的屏参 lcd_hbp 是 上面参数 hbp + hs 的和；
   The lcd_hbp of Allwiner platform is the sum of the above parameters hbp + hs;

   同样的，全志平台的屏参 lcd_vbp 是 上面参数 vbp + vs的 和，这点要注意。
   In the same, lcd_vbp are the sum of the above parameters vbp + vs, which should be noted.
   //-------------------------------------------------------------------

   EOTP(End Of Transmite Packet，hs 传完了，会发这样一个包) 要打开，(之前的LT8911B 需要关闭EOTP，这里LT8911EXB 需要打开)
   Eotp (end of transmit packet, HS will send such a packet) must be turn-on (lt8911b needs to turn-off eotp, here lt8911exb needs to turn-on)

   1、MTK平台 的 dis_eotp_en 的值，改成 false;	 LK和kernel都需要修改,改成false。
   1. The value of dis_eotp_en of MTK platform should be changed to 'false'; LK and kernel need to be changed to 'false'.

   2、展讯平台的 tx_eotp 的值置 1 。
   2. The value of tx_eotp of Spreadtrum platform is set to 1.

   3、RK平台 EN_EOTP_TX 置1.
   3. The value of EN_EOTP_TX of RK platform is set to 1.

   4、高通平台 找到 dsi_ctrl_hw_cmn.c -->void dsi_ctrl_hw_cmn_host_setup(struct dsi_ctrl_hw *ctrl,struct dsi_host_common_cfg *cfg)-->增加cfg->append_tx_eot = true;
   4、Qualcomm: dsi_ctrl_hw_cmn.c -->void dsi_ctrl_hw_cmn_host_setup(struct dsi_ctrl_hw *ctrl,struct dsi_host_common_cfg *cfg)-->add cfg->append_tx_eot = true;

*******************************************************************/

#define _6bit_                                                              // eDP panel Color Depth，262K color

#endif

//*****************************************************//

void LT8911EX_ChipID( void )                                                // read Chip ID
{
//	Debug_printk( "\r\n###################start#####################" );
	lt8911_write_bytes( 0xff, 0x81 );                                 //register bank
	lt8911_write_bytes( 0x08, 0x7f );
    lt8911_read_bytes( 0x00 );//0x17
	lt8911_read_bytes( 0x01 );//0x05
	lt8911_read_bytes( 0x02 );//0xe0
#ifdef _uart_debug_
	printk( "\r\nLT8911EXB chip ID:", lt8911_read_bytes( 0x00 ) );    // 0x17
	printk( ", ", lt8911_read_bytes( 0x01 ) );                        // 0x05
	printk( ", ", lt8911_read_bytes( 0x02 ) );                        // 0xE0
#endif
}

/***********************************************************

***********************************************************/
void LT8911EXB_read_edid( void )
{
#ifdef _read_edid_
	u8 reg, i, j;
//	bool	aux_reply, aux_ack, aux_nack, aux_defer;
	lt8911_write_bytes( 0xff, 0xac );
	lt8911_write_bytes( 0x00, 0x20 ); //Soft Link train
	lt8911_write_bytes( 0xff, 0xa6 );
	lt8911_write_bytes( 0x2a, 0x01 );

	/*set edid offset addr*/
	lt8911_write_bytes( 0x2b, 0x40 ); //CMD
	lt8911_write_bytes( 0x2b, 0x00 ); //addr[15:8]
	lt8911_write_bytes( 0x2b, 0x50 ); //addr[7:0]
	lt8911_write_bytes( 0x2b, 0x00 ); //data lenth
	lt8911_write_bytes( 0x2b, 0x00 ); //data lenth
	lt8911_write_bytes( 0x2c, 0x00 ); //start Aux read edid

#ifdef _uart_debug_
	printk( "\r\n" );
	printk( "\r\nRead eDP EDID......" );
#endif

	msleep( 20 );                         //more than 10ms
	reg = lt8911_read_bytes( 0x25 );
	if( ( reg & 0x0f ) == 0x0c )
	{
		for( j = 0; j < 8; j++ )
		{
			if( j == 7 )
			{
				lt8911_write_bytes( 0x2b, 0x10 ); //MOT
			}else
			{
				lt8911_write_bytes( 0x2b, 0x50 );
			}

			lt8911_write_bytes( 0x2b, 0x00 );
			lt8911_write_bytes( 0x2b, 0x50 );
			lt8911_write_bytes( 0x2b, 0x0f );
			lt8911_write_bytes( 0x2c, 0x00 ); //start Aux read edid
			msleep( 50 );                         //more than 50ms

			if( lt8911_read_bytes( 0x39 ) == 0x31 )
			{
				lt8911_read_bytes( 0x2b );
				for( i = 0; i < 16; i++ )
				{
					EDID_DATA[j * 16 + i] = lt8911_read_bytes( 0x2b );
				}

				EDID_Reply = 1;
			}else
			{
				EDID_Reply = 0;
#ifdef _uart_debug_
				printk( "\r\nno_reply" );
				printk( "\r\n" );
#endif
				//		print("\r\n*************End***************");
				return;
			}
		}

#ifdef _uart_debug_

		for( i = 0; i < 128; i++ ) //print edid data
		{
			if( ( i % 16 ) == 0 )
			{
				printk( "\r\n" );
			}
			printk( ", ", EDID_DATA[i] );
		}

		printk( "\r\n" );
		printk( "\r\neDP Timing = { H_FP / H_pluse / H_BP / H_act / H_tol / V_FP / V_pluse / V_BP / V_act / V_tol / D_CLK };" );
		printk( "\r\neDP Timing = { " );
		EDID_Timing[hfp] = ( ( EDID_DATA[0x41] & 0xC0 ) * 4 + EDID_DATA[0x3e] );
		printk( (u32)EDID_Timing[hfp] );        // HFB
		printk( ", " );

		EDID_Timing[hs] = ( ( EDID_DATA[0x41] & 0x30 ) * 16 + EDID_DATA[0x3f] );
		printk( (u32)EDID_Timing[hs] );         // Hsync Wid
		printk( ", " );

		EDID_Timing[hbp] = ( ( ( EDID_DATA[0x3a] & 0x0f ) * 0x100 + EDID_DATA[0x39] ) - ( ( EDID_DATA[0x41] & 0x30 ) * 16 + EDID_DATA[0x3f] ) - ( ( EDID_DATA[0x41] & 0xC0 ) * 4 + EDID_DATA[0x3e] ) );
		printk( (u32)EDID_Timing[hbp] );        // HBP
		printk( ", " );

		EDID_Timing[hact] = ( ( EDID_DATA[0x3a] & 0xf0 ) * 16 + EDID_DATA[0x38] );
		printk( (u32)EDID_Timing[hact] );       // H active
		printk( ", " );

		EDID_Timing[htotal] = ( ( EDID_DATA[0x3a] & 0xf0 ) * 16 + EDID_DATA[0x38] + ( ( EDID_DATA[0x3a] & 0x0f ) * 0x100 + EDID_DATA[0x39] ) );
		printk( (u32)EDID_Timing[htotal] );     // H total
		printk( ", " );

		EDID_Timing[vfp] = ( ( EDID_DATA[0x41] & 0x0c ) * 4 + ( EDID_DATA[0x40] & 0xf0 ) / 16 );
		printk( (u32)EDID_Timing[vfp] );        // VFB
		printk( ", " );

		EDID_Timing[vs] = ( ( EDID_DATA[0x41] & 0x03 ) * 16 + EDID_DATA[0x40] & 0x0f );
		printk( (u32)EDID_Timing[vs] );         // Vsync Wid
		printk( ", " );

		EDID_Timing[vbp] = ( ( ( EDID_DATA[0x3d] & 0x03 ) * 0x100 + EDID_DATA[0x3c] ) - ( ( EDID_DATA[0x41] & 0x03 ) * 16 + EDID_DATA[0x40] & 0x0f ) - ( ( EDID_DATA[0x41] & 0x0c ) * 4 + ( EDID_DATA[0x40] & 0xf0 ) / 16 ) );
		printk( (u32)EDID_Timing[vbp] );        // VBP
		printk( ", " );

		EDID_Timing[vact] = ( ( EDID_DATA[0x3d] & 0xf0 ) * 16 + EDID_DATA[0x3b] );
		printk( (u32)EDID_Timing[vact] );       // V active
		printk( ", " );

		EDID_Timing[vtotal] = ( ( EDID_DATA[0x3d] & 0xf0 ) * 16 + EDID_DATA[0x3b] + ( ( EDID_DATA[0x3d] & 0x03 ) * 0x100 + EDID_DATA[0x3c] ) );
		printk( (u32)EDID_Timing[vtotal] );     // V total
		printk( ", " );

		EDID_Timing[pclk_10khz] = ( EDID_DATA[0x37] * 0x100 + EDID_DATA[0x36] );
		printk( (u32)EDID_Timing[pclk_10khz] ); // CLK
		printk( " };" );
		printk( "\r\n" );
#endif
	}

	return;

#endif
}

/***********************************************************

***********************************************************/
/*
{
	lt8911_write_bytes( 0xff, 0xd0 );
	lt8911_write_bytes( 0x0d, (u8)( MIPI_Timing[vtotal] / 256 ) );
	lt8911_write_bytes( 0x0e, (u8)( MIPI_Timing[vtotal] % 256 ) );    //vtotal
	lt8911_write_bytes( 0x0f, (u8)( MIPI_Timing[vact] / 256 ) );
	lt8911_write_bytes( 0x10, (u8)( MIPI_Timing[vact] % 256 ) );      //vactive

	lt8911_write_bytes( 0x11, (u8)( MIPI_Timing[htotal] / 256 ) );
	lt8911_write_bytes( 0x12, (u8)( MIPI_Timing[htotal] % 256 ) );    //htotal
	lt8911_write_bytes( 0x13, (u8)( MIPI_Timing[hact] / 256 ) );
	lt8911_write_bytes( 0x14, (u8)( MIPI_Timing[hact] % 256 ) );      //hactive

	lt8911_write_bytes( 0x15, (u8)( MIPI_Timing[vs] % 256 ) );        //vsa
	lt8911_write_bytes( 0x16, (u8)( MIPI_Timing[hs] % 256 ) );        //hsa
	lt8911_write_bytes( 0x17, (u8)( MIPI_Timing[vfp] / 256 ) );
	lt8911_write_bytes( 0x18, (u8)( MIPI_Timing[vfp] % 256 ) );       //vfp

	lt8911_write_bytes( 0x19, (u8)( MIPI_Timing[hfp] / 256 ) );
	lt8911_write_bytes( 0x1a, (u8)( MIPI_Timing[hfp] % 256 ) );       //hfp
}
*/
//------------------------------------
#if 0
void LT8911EXB_eDP_Video_cfg( void )                                        // ( struct video_timing *video_format )
{
	lt8911_write_bytes( 0xff, 0xa8 );
	lt8911_write_bytes( 0x2d, 0x88 );                                 // MSA from register

#ifdef _Msa_Active_Only_
	lt8911_write_bytes( 0x05, 0x00 );
	lt8911_write_bytes( 0x06, 0x00 );                                 //htotal
	lt8911_write_bytes( 0x07, 0x00 );
	lt8911_write_bytes( 0x08, 0x00 );                                 //h_start

	lt8911_write_bytes( 0x09, 0x00 );
	lt8911_write_bytes( 0x0a, 0x00 );                                 //hsa
	lt8911_write_bytes( 0x0b, (u8)( MIPI_Timing[hact] / 256 ) );
	lt8911_write_bytes( 0x0c, (u8)( MIPI_Timing[hact] % 256 ) );      //hactive

	lt8911_write_bytes( 0x0d, 0x00 );
	lt8911_write_bytes( 0x0e, 0x00 );                                 //vtotal

	lt8911_write_bytes( 0x11, 0x00 );
	lt8911_write_bytes( 0x12, 0x00 );
	lt8911_write_bytes( 0x14, 0x00 );
	lt8911_write_bytes( 0x15, (u8)( MIPI_Timing[vact] / 256 ) );
	lt8911_write_bytes( 0x16, (u8)( MIPI_Timing[vact] % 256 ) );      //vactive

#else

	lt8911_write_bytes( 0x05, (u8)( MIPI_Timing[htotal] / 256 ) );
	lt8911_write_bytes( 0x06, (u8)( MIPI_Timing[htotal] % 256 ) );
	lt8911_write_bytes( 0x07, (u8)( ( MIPI_Timing[hs] + MIPI_Timing[hbp] ) / 256 ) );
	lt8911_write_bytes( 0x08, (u8)( ( MIPI_Timing[hs] + MIPI_Timing[hbp] ) % 256 ) );
	lt8911_write_bytes( 0x09, (u8)( MIPI_Timing[hs] / 256 ) );
	lt8911_write_bytes( 0x0a, (u8)( MIPI_Timing[hs] % 256 ) );
	lt8911_write_bytes( 0x0b, (u8)( MIPI_Timing[hact] / 256 ) );
	lt8911_write_bytes( 0x0c, (u8)( MIPI_Timing[hact] % 256 ) );
	lt8911_write_bytes( 0x0d, (u8)( MIPI_Timing[vtotal] / 256 ) );
	lt8911_write_bytes( 0x0e, (u8)( MIPI_Timing[vtotal] % 256 ) );
	lt8911_write_bytes( 0x11, (u8)( ( MIPI_Timing[vs] + MIPI_Timing[vbp] ) / 256 ) );
	lt8911_write_bytes( 0x12, (u8)( ( MIPI_Timing[vs] + MIPI_Timing[vbp] ) % 256 ) );
	lt8911_write_bytes( 0x14, (u8)( MIPI_Timing[vs] % 256 ) );
	lt8911_write_bytes( 0x15, (u8)( MIPI_Timing[vact] / 256 ) );
	lt8911_write_bytes( 0x16, (u8)( MIPI_Timing[vact] % 256 ) );
#endif
}
#endif
/***********************************************************

***********************************************************/

//------------------------------------

void LT8911EXB_init( void )
{
	u8	i;
	u8	pcr_pll_postdiv;
	u8	pcr_m;
	u16 Temp16;
	
	Temp16 = MIPI_Timing[pclk_10khz];
	pcr_pll_postdiv = 0x04;
	pcr_m = (u8)( Temp16 * pcr_pll_postdiv / 25 / 100 );
	
	/* init */
	lt8911_write_bytes( 0xff, 0x81 ); // Change Reg bank
	lt8911_write_bytes( 0x08, 0x7f ); // i2c over aux issue
	lt8911_write_bytes( 0x49, 0xff ); // enable 0x87xx

	lt8911_write_bytes( 0xff, 0x82 ); // Change Reg bank
	lt8911_write_bytes( 0x5a, 0x0e ); // GPIO test output

	//for power consumption//
	lt8911_write_bytes( 0xff, 0x81 );
	lt8911_write_bytes( 0x05, 0x06 );

	unsigned char reg43_47[] = {
		0x43,0x00,0x1f,0xf7,0xf6
	};
	lt8911_write_block(reg43_47, sizeof(reg43_47));	

	lt8911_write_bytes( 0x49, 0x7f );

	lt8911_write_bytes( 0xff, 0x82 );
	lt8911_write_bytes( 0x12, 0x33 );//edp lane 2; lan 1 0x11


	/* mipi Rx analog */
	lt8911_write_bytes( 0xff, 0x82 ); // Change Reg bank
	lt8911_write_bytes( 0x32, 0x51 );
	lt8911_write_bytes( 0x35, 0x22 ); //EQ current 0x22/0x42/0x62/0x82/0xA2/0xC2/0xe2

	unsigned char reg3a_3b[] = {
		0x3a,0x77,0x77
	};	
	lt8911_write_block(reg3a_3b, sizeof(reg3a_3b));	


	unsigned char reg4c_4d[] = {
		0x4c,0x0c,0x00
	};	
	lt8911_write_block(reg4c_4d, sizeof(reg4c_4d));	

	/* dessc_pcr  pll analog */
	lt8911_write_bytes( 0xff, 0x82 ); // Change Reg bank
	
	unsigned char reg6a_6b[] = {
		0x6a,0x40,PCR_PLL_PREDIV
	};	
	lt8911_write_block(reg6a_6b, sizeof(reg6a_6b));	
	
	lt8911_write_bytes( 0x6e, 0x81 ); //0x40:pre-div = 1, pixel_clk =88~176MHz


	/* dessc pll digital */
	lt8911_write_bytes( 0xff, 0x85 );     // Change Reg bank

	unsigned char rega9_ragae[] = {
		0xa9,0x31,0x17,0xba,0xe1,0x47,0x01
	};	
	lt8911_write_block(rega9_ragae, sizeof(rega9_ragae));	

	lt8911_write_bytes( 0xae, 0x11 );

	/* Digital Top */
	lt8911_write_bytes( 0xff, 0x85 );                             // Change Reg bank
	lt8911_write_bytes( 0xc0, 0x01 );                             //select mipi Rx
#ifdef _6bit_
	lt8911_write_bytes( 0xb0, 0xd0 );                             //enable dither
#else
	lt8911_write_bytes( 0xb0, 0x00 );                             // disable dither
#endif

	/* mipi Rx Digital */
	lt8911_write_bytes( 0xff, 0xd0 );                             // Change Reg bank
	lt8911_write_bytes( 0x00, _MIPI_data_PN_ + _MIPI_Lane_ % 4 ); // 0: 4 Lane / 1: 1 Lane / 2 : 2 Lane / 3: 3 Lane
	lt8911_write_bytes( 0x02, 0x08 );                             //settle
	lt8911_write_bytes( 0x03, _MIPI_data_sequence_ );             // default is 0x00
	lt8911_write_bytes( 0x08, 0x00 );
//	lt8911_write_bytes( 0x0a, 0x12 );               //pcr mode

	lt8911_write_bytes( 0x0c, 0x80 );                             //fifo position
	lt8911_write_bytes( 0x1c, 0x80 );                             //fifo position

	//	hs mode:MIPI行采样；vs mode:MIPI帧采样
	lt8911_write_bytes( 0x24, 0x70 );                             // 0x30  [3:0]  line limit	  //pcr mode( de hs vs)

	lt8911_write_bytes( 0x31, 0x0a );

	/*stage1 hs mode*/
	lt8911_write_bytes( 0x25, 0x90 );                             // 0x80		   // line limit
	lt8911_write_bytes( 0x2a, 0x3a );                             // 0x04		   // step in limit
	
	unsigned char reg21_reg22[] = {
		0x21,0x4f,0xff
	};	
	lt8911_write_block(reg21_reg22, sizeof(reg21_reg22));

	/*stage2 de mode*/
	lt8911_write_bytes( 0x0a, 0x02 );                             //de adjust pre line

	unsigned char reg38_reg3b[] = {
		0x38,0x02,0x04,0x08,0x10
	};	
	lt8911_write_block(reg38_reg3b, sizeof(reg38_reg3b));	

	lt8911_write_bytes( 0x3f, 0x04 );                             //de_step 1

	unsigned char reg40_reg42[] = {
		0x40,0x08,0x60
	};	
	lt8911_write_block(reg40_reg42, sizeof(reg40_reg42));	
	

	/*stage2 hs mode*/
	lt8911_write_bytes( 0x1e, 0x01 );                             // 0x11
	lt8911_write_bytes( 0x23, 0xf0 );                             // 0x80			   //

	lt8911_write_bytes( 0x2b, 0x80 );                             // 0xa0

#ifdef _Test_Pattern_
	lt8911_write_bytes( 0x26, ( pcr_m | 0x80 ) );
#else

	lt8911_write_bytes( 0x26, pcr_m );

//	lt8911_write_bytes( 0x27, Read_0xD095 );
//	lt8911_write_bytes( 0x28, Read_0xD096 );
#endif

	lt8911_write_bytes( 0xff, 0xd0 );
	
unsigned char timing_settings[] = {
    0x0d, 
    (u8)(MIPI_Timing[vtotal] / 256),
    (u8)(MIPI_Timing[vtotal] % 256),
    (u8)(MIPI_Timing[vact] / 256),
    (u8)(MIPI_Timing[vact] % 256),
    (u8)(MIPI_Timing[htotal] / 256),
    (u8)(MIPI_Timing[htotal] % 256),
    (u8)(MIPI_Timing[hact] / 256),
    (u8)(MIPI_Timing[hact] % 256),
    (u8)(MIPI_Timing[vs] % 256),
    (u8)(MIPI_Timing[hs] % 256),
    (u8)(MIPI_Timing[vfp] / 256),
    (u8)(MIPI_Timing[vfp] % 256),
    (u8)(MIPI_Timing[hfp] / 256),
    (u8)(MIPI_Timing[hfp] % 256)
};
lt8911_write_block(timing_settings, sizeof(timing_settings));


	lt8911_write_bytes( 0xff, 0x81 ); // Change Reg bank
	lt8911_write_bytes( 0x03, 0x7b ); //PCR reset
	lt8911_write_bytes( 0x03, 0xff );

#ifdef _eDP_2G7_
	lt8911_write_bytes( 0xff, 0x87 );

	unsigned char reg19_reg1c[] = {
		0x19,0x31,0x1b,0x00,0x00
	};	
	lt8911_write_block(reg19_reg1c, sizeof(reg19_reg1c));	
	
	// txpll Analog
	lt8911_write_bytes( 0xff, 0x82 );
	lt8911_write_bytes( 0x09, 0x00 ); // div hardware mode, for ssc.

//	lt8911_write_bytes( 0x01, 0x18 );// default : 0x18

	unsigned char reg02_reg04[] = {
		0x02,0x42,0x00
	};	
	lt8911_write_block(reg02_reg04, sizeof(reg02_reg04));

	lt8911_write_bytes( 0x03, 0x01 ); // txpll en = 1
//	lt8911_write_bytes( 0x04, 0x3a );// default : 0x3A
	lt8911_write_bytes(0x0a,0x1b);
	lt8911_write_bytes(0x04,0x2a);
		
	lt8911_write_bytes( 0xff, 0x87 );
	lt8911_write_bytes( 0x0c, 0x10 ); // cal en = 0

	lt8911_write_bytes( 0xff, 0x81 );
	lt8911_write_bytes( 0x09, 0xfc );
	lt8911_write_bytes( 0x09, 0xfd );

	lt8911_write_bytes( 0xff, 0x87 );
	lt8911_write_bytes( 0x0c, 0x11 ); // cal en = 1

	// ssc
	lt8911_write_bytes( 0xff, 0x87 );
	lt8911_write_bytes( 0x13, 0x83 );
	lt8911_write_bytes( 0x14, 0x41 );
	lt8911_write_bytes( 0x16, 0x0a );


	unsigned char reg18_reg19[] = {
		0x18,0x0a,0x33
	};	
	lt8911_write_block(reg18_reg19, sizeof(reg18_reg19));

#endif


	lt8911_write_bytes( 0xff, 0x87 );

	for( i = 0; i < 5; i++ ) //Check Tx PLL
	{
		msleep( 5 );
		if( lt8911_read_bytes( 0x37 ) & 0x02 )
		{
			printk( "\r\nLT8911 tx pll locked" );
			lt8911_write_bytes(0xff,0x87);
			lt8911_write_bytes(0x1a,0x36);
			lt8911_write_bytes(0xff,0x82);
			lt8911_write_bytes(0x0a,0x36);
			lt8911_write_bytes(0x04,0x3a);
			break;
		}else
		{
			printk( "\r\nLT8911 tx pll unlocked" );
			lt8911_write_bytes( 0xff, 0x81 );
			lt8911_write_bytes( 0x09, 0xfc );
			lt8911_write_bytes( 0x09, 0xfd );

			lt8911_write_bytes( 0xff, 0x87 );
			lt8911_write_bytes( 0x0c, 0x10 );
			lt8911_write_bytes( 0x0c, 0x11 );
		}
	}

	lt8911_write_bytes( 0xff, 0xac ); // Change Reg bank
	lt8911_write_bytes( 0x15, _eDP_data_sequence_ ); // eDP data swap
	lt8911_write_bytes( 0x16, _eDP_data_PN_); // eDP P / N swap

	// AUX reset
	lt8911_write_bytes( 0xff, 0x81 ); // Change Reg bank
	lt8911_write_bytes( 0x07, 0xfe );
	lt8911_write_bytes( 0x07, 0xff );
	lt8911_write_bytes( 0x0a, 0xfc );
	lt8911_write_bytes( 0x0a, 0xfe );

	/* tx phy */
	lt8911_write_bytes( 0xff, 0x82 ); // Change Reg bank
	lt8911_write_bytes( 0x11, 0x00 );
	lt8911_write_bytes( 0x13, 0x10 );
	lt8911_write_bytes( 0x14, 0x0c );
	lt8911_write_bytes( 0x14, 0x08 );
	lt8911_write_bytes( 0x13, 0x20 );

	lt8911_write_bytes( 0xff, 0x82 ); // Change Reg bank
	lt8911_write_bytes( 0x0e, 0x35 );
//	lt8911_write_bytes( 0x12, 0xff );
//	lt8911_write_bytes( 0xff, 0x80 );
//	lt8911_write_bytes( 0x40, 0x22 );

	/*eDP Tx Digital */
	lt8911_write_bytes( 0xff, 0xa8 ); // Change Reg bank

#ifdef _Test_Pattern_

	lt8911_write_bytes( 0x24, 0x50 ); // bit2 ~ bit 0 : test panttern image mode
	lt8911_write_bytes( 0x25, 0x70 ); // bit6 ~ bit 4 : test Pattern color
	lt8911_write_bytes( 0x27, 0x50 ); //0x50:Pattern; 0x10:mipi video

//	lt8911_write_bytes( 0x2d, 0x00 ); //  pure color setting
//	lt8911_write_bytes( 0x2d, 0x84 ); // black color
	lt8911_write_bytes( 0x2d, 0x88 ); //  block

#else
	lt8911_write_bytes( 0x27, 0x10 ); //0x50:Pattern; 0x10:mipi video
#endif

#ifdef _6bit_
	lt8911_write_bytes( 0x17, 0x00 );
	lt8911_write_bytes( 0x18, 0x00 );
#else
	// _8bit_
	lt8911_write_bytes( 0x17, 0x10 );
	lt8911_write_bytes( 0x18, 0x20 );
#endif

	/* nvid */
	lt8911_write_bytes( 0xff, 0xa0 );                             // Change Reg bank
	lt8911_write_bytes( 0x00, (u8)( Nvid_Val[_Nvid] / 256 ) );    // 0x08
	lt8911_write_bytes( 0x01, (u8)( Nvid_Val[_Nvid] % 256 ) );    // 0x00
}

//-------------------------------------------------------------------
/* mipi should be ready before configuring below video check setting*/
void LT8911EXB_video_check( void )
{
	// u8	temp;
	u32 reg = 0x00;
	/* mipi byte clk check*/
	lt8911_write_bytes( 0xff, 0x85 );     // Change Reg bank
	lt8911_write_bytes( 0x1d, 0x00 );     //FM select byte clk
	lt8911_write_bytes( 0x40, 0xf7 );
	lt8911_write_bytes( 0x41, 0x30 );

	//#ifdef _eDP_scramble_
	if( ScrambleMode )
	{
		lt8911_write_bytes( 0xa1, 0x82 ); //eDP scramble mode;
	}
	//#else
	else
	{
		lt8911_write_bytes( 0xa1, 0x02 ); // DP scramble mode;
	}
	//#endif

//	lt8911_write_bytes( 0x17, 0xf0 ); // 0xf0:Close scramble; 0xD0 : Open scramble

	lt8911_write_bytes( 0xff, 0x81 );
	lt8911_write_bytes( 0x09, 0x7d );
	lt8911_write_bytes( 0x09, 0xfd );

	lt8911_write_bytes( 0xff, 0x85 );
	msleep( 200 );
	if( lt8911_read_bytes( 0x50 ) == 0x03 )
	{
		reg	   = lt8911_read_bytes( 0x4d );
		reg	   = reg * 256 + lt8911_read_bytes( 0x4e );
		reg	   = reg * 256 + lt8911_read_bytes( 0x4f );

		printk( "\r\nvideo check: mipi byteclk =  " ); // mipi byteclk = reg * 1000
		// printk( reg );
	}else
	{
		printk( "\r\nvideo check: mipi clk unstable" );
	}

	/* mipi vtotal check*/
	reg	   = lt8911_read_bytes( 0x76 );
	reg	   = reg * 256 + lt8911_read_bytes( 0x77 );

	printk( "\r\nvideo check: Vtotal =  " );
	// printk( reg );

	/* mipi word count check*/
	lt8911_write_bytes( 0xff, 0xd0 );
	reg	   = lt8911_read_bytes( 0x82 );
	reg	   = reg * 256 + lt8911_read_bytes( 0x83 );
	reg	   = reg / 3;

	printk( "\r\nvideo check: Hact(word counter) =  " );
	// printk( reg );

	/* mipi Vact check*/
	reg	   = lt8911_read_bytes( 0x85 );
	reg	   = reg * 256 + lt8911_read_bytes( 0x86 );

	printk( "\r\nvideo check: Vact =  " );
	// printk( reg );
}

//------------------------------------------------------------------
void DpcdWrite( u32 Address, u8 Data )
{
	/***************************
	   注意大小端的问题!
	   这里默认是大端模式

	   Pay attention to the Big-Endian and Little-Endian!
	   The default mode is Big-Endian here.

	 ****************************/
	u8	AddressH   = 0x0f & ( Address >> 16 );
	u8	AddressM   = 0xff & ( Address >> 8 );
	u8	AddressL   = 0xff & Address;

	u8	reg;

	lt8911_write_bytes( 0xff, 0xa6 );
	lt8911_write_bytes( 0x2b, ( 0x80 | AddressH ) );  //CMD
	lt8911_write_bytes( 0x2b, AddressM );             //addr[15:8]
	lt8911_write_bytes( 0x2b, AddressL );             //addr[7:0]
	lt8911_write_bytes( 0x2b, 0x00 );                 //data lenth
	lt8911_write_bytes( 0x2b, Data );                 //data
	lt8911_write_bytes( 0x2c, 0x00 );                 //start Aux

	msleep( 20 );                                         //more than 10ms
	reg = lt8911_read_bytes( 0x25 );

	if( ( reg & 0x0f ) == 0x0c )
	{
		return;
	}
}

//------------------------------------------------------------------

u8 DpcdRead( u32 Address )
{
	/***************************
	   注意大小端的问题!
	   这里默认是大端模式

	   Pay attention to the Big-Endian and Little-Endian!
	   The default mode is Big-Endian here.

	 ****************************/

	u8	DpcdValue  = 0x00;
	u8	AddressH   = 0x0f & ( Address >> 16 );
	u8	AddressM   = 0xff & ( Address >> 8 );
	u8	AddressL   = 0xff & Address;
	u8	reg;

	lt8911_write_bytes( 0xff, 0xac );
	lt8911_write_bytes( 0x00, 0x20 );                 //Soft Link train
	lt8911_write_bytes( 0xff, 0xa6 );
	lt8911_write_bytes( 0x2a, 0x01 );

	lt8911_write_bytes( 0xff, 0xa6 );
	lt8911_write_bytes( 0x2b, ( 0x90 | AddressH ) );  //CMD
	lt8911_write_bytes( 0x2b, AddressM );             //addr[15:8]
	lt8911_write_bytes( 0x2b, AddressL );             //addr[7:0]
	lt8911_write_bytes( 0x2b, 0x00 );                 //data lenth
	lt8911_write_bytes( 0x2c, 0x00 );                 //start Aux read edid

	msleep( 50 );                                         //more than 10ms
	reg = lt8911_read_bytes( 0x25 );
	if( ( reg & 0x0f ) == 0x0c )
	{
		if( lt8911_read_bytes( 0x39 ) == 0x22 )
		{
			lt8911_read_bytes( 0x2b );
			DpcdValue = lt8911_read_bytes( 0x2b );
		}


		/*
		   else
		   {
		   //	goto no_reply;
		   //	DpcdValue = 0xff;
		   return DpcdValue;
		   }//*/
	}else
	{
		lt8911_write_bytes( 0xff, 0x81 ); // change bank
		lt8911_write_bytes( 0x07, 0xfe );
		lt8911_write_bytes( 0x07, 0xff );
		lt8911_write_bytes( 0x0a, 0xfc );
		lt8911_write_bytes( 0x0a, 0xfe );
	}

	return DpcdValue;
}

//------------------------------------------------------------------

void LT8911EX_link_train( void )
{
	lt8911_write_bytes( 0xff, 0x81 );
	lt8911_write_bytes( 0x06, 0xdf ); // rset VID TX
	lt8911_write_bytes( 0x06, 0xff );

	lt8911_write_bytes( 0xff, 0x85 );

//	lt8911_write_bytes( 0x17, 0xf0 ); // turn off scramble

//#ifdef _eDP_scramble_
	if( ScrambleMode )
	{
		lt8911_write_bytes( 0xa1, 0x82 ); // eDP scramble mode;

		/* Aux operater init */

//*
		lt8911_write_bytes( 0xff, 0xac );
		lt8911_write_bytes( 0x00, 0x20 ); //Soft Link train
		lt8911_write_bytes( 0xff, 0xa6 );
		lt8911_write_bytes( 0x2a, 0x01 );

		DpcdWrite( 0x010a, 0x01 );
		msleep( 10 );
		DpcdWrite( 0x0102, 0x00 );
		msleep( 10 );
		DpcdWrite( 0x010a, 0x01 );

		msleep( 200 );
		//*/
	}
//#else
	else
	{
		lt8911_write_bytes( 0xa1, 0x02 ); // DP scramble mode;
	}
//#endif

	/* Aux setup */
	lt8911_write_bytes( 0xff, 0xac );
	lt8911_write_bytes( 0x00, 0x60 );     //Soft Link train
	lt8911_write_bytes( 0xff, 0xa6 );
	lt8911_write_bytes( 0x2a, 0x00 );

	lt8911_write_bytes( 0xff, 0x81 );
	lt8911_write_bytes( 0x07, 0xfe );
	lt8911_write_bytes( 0x07, 0xff );
	lt8911_write_bytes( 0x0a, 0xfc );
	lt8911_write_bytes( 0x0a, 0xfe );

	/* link train */

	lt8911_write_bytes( 0xff, 0x85 );
	lt8911_write_bytes( 0x1a, eDP_lane );

#ifdef _link_train_enable_
	lt8911_write_bytes( 0xff, 0xac );
	lt8911_write_bytes( 0x00, 0x64 );
	lt8911_write_bytes( 0x01, 0x0a );
	lt8911_write_bytes( 0x0c, 0x85 );
	lt8911_write_bytes( 0x0c, 0xc5 );
#else
	lt8911_write_bytes( 0xff, 0xac );
	lt8911_write_bytes( 0x00, 0x00 );
	lt8911_write_bytes( 0x01, 0x0a );
	lt8911_write_bytes( 0x14, 0x80 );
	lt8911_write_bytes( 0x14, 0x81 );
	msleep( 50 );
	lt8911_write_bytes( 0x14, 0x84 );
	msleep( 50 );
	lt8911_write_bytes( 0x14, 0xc0 );
#endif
}

//-------------------------------------------

//*
void LT8911EX_link_train_result( void )
{
	u8 i, reg;
	lt8911_write_bytes( 0xff, 0xac );
	for( i = 0; i < 10; i++ )
	{
		reg = lt8911_read_bytes( 0x82 );
		//  Debug_DispStrNum( "\r\n0x82 = ", reg );
		if( reg & 0x20 )
		{
			if( ( reg & 0x1f ) == 0x1e )
			{
				printk( "Link train success\n");
			} else
			{
				printk( "Link train fail\n");
			}

			// printk( "\r\npanel link rate: ", lt8911_read_bytes( 0x83 ) );
			// printk( "\r\npanel link count: ", lt8911_read_bytes( 0x84 ) );
			return;
		}else
		{
			printk( "\r\nlink trian on going..." );
		}
		msleep( 50 );
	}
}

//*/

void LT8911EXB_MainLoop( void )
{
#ifndef _Test_Pattern_
	u16 reg;

	lt8911_write_bytes( 0xff, 0x85 );
	//lt8911_write_bytes(0x1d,0x00); //FM select byte clk
	//lt8911_write_bytes(0x40,0xf7);
	//lt8911_write_bytes(0x41,0x30);

	if( ScrambleMode )
	{
		lt8911_write_bytes( 0xa1, 0x82 ); //
	}else
	{
		lt8911_write_bytes( 0xa1, 0x02 ); //
	}

	lt8911_write_bytes( 0xff, 0x81 );     //video check rst
	lt8911_write_bytes( 0x09, 0x7d );
	lt8911_write_bytes( 0x09, 0xfd );
	msleep( 50 );

	lt8911_write_bytes( 0xff, 0x85 );
	reg	   = lt8911_read_bytes( 0x76 );
	reg	   = reg * 256 + lt8911_read_bytes( 0x77 );

//	if( reg == MIPI_Timing[vtotal] )
	if( ( reg > ( MIPI_Timing[vtotal] - 5 ) ) && ( reg < ( MIPI_Timing[vtotal] + 5 ) ) )
	{
		if( !flag_mipi_on )
		{
			lt8911_write_bytes( 0xff, 0x81 ); //PCR reset
			lt8911_write_bytes( 0x03, 0x7b );
			lt8911_write_bytes( 0x03, 0xff );

			lt8911_write_bytes( 0xff, 0xa8 );
			lt8911_write_bytes( 0x2d, 0x88 );
			flag_mipi_on = 1;
			printk( "\r\nPCR reset" );
		}
	}else
	{
		lt8911_write_bytes( 0xff, 0xa8 );
		lt8911_write_bytes( 0x2d, 0x8c ); //edp output idle pattern;
		flag_mipi_on = 0;
	}

#ifdef _uart_debug_
	lt8911_write_bytes( 0xff, 0xd0 );
	reg = lt8911_read_bytes( 0x87 );
//	reg	   = reg * 256 + HDMI_ReadI2C_Byte( 0x77 );

	printk( "\r\nReg0xD087 =  " );
	printk( " ", reg );
	printk( "\r\n " );
	if( reg & 0x10 )
	{
		printk( "\r\nPCR Clock stable" );
	}else
	{
		printk( "\r\nPCR Clock unstable" );
	}
	printk( "\r\n " );
#endif

#endif
}

/***********************************************************/


/*
   reg 0x822/0x8226 : bit1 bit0
   Da_edptx0_tap0_current_tune software output set value:

   2'b00 = None;
   2'b01 = 12.8mA;
   2'b10 = 15mA

   //--------------------------//

   reg 0x8223/ 0x8227 bit7 ~ bit0
   Da_edptx_tap0_current_tune software output set value:

   8'b00000000 = None;
   8'b00000001 = 50uA;
   8'b00000010 = 100uA;
   8'b00000100 = 200uA;
   8'b00001000 = 400uA;
   8'b00010000 = 800uA;
   8'b00100000 = 1.6mA;
   8'b01000000 = 3.2mA;
   8'b10000000 = 6.4mA

   //--------------------------//

 */
enum
{
	_Level0_ = 0,                                               // 27.8 mA  0x83/0x00
	_Level1_,                                                   // 26.2 mA  0x82/0xe0
	_Level2_,                                                   // 24.6 mA  0x82/0xc0
	_Level3_,                                                   // 23 mA    0x82/0xa0
	_Level4_,                                                   // 21.4 mA  0x82/0x80
	_Level5_,                                                   // 18.2 mA  0x82/0x40
	_Level6_,                                                   // 16.6 mA  0x82/0x20
	_Level7_,                                                   // 15mA     0x82/0x00  // level 1
	_Level8_,                                                   // 12.8mA   0x81/0x00  // level 2
	_Level9_,                                                   // 11.2mA   0x80/0xe0  // level 3
	_Level10_,                                                  // 9.6mA    0x80/0xc0  // level 4
	_Level11_,                                                  // 8mA      0x80/0xa0  // level 5
	_Level12_,                                                  // 6mA      0x80/0x80  // level 6
};

u8	Swing_Setting1[] = { 0x83, 0x82, 0x82, 0x82, 0x82, 0x82, 0x82, 0x82, 0x81, 0x80, 0x80, 0x80, 0x80 };
u8	Swing_Setting2[] = { 0x00, 0xe0, 0xc0, 0xa0, 0x80, 0x40, 0x20, 0x00, 0x00, 0xe0, 0xc0, 0xa0, 0x80 };

u8	Level = _Level7_;                                           // normal


/***********************************************************

***********************************************************/
void LT8911EX_TxSwingPreSet( void )
{
	lt8911_write_bytes( 0xFF, 0x82 );
	unsigned char reg22_29[] = {
		0x22,Swing_Setting1[Level], Swing_Setting2[Level], 0x80, 0x00, Swing_Setting1[Level], Swing_Setting2[Level], 0x80, 0x00
	};	
	lt8911_write_block(reg22_29, sizeof(reg22_29));		

}

/***********************************************************

***********************************************************/
void PCR_Status( void )                                         // for debug
{
#ifdef _uart_debug_
	u8 reg;

	lt8911_write_bytes( 0xff, 0xd0 );
	reg = lt8911_read_bytes( 0x87 );

	printk( "\r\nReg0xD087 =	");
	printk( " ", reg );
	printk( "\r\n " );
	if( reg & 0x10 )
	{
		printk( "\r\nPCR Clock stable" );
	}else
	{
		printk( "\r\nPCR Clock unstable" );
	}
	printk( "\r\n " );
#endif
}

/***********************************************************

***********************************************************/
void LT8911_MainLoop( void )
{
	u16 reg;//H_act, V_act;
//	bool	flag_mipi_on = 0;

	lt8911_write_bytes( 0xff, 0x85 );

	if( ScrambleMode )
	{
		lt8911_write_bytes( 0xa1, 0x82 ); //video check from mipi
	}else
	{
		lt8911_write_bytes( 0xa1, 0x02 );
	}

	lt8911_write_bytes( 0xff, 0x81 );     //video check rst
	lt8911_write_bytes( 0x09, 0x7d );
	lt8911_write_bytes( 0x09, 0xfd );
	msleep( 50 );

	/* mipi word count check*/
	lt8911_write_bytes( 0xff, 0xd0 );
	reg	   = lt8911_read_bytes( 0x82 );
	reg	   = reg * 256 + lt8911_read_bytes( 0x83 );
	// H_act  = reg / 3;

#ifdef _uart_debug_

	printk( "\r\nHact(word counter) =  " ); // H active = word counter / 3
	printk( H_act );
	printk( "\r\n " );

#endif

	/* mipi Vact check*/
	reg	   = lt8911_read_bytes( 0x85 );
	// V_act  = reg * 256 + lt8911_read_bytes( 0x86 );

#ifdef _uart_debug_

	printk( "\r\nVact =  " );
	printk( V_act );
	printk( "\r\n " );
#endif

	lt8911_write_bytes( 0xff, 0x85 );
	reg	   = lt8911_read_bytes( 0x76 );
	reg	   = reg * 256 + lt8911_read_bytes( 0x77 );

#ifdef _uart_debug_
	printk( "\r\nvideo check: Vtotal =  " );
	printk( reg );
	printk( "\r\n " );
#endif

//	if( reg == MIPI_Timing[vtotal] )
	if( ( reg > ( MIPI_Timing[vtotal] - 5 ) ) && ( reg < ( MIPI_Timing[vtotal] + 5 ) ) )
	{
		if( !flag_mipi_on )
		{
			lt8911_write_bytes( 0xff, 0x81 ); //PCR reset
			lt8911_write_bytes( 0x03, 0x7b );
			lt8911_write_bytes( 0x03, 0xff );

			lt8911_write_bytes( 0xff, 0xa8 );
			lt8911_write_bytes( 0x2d, 0x88 );
			flag_mipi_on = 1;
#ifdef _uart_debug_
			printk( "\r\nPCR reset" );
#endif
		}
	}else
	{
		lt8911_write_bytes( 0xff, 0xa8 );
		lt8911_write_bytes( 0x2d, 0x8c ); //edp output idle pattern;
		flag_mipi_on = 0;
	}

#ifdef _uart_debug_
	lt8911_write_bytes( 0xff, 0xd0 );
	reg = lt8911_read_bytes( 0x87 );

	printk( "\r\nReg0xD087 =  " );
	printk( " ", reg );
	printk( "\r\n " );
	if( reg & 0x10 )
	{
		printk( "\r\nPCR Clock stable" );
	}else
	{
		printk( "\r\nPCR Clock unstable" );
	}
	printk( "\r\n " );
#endif
}

/***********************************************************/

void LT8911EXB_LinkTrainResultCheck( void )
{
#ifdef _link_train_enable_
	u8	i;
	u8	val;
	//int ret;

	lt8911_write_bytes( 0xff, 0xac );
	for( i = 0; i < 10; i++ )
	{
		val = lt8911_read_bytes( 0x82 );
		if( val & 0x20 )
		{
			if( ( val & 0x1f ) == 0x1e )
			{
#ifdef _uart_debug_
				//   printk("\r\nLT8911_LinkTrainResultCheck: edp link train successed: 0x%bx", val);
				printk( "\r\nedp link train successed: ", val );
#endif
				return;
			}else
			{
#ifdef _uart_debug_
				//printk("\r\nLT8911_LinkTrainResultCheck: edp link train failed: 0x%bx", val);
				printk( "\r\nedp link train failed: ", val );
#endif
				lt8911_write_bytes( 0xff, 0xac );
				lt8911_write_bytes( 0x00, 0x00 );
				lt8911_write_bytes( 0x01, 0x0a );
				lt8911_write_bytes( 0x14, 0x80 );
				lt8911_write_bytes( 0x14, 0x81 );
				msleep( 50 );
				lt8911_write_bytes( 0x14, 0x84 );
				msleep( 50 );
				lt8911_write_bytes( 0x14, 0xc0 );
				//printk("\r\nLT8911_LinkTrainResultCheck: Enable eDP video output while linktrian fail");
			}

#ifdef _uart_debug_

			val = lt8911_read_bytes( 0x83 );
			//printk("\r\nLT8911_LinkTrainResultCheck: panel link rate: 0x%bx",val);
			printk( "\r\npanel link rate: ", val );
			val = lt8911_read_bytes( 0x84 );
			//printk("\r\nLT8911_LinkTrainResultCheck: panel link count: 0x%bx",val);
			printk( "\r\npanel link count: ", val );
#endif
			msleep( 100 ); // return;
		}else
		{
			//printk("\r\nLT8911_LinkTrainResultCheck: link trian on going...");
			msleep( 100 );
		}
	}
#endif
}

//----------------------------------------
struct completion lt8911_config_complete;
int lt8911_config_finished = 0;
// 原有的LT8911EXB_config函数保持不变，但重命名为lt8911_do_config
static void lt8911_do_config(void)
{
    printk("lt8911_do_config start!!!\n");
    
    //LT8911EXB_eDP_Video_cfg( );
    LT8911EXB_init( );

    ScrambleMode = 0;

    LT8911EX_TxSwingPreSet( );
    LT8911EX_link_train( );
    //LT8911EXB_LinkTrainResultCheck( );
    // LT8911EX_link_train_result( );
    //LT8911EXB_video_check( );
    //PCR_Status( );
    lt8911_config_finished = 1;
    complete_all(&lt8911_config_complete);
    printk("lt8911_do_config end!!!\n");
}
EXPORT_SYMBOL(lt8911_config_finished);
void wait_lt8911_config_complete(void)
{
    if (!lt8911_config_finished) {
        printk("Waiting for LT8911 config to complete...\n");
        wait_for_completion(&lt8911_config_complete);
    }
}
EXPORT_SYMBOL(wait_lt8911_config_complete);
EXPORT_SYMBOL(lt8911_config_complete);
// 新增异步处理函数
static void lt8911_config_work_func(struct work_struct *work)
{
    printk("Starting LT8911 async config\n");
    lt8911_do_config();
    printk("LT8911 async config completed\n");
}



void LT8911EXB_config( void )
{

	printk("LT8911EXB_config start!!!\n");
    // 取消之前可能还在排队的工作
    cancel_delayed_work_sync(&lt8911_config_work);
    
    // 延迟一小段时间让系统稳定后再执行配置
    queue_delayed_work(lt8911_config_wq, &lt8911_config_work, msecs_to_jiffies(50));
	printk("LT8911EXB_config end!!!\n");
//======================================//
}
EXPORT_SYMBOL(LT8911EXB_config);
/************************************** The End Of File **************************************/

static int lt8911_probe(struct i2c_client *client)
{
	printk("lt8911_iic_probe\n");
	printk("TPS: info==>name=%s addr=0x%x\n", client->name, client->addr);
	lt8911_i2c_client = client;

    // 初始化工作队列和工作项
    lt8911_config_wq = create_singlethread_workqueue("lt8911_config");
    if (!lt8911_config_wq) {
        printk(KERN_ERR "Failed to create lt8911 config workqueue\n");
        return -ENOMEM;
    }
    
    INIT_DELAYED_WORK(&lt8911_config_work, lt8911_config_work_func);	
	
	return 0;
}

static void lt8911_remove(struct i2c_client *client)
{
	printk("lt8911_remove\n");
	
    // 清理工作队列
    if (lt8911_config_wq) {
        cancel_delayed_work_sync(&lt8911_config_work);
        destroy_workqueue(lt8911_config_wq);
        lt8911_config_wq = NULL;
    }
	
	lt8911_i2c_client = NULL;
	i2c_unregister_device(client);

}

static const struct i2c_device_id lt8911_id[] = {
	{I2C_ID_NAME, 0},
	{}
};


static struct i2c_driver lt8911_iic_driver = {
	.id_table = lt8911_id,
	.probe = lt8911_probe,
	.remove = lt8911_remove,
	/* .detect               = mt6605_detect, */
	.driver = {
		   .owner = THIS_MODULE,
		   .name = "lt8911",
			.of_match_table = lcm_of_match,
		   },
};


static int __init lt8911_iic_init(void)
{
	printk("lt8911_iic_init\n");
	init_completion(&lt8911_config_complete);
	i2c_add_driver(&lt8911_iic_driver);
	printk("lt8911_iic_init success\n");
	return 0;
}

static void __exit lt8911_iic_exit(void)
{
	printk("lt8911_iic_exit\n");
	i2c_del_driver(&lt8911_iic_driver);
}


module_init(lt8911_iic_init);
module_exit(lt8911_iic_exit);

MODULE_AUTHOR("Mike Liu");
MODULE_DESCRIPTION("MTK lt8911 I2C Driver");
MODULE_LICENSE("GPL");

