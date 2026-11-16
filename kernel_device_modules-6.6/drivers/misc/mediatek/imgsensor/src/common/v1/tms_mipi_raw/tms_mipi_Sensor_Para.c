#include "tms_mipi_Sensor_Para.h"

#define SC132GS_MIPI_SENSOR_ID 0x0132
#define SC132GS_REG_CHIP_ID 0x3107

#define OV9281_MIPI_SENSOR_ID 0x9281
#define OV9281_REG_CHIP_ID 0x300A

const int tms_device_configs_len = 2;
struct TMSDeviceConfig tms_device_configs[] = {
    {
        .id = SC132GS_MIPI_SENSOR_ID,
        .id_reg_start = SC132GS_REG_CHIP_ID,
        .imgsensor_info = {
            .pclk = 48000000,
            .line_length = 0x2ee,
            .frame_length = 0x578,
            .startx = 0,
            .starty = 0,
            .grabwindow_width = 800,
            .grabwindow_height = 1280,
            .mipi_pixel_rate = 48000000,
            .max_framerate = 600,

            .sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW8_MONO,
            .mipi_lane_num = SENSOR_MIPI_1_LANE,
            .i2c_addr_table = {0x60, 0xff},
        },
        .imgsensor_winsize_info = {800, 1280, 0, 0, 800, 1280, 800, 1280, 0, 0, 800, 1280, 0, 0, 800, 1280},
    },
    {
        .id = OV9281_MIPI_SENSOR_ID,
        .id_reg_start = OV9281_REG_CHIP_ID,
        .imgsensor_info = {
            .pclk = 640000000,
            .line_length = 0xb60,
            .frame_length = 0x780,
            .startx = 0,
            .starty = 0,
            .grabwindow_width = 1280,
            .grabwindow_height = 800,
            .mipi_pixel_rate = 640000000,
            .max_framerate = 600,

            .sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW8_MONO,
            .mipi_lane_num = SENSOR_MIPI_1_LANE,
            .i2c_addr_table = {0xc0, 0x20, 0xff},
        },
        .imgsensor_winsize_info = {1280, 800, 0, 0, 1280, 800, 1280, 800, 0, 0, 1280, 800, 0, 0, 1280, 800},
    },
};
