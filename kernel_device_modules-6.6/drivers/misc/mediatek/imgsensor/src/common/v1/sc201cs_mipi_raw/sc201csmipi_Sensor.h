/*****************************************************************************
 *
 * Filename:
 * ---------
 *     sc201csmipi_Sensor.h
 *
 * Project:
 * --------
 *     ALPS
 *
 * Description:
 * ------------
 *     CMOS sensor header file
 *
 ****************************************************************************/
#ifndef _SC201CSMIPI_SENSOR_H
#define _SC201CSMIPI_SENSOR_H

/* SENSOR MIRROR FLIP INFO */
#define SC201CS_MIRROR_NORMAL    1
#define SC201CS_MIRROR_H         0
#define SC201CS_MIRROR_V         0
#define SC201CS_MIRROR_HV        0

#if SC201CS_MIRROR_NORMAL
#define SC201CS_MIRROR	        0x00
#elif SC201CS_MIRROR_H
#define SC201CS_MIRROR	        0x06
#elif SC201CS_MIRROR_V
#define SC201CS_MIRROR	        0x60
#elif SC201CS_MIRROR_HV
#define SC201CS_MIRROR	        0x66
#else
#define SC201CS_MIRROR	        0x00
#endif


/* SENSOR PRIVATE INFO FOR GAIN SETTING */
#define SC201CS_SENSOR_GAIN_BASE             0x400
#define SC201CS_SENSOR_GAIN_MAX              (3175 * SC201CS_SENSOR_GAIN_BASE /100)
#define SC201CS_SENSOR_GAIN_MAX_VALID_INDEX  5  
//#define SC201CS_SENSOR_DGAIN_BASE          0x400

typedef enum{
	IMGSENSOR_MODE_INIT,
	IMGSENSOR_MODE_PREVIEW,
	IMGSENSOR_MODE_CAPTURE,
	IMGSENSOR_MODE_VIDEO,
	IMGSENSOR_MODE_HIGH_SPEED_VIDEO,
	IMGSENSOR_MODE_SLIM_VIDEO,
} IMGSENSOR_MODE;


typedef struct imgsensor_mode_struct {
	kal_uint32 pclk;					//record different mode's pclk
	kal_uint32 linelength;				//record different mode's linelength
	kal_uint32 framelength;				//record different mode's framelength
	
	kal_uint8 startx;					//record different mode's startx of grabwindow
	kal_uint8 starty;					//record different mode's startx of grabwindow
	kal_uint16 grabwindow_width;
	kal_uint16 grabwindow_height;
	kal_uint8 mipi_data_lp2hs_settle_dc;
	kal_uint16 max_framerate;
} imgsensor_mode_struct;

/* SENSOR PRIVATE STRUCT FOR VARIABLES*/
typedef struct imgsensor_struct {
	kal_uint8	mirror;								//mirrorflip information
	
	kal_uint8	sensor_mode;						//record IMGSENSOR_MODE enum value
	kal_uint32 shutter;
	kal_uint16 gain;
	kal_uint32 pclk;
	kal_uint32 frame_length;
	kal_uint32 line_length;
	kal_uint32 min_frame_length;
	kal_uint16 dummy_pixel;
	kal_uint16	dummy_line;							//current dummline
	
	kal_uint16	current_fps;						//current max fps
	kal_bool	autoflicker_en;						//record autoflicker enable or disable
	kal_bool	test_pattern;						//record test pattern mode or not
	enum MSDK_SCENARIO_ID_ENUM	current_scenario_id;	//current scenario id
	kal_uint8	ihdr_en;							//ihdr enable or disable
	
	kal_uint8	i2c_write_id;						//record current sensor's i2c write id
} imgsensor_struct;

/* SENSOR PRIVATE STRUCT FOR CONSTANT */
typedef struct imgsensor_info_struct {
	kal_uint32	sensor_id;							//record sensor id defined in Kd_imgsensor.h
	kal_uint32	checksum_value;						//checksum value for Camera Auto Test
	struct imgsensor_mode_struct	pre;					//preview scenario relative information
	struct imgsensor_mode_struct	cap;					//capture scenario relative information
	struct imgsensor_mode_struct	cap1;					//capture for PIP 24fps relative information, capture1 mode must use same framelength, linelength with Capture mode for shutter calculate
	struct imgsensor_mode_struct	normal_video;			//normal video  scenario relative information
	struct imgsensor_mode_struct	hs_video;				//high speed video scenario relative information
	struct imgsensor_mode_struct	slim_video;				//slim video for VT scenario relative information
	
	kal_uint8	ae_shut_delay_frame;				//shutter delay frame for AE cycle
	kal_uint8	ae_sensor_gain_delay_frame;			//sensor gain delay frame for AE cycle
	kal_uint8	ae_ispGain_delay_frame;				//isp gain delay frame for AE cycle
	kal_uint8  ihdr_support;
	kal_uint8  ihdr_le_firstline;
	kal_uint8  sensor_mode_num;
	kal_uint8  cap_delay_frame;
	kal_uint8  pre_delay_frame;
	kal_uint8  video_delay_frame;
	kal_uint8  hs_video_delay_frame;
	kal_uint8  slim_video_delay_frame;
	kal_uint8  margin;
	kal_uint32 min_shutter;
	kal_uint32 max_frame_length;
	kal_uint8  isp_driving_current;
	kal_uint8  sensor_interface_type;
	kal_uint8  mipi_sensor_type;
	kal_uint8  mipi_settle_delay_mode;
	kal_uint8  sensor_output_dataformat;
	kal_uint8  mclk;
	kal_uint8  mipi_lane_num;
	kal_uint8  i2c_addr_table[5];
	kal_uint32 i2c_speed;
}imgsensor_info_struct;

extern int iReadRegI2C(u8 *a_pSendData, u16 a_sizeSendData, u8 *a_pRecvData, u16 a_sizeRecvData, u16 i2cId);
extern int iWriteRegI2C(u8 *a_pSendData, u16 a_sizeSendData, u16 i2cId);
extern int iWriteReg(u16 a_u2Addr, u32 a_u4Data, u32 a_u4Bytes, u16 i2cId);
extern void kdSetI2CSpeed(u16 i2cSpeed);
extern int iBurstWriteReg(u8 *pData, u32 bytes, u16 i2cId);
extern int iBurstWriteReg_multi(u8 *pData, u32 bytes, u16 i2cId, u16 transfer_length, u16 timing);
extern int iWriteRegI2CTiming(u8 *a_pSendData, u16 a_sizeSendData, u16 i2cId, u16 timing);

#endif


