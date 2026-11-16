#ifndef __TMSMIPI_SENSOR_H__
#define __TMSMIPI_SENSOR_H__

#include "kd_camera_typedef.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"

/************************** Modify Following Strings for Debug **************************/
#define PFX "tms_camera_sensor"
/****************************   Modify end    *******************************************/
#define TMS_DEBUG 1
#if TMS_DEBUG
#define LOG_INFO(fmt, args...) printk(KERN_DEBUG "[%s:D:%s] " fmt "\n", PFX, __func__, ##args)
#else
#define LOG_INFO(fmt, args...)
#endif

/* SENSOR PRIVATE STRUCT FOR CONSTANT */
struct imgsensor_info_def
{
    kal_uint32 pclk;
    kal_uint32 line_length;
    kal_uint32 frame_length;
    kal_uint8 startx;
    kal_uint8 starty;
    kal_uint16 grabwindow_width;
    kal_uint16 grabwindow_height;
    kal_uint32 mipi_pixel_rate;
    kal_uint16 max_framerate;

    kal_uint8 sensor_output_dataformat;
    kal_uint8 mipi_lane_num;
    kal_uint8 i2c_addr_table[3];
};

extern int iReadRegI2CTiming(u8 *a_pSendData, u16 a_sizeSendData, u8 *a_pRecvData, u16 a_sizeRecvData, u16 i2cId, u16 timing);
extern int iWriteRegI2CTiming(u8 *a_pSendData, u16 a_sizeSendData, u16 i2cId, u16 timing);

#define TMS_PROPERTIES_VENSION 1
struct tms_engine_properties
{
    uint32_t version;
    uint32_t sensor_id;
    uint16_t sensor_addr;
};

struct tms_msg_data
{
    uint16_t addr;
    uint8_t reg_len; // 1: 8bit or 2: 16bit
    uint32_t len;    // read: max length of data buffer; write: length of data to write
    uint8_t *buff;   // read: data buffer; write: data to write
};
struct tms_msg
{
    /*
    0x00: read raw, 0x01: write raw;
    0x10: read protocol, 0x11: write protocol;
    */
    uint8_t tag;              // msg_tag
    uint16_t addr;            // target i2c addr
    struct tms_msg_data data; // sensor: tms_msg_regval; soc: tms_msg_data
};

enum tms_msg_tag
{
    RD_RAW = 0x00,
    WR_RAW,
    RD_PROTOCOL = 0x10,
    WR_PROTOCOL,
};

#endif
