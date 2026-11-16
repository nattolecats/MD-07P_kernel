#ifndef TMS_MIPI_SENSOR_PARA_H
#define TMS_MIPI_SENSOR_PARA_H

#include "tms_mipi_Sensor.h"

#include "kd_imgsensor_define.h"

struct TMSDeviceConfig
{
    uint16_t id;
    uint16_t id_reg_start; // id==*start<<8 | *(start+1)
    struct imgsensor_info_def imgsensor_info;
    struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info;
};

extern struct TMSDeviceConfig tms_device_configs[];
extern const int tms_device_configs_len;

extern int matched_device_index;
extern u16 i2c_write_addr;

#endif
