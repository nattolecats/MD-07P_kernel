#include <linux/videodev2.h>
#include <linux/syscalls.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/atomic.h>
#include <linux/types.h>

#include <linux/miscdevice.h>
#include <linux/vmalloc.h>

#include <linux/slab.h> //kmalloc

#include "tms_mipi_Sensor.h"
#include "tms_mipi_Sensor_Para.h"

#define __INNER_TMS_GET_PROPERTIES (SENSOR_FEATURE_START - 10)
#define __INNER_TMS_IIC_RDWR (SENSOR_FEATURE_START - 11)

#define TMS_GET_PROPERTIES (BASE_VIDIOC_PRIVATE + 1)
#define TMS_IIC_RDWR (BASE_VIDIOC_PRIVATE + 2)

#define TMS_SCN_DATA_SIZE 1024
struct tms_scn_dev
{
    struct miscdevice *dev;
    struct mutex mutex;
    unsigned int data_len;
    __u8 *inner;
};

static struct tms_engine_properties tms_props;
uint16_t i2c_write_addr = 0;
int matched_device_index = 0;

struct tms_scn_dev *tms_scn_devp;
static int is_device_registered = 0;
static int is_dev_tmsscn = 0;
static uint8_t check_device_addr = 0x40;

static int tms_scn_open(struct inode *inode, struct file *filp)
{
    filp->private_data = tms_scn_devp;
    return 0;
}
static int tms_scn_release(struct inode *inode, struct file *filp)
{
    return 0;
}
static long tms_scn_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    return 0;
}

static ssize_t tms_scn_read(struct file *filp, char __user *buf, size_t size, loff_t *ppos)
{
    unsigned int count = size;
    int ret = 0;
    struct tms_scn_dev *dev = filp->private_data;

    if (count > TMS_SCN_DATA_SIZE)
    {
        count = TMS_SCN_DATA_SIZE;
        printk(KERN_INFO "TMSSCN read too long, set to %u\n", count);
    }

    if (dev->data_len < 1)
        return 0;
    if (count > dev->data_len)
    {
        count = dev->data_len;
        printk(KERN_INFO "TMSSCN read too long, set to %u\n", count);
    }

    mutex_lock(&dev->mutex);
    if (copy_to_user(buf, dev->inner, count))
    {
        ret = -EFAULT;
    }
    else
    {
        ret = count;
        printk(KERN_INFO "TMSSCN read %u bytes(s)\n", count);
    }
    mutex_unlock(&dev->mutex);

    return ret;
}

static ssize_t tms_scn_write(struct file *filp, const char __user *buf, size_t size, loff_t *ppos)
{
    unsigned int count = size;
    int ret = 0;
    struct tms_scn_dev *dev = filp->private_data;

    if (count > TMS_SCN_DATA_SIZE)
        count = TMS_SCN_DATA_SIZE;

    mutex_lock(&dev->mutex);
    if (copy_from_user(dev->inner, buf, count))
        ret = -EFAULT;
    else
    {
        ret = count;
        dev->data_len = count;
        printk(KERN_INFO "TMSSCN written %u bytes\n", count);
    }
    mutex_unlock(&dev->mutex);

    return ret;
}

static const struct file_operations tms_scn_misc_fops = {
    .read = tms_scn_read,
    .write = tms_scn_write,
    .unlocked_ioctl = tms_scn_ioctl,
    .open = tms_scn_open,
    .release = tms_scn_release,
};

static struct miscdevice tms_scn_misc_device = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "tms_scn",
    .fops = &tms_scn_misc_fops,
};

static int register_device(void)
{
    tms_scn_devp = kzalloc(sizeof(struct tms_scn_dev), GFP_KERNEL);
    tms_scn_devp->inner = (unsigned char *)vmalloc(TMS_SCN_DATA_SIZE);
    if (!tms_scn_devp || !tms_scn_devp->inner)
    {
        return -ENOMEM;
    }

    memcpy(tms_scn_devp->inner, &tms_props, sizeof(tms_props));
    tms_scn_devp->data_len = sizeof(tms_props);
    tms_scn_devp->dev = &tms_scn_misc_device;

    mutex_init(&tms_scn_devp->mutex);
    return misc_register(tms_scn_devp->dev);
}

#define iReadRegI2C(a_pSendData, a_sizeSendData, a_pRecvData, a_sizeRecvData, i2cId) iReadRegI2CTiming(a_pSendData, a_sizeSendData, a_pRecvData, a_sizeRecvData, i2cId, 400)
#define iWriteRegI2C(a_pSendData, a_sizeSendData, i2cId) iWriteRegI2CTiming(a_pSendData, a_sizeSendData, i2cId, 400)

static kal_uint8 read_cmos_sensor(kal_uint16 addr)
{
    kal_uint8 get_byte = 0;

    u8 pu_send_cmd[2] = {(u8)(addr >> 8), (u8)addr};
    kal_uint16 i2c_addr = i2c_write_addr;
    LOG_INFO("tms read sensor: %x, %x", addr, i2c_addr);

    iReadRegI2C(pu_send_cmd, 2, &get_byte, 1, i2c_addr);

    return get_byte;
}

static void write_cmos_sensor(kal_uint32 addr, kal_uint32 arg)
{
    u8 pu_send_cmd[3] = {(u8)(addr >> 8), (u8)addr, (u8)arg};

    kal_uint16 i2c_addr = i2c_write_addr;
    LOG_INFO("tms write sensor: %x, %x", addr, i2c_addr);
    iWriteRegI2C(pu_send_cmd, 3, i2c_addr);
}

static kal_uint16 return_sensor_id(kal_uint16 reg_start)
{
    kal_uint16 sensor_id = read_cmos_sensor(reg_start);
    sensor_id <<= 8;
    sensor_id |= read_cmos_sensor(reg_start + 1);
    return sensor_id;
}

static int check_device(void)
{
    const uint8_t check_tms_device_cmd[] = {0x03, 0x00, 0x36, 0x00, 0x00, 0x36};
    uint16_t check_tms_device_data[2] = {0};
    kal_uint8 i, j, k;
    kal_uint16 sensor_id = 0;
    int matched_device_index = -1;

    int length = tms_device_configs_len;
    LOG_INFO("check device, count: %d", length);

    for (j = 0; j < length; j++)
    {
        for (i = 0; tms_device_configs[j].imgsensor_info.i2c_addr_table[i] != 0xff; i++)
        {
            i2c_write_addr = tms_device_configs[j].imgsensor_info.i2c_addr_table[i];

            sensor_id = return_sensor_id(tms_device_configs[j].id_reg_start);
            if (sensor_id == tms_device_configs[j].id)
            {
                matched_device_index = j + 1;
                LOG_INFO("i2c write id: 0x%x, sensor id: 0x%x, sensor index: %d", i2c_write_addr, sensor_id, matched_device_index);

                if (!!is_dev_tmsscn)
                {
                    return matched_device_index;
                }

                // check device
                LOG_INFO("check tms device: %X @ %X", i2c_write_addr, check_device_addr);
                if (iWriteRegI2C((uint8_t *)&check_tms_device_cmd, sizeof(check_tms_device_cmd), check_device_addr << 1) >= 0)
                {
                    for (k = 3; k > 0; k--)
                    {
                        if (iReadRegI2C((uint8_t *)&check_tms_device_data, 2, (uint8_t *)&check_tms_device_data, 4, check_device_addr << 1) >= 0)
                        {
                            if ((check_tms_device_data[0] > 0) || ((check_tms_device_data[0] == 0x0000) && (check_tms_device_data[1] == 0xFFFF)))
                            {
                                LOG_INFO("found tms device #%X%X", check_tms_device_data[0], check_tms_device_data[1]);
                                is_dev_tmsscn = 1;
                                break;
                            }
                            else
                            {
                                LOG_INFO("got unknown #%X%X", check_tms_device_data[0], check_tms_device_data[1]);
                            }
                        }
                    }
                }

                if (!is_dev_tmsscn)
                {
                    LOG_INFO("not tms device, ignored this dev");
                    return -1;
                }

                memset(&tms_props, 0, sizeof(tms_props));
                tms_props.version = TMS_PROPERTIES_VENSION;
                tms_props.sensor_id = sensor_id;
                tms_props.sensor_addr = i2c_write_addr;

                return matched_device_index;
            }
            LOG_INFO("Read sensor id fail, write id: 0x%x, id: 0x%x", i2c_write_addr, sensor_id);
        }
    }

    return matched_device_index;
}

static kal_uint32 get_imgsensor_id(UINT32 *sensor_id)
{
    int tmp_id = check_device();
    if (tmp_id > 0)
    {
        matched_device_index = tmp_id;
        *sensor_id = tms_device_configs[matched_device_index - 1].id;
        if (!is_device_registered)
        {
            is_device_registered = register_device() == 0;
        }
        return ERROR_NONE;
    }
    else
    {
        *sensor_id = 0xFFFFFFFF;
        return ERROR_SENSOR_CONNECT_FAIL;
    }
}

static kal_uint32 open(void)
{
    if (check_device() == -1)
    {
        return ERROR_SENSOR_CONNECT_FAIL;
    }

    return ERROR_NONE;
}

static kal_uint32 close(void)
{
    return ERROR_NONE;
}

static kal_uint32 get_resolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *sensor_resolution)
{
    if (matched_device_index > 0)
    {
        sensor_resolution->SensorFullWidth = tms_device_configs[matched_device_index - 1].imgsensor_info.grabwindow_width;
        sensor_resolution->SensorFullHeight = tms_device_configs[matched_device_index - 1].imgsensor_info.grabwindow_height;
        sensor_resolution->SensorPreviewWidth = tms_device_configs[matched_device_index - 1].imgsensor_info.grabwindow_width;
        sensor_resolution->SensorPreviewHeight = tms_device_configs[matched_device_index - 1].imgsensor_info.grabwindow_height;
        sensor_resolution->SensorVideoWidth = tms_device_configs[matched_device_index - 1].imgsensor_info.grabwindow_width;
        sensor_resolution->SensorVideoHeight = tms_device_configs[matched_device_index - 1].imgsensor_info.grabwindow_height;
        sensor_resolution->SensorHighSpeedVideoWidth = tms_device_configs[matched_device_index - 1].imgsensor_info.grabwindow_width;
        sensor_resolution->SensorHighSpeedVideoHeight = tms_device_configs[matched_device_index - 1].imgsensor_info.grabwindow_height;
        sensor_resolution->SensorSlimVideoWidth = tms_device_configs[matched_device_index - 1].imgsensor_info.grabwindow_width;
        sensor_resolution->SensorSlimVideoHeight = tms_device_configs[matched_device_index - 1].imgsensor_info.grabwindow_height;
    }
    return ERROR_NONE;
}

static kal_uint32 get_info(enum MSDK_SCENARIO_ID_ENUM scenario_id,
                           MSDK_SENSOR_INFO_STRUCT *sensor_info,
                           MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    LOG_INFO("scenario_id = %d", scenario_id);
    if (matched_device_index > 0)
    {

        sensor_info->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;
        sensor_info->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW; /* not use */
        sensor_info->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;        /* inverse with datasheet */
        sensor_info->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
        sensor_info->SensorInterruptDelayLines = 4; /* not use */
        sensor_info->SensorResetActiveHigh = FALSE; /* not use */
        sensor_info->SensorResetDelayCount = 5;     /* not use */

        sensor_info->SensroInterfaceType = SENSOR_INTERFACE_TYPE_MIPI;
        sensor_info->MIPIsensorType = MIPI_OPHY_NCSI2;
        sensor_info->SettleDelayMode = MIPI_SETTLEDELAY_AUTO;
        sensor_info->SensorOutputDataFormat = tms_device_configs[matched_device_index - 1].imgsensor_info.sensor_output_dataformat;

        sensor_info->CaptureDelayFrame = 2;
        sensor_info->PreviewDelayFrame = 2;
        sensor_info->VideoDelayFrame = 2;
        sensor_info->HighSpeedVideoDelayFrame = 2;
        sensor_info->SlimVideoDelayFrame = 2;

        sensor_info->SensorMasterClockSwitch = 0; /* not use */
        sensor_info->SensorDrivingCurrent = ISP_DRIVING_6MA;

        sensor_info->AEShutDelayFrame = 0;
        sensor_info->AESensorGainDelayFrame = 0;
        sensor_info->AEISPGainDelayFrame = 2;
        sensor_info->IHDR_Support = 0;
        sensor_info->IHDR_LE_FirstLine = 0;
        sensor_info->SensorModeNum = 5;

        sensor_info->SensorMIPILaneNumber = tms_device_configs[matched_device_index - 1].imgsensor_info.mipi_lane_num;
        sensor_info->SensorClockFreq = 24;
        sensor_info->SensorClockDividCount = 3; /* not use */
        sensor_info->SensorClockRisingCount = 0;
        sensor_info->SensorClockFallingCount = 2; /* not use */
        sensor_info->SensorPixelClockCount = 3;   /* not use */
        sensor_info->SensorDataLatchCount = 2;    /* not use */

        sensor_info->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
        sensor_info->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
        sensor_info->SensorWidthSampling = 0; /* 0 is default 1x */
        sensor_info->SensorHightSampling = 0; /* 0 is default 1x */
        sensor_info->SensorPacketECCOrder = 1;

        sensor_info->SensorGrabStartX = tms_device_configs[matched_device_index - 1].imgsensor_info.startx;
        sensor_info->SensorGrabStartY = tms_device_configs[matched_device_index - 1].imgsensor_info.starty;
        sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = 15;
    }
    return ERROR_NONE;
}

static kal_uint32 control(enum MSDK_SCENARIO_ID_ENUM scenario_id, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                          MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    LOG_INFO("scenario_id = %d", scenario_id);

    return ERROR_NONE;
}

static kal_uint32 get_default_framerate_by_scenario(enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 *framerate)
{
    LOG_INFO("scenario_id = %d", scenario_id);
    if (matched_device_index > 0)
        *framerate = tms_device_configs[matched_device_index - 1].imgsensor_info.max_framerate;
    else
        *framerate = 0;

    return ERROR_NONE;
}

int write_device(struct tms_msg *msg)
{
    u16 data_len = msg->tag == WR_PROTOCOL ? msg->data.len : 2 + msg->data.len;
    u8 *pu_send_cmd = kmalloc(data_len, GFP_KERNEL);
    int data_start = 0;
    int ret = -1;

    if (msg->tag != WR_PROTOCOL)
    {
        switch (msg->data.reg_len)
        {
        case 1:
            pu_send_cmd[0] = (u8)msg->data.addr;
            data_start = 1;
            data_len--;
            break;
        case 2:
            pu_send_cmd[0] = msg->data.addr >> 8;
            pu_send_cmd[1] = msg->data.addr & 0xFF;
            data_start = 2;
            break;
        default:
            LOG_INFO("msg reg length error");
            return -1;
        }
    }

    memcpy(&pu_send_cmd[data_start], msg->data.buff, msg->data.len);

    ret = iWriteRegI2C(pu_send_cmd, data_len, msg->addr);
    kfree(pu_send_cmd);
    return ret;
}

int read_device(struct tms_msg *msg)
{
    if (msg->data.reg_len == 1)
    {
        u8 pu_send_cmd = (u8)msg->data.addr;
        return iReadRegI2C(&pu_send_cmd, 1, msg->data.buff, msg->data.len, msg->addr);
    }
    else if (msg->data.reg_len == 2)
    {
        u8 pu_send_cmd[2] = {(u8)(msg->data.addr >> 8), (u8)(msg->data.addr & 0xFF)};
        return iReadRegI2C(pu_send_cmd, 2, msg->data.buff, msg->data.len, msg->addr);
    }

    LOG_INFO("msg reg length error");
    return -1;
}

int process_msg(struct tms_msg *msg)
{
    int retval = -1;
    u8 __user *pData = (u8 __user *)msg->data.buff;

    msg->data.buff = memdup_user(pData, msg->data.len);
    switch (msg->tag)
    {
    case RD_RAW:
    case RD_PROTOCOL:
        if (read_device(msg) == 0)
        {
            if (copy_to_user(pData, msg->data.buff, msg->data.len))
            {
                LOG_INFO("read ioctl copy_to_user error");
            }
            else
            {
                retval = msg->data.len;
            }
        }
        break;
    case WR_RAW:
    case WR_PROTOCOL:
        retval = write_device(msg);
        break;
    default:
        break;
    }

    kfree(msg->data.buff);
    return retval;
}

static long tms_ioctl(kal_uint32 cmd, kal_uint8 *arg, kal_uint32 *len)
{
    long ret = 0;
    struct tms_msg msg;

    LOG_INFO("cmd: %x", cmd);
    switch (cmd)
    {
    case __INNER_TMS_GET_PROPERTIES:
    case TMS_GET_PROPERTIES:
        if (*len >= sizeof(tms_props))
        {
            memcpy(arg, &tms_props, sizeof(tms_props));
            *len = sizeof(tms_props);
        }
        else
        {
            ret = -EINVAL;
        }
        break;
    case __INNER_TMS_IIC_RDWR:
    case TMS_IIC_RDWR:
        if (*len >= sizeof(struct tms_msg))
        {
            memcpy(&msg, (struct tms_msg *)arg, sizeof(struct tms_msg));
            ret = process_msg(&msg);
        }
        break;

    default:
        LOG_INFO("cmd unimplemented");
        break;
    }
    return ret;
}

static kal_uint32 feature_control(MSDK_SENSOR_FEATURE_ENUM feature_id, kal_uint8 *feature_arg, kal_uint32 *feature_arg_len)
{
    UINT16 *feature_return_arg_16 = (UINT16 *)feature_arg;
    UINT32 *feature_return_arg_32 = (UINT32 *)feature_arg;
    // UINT32 *feature_data_32 = (UINT32 *)feature_arg;
    unsigned long long *feature_data = (unsigned long long *)feature_arg;

    struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;
    MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data = (MSDK_SENSOR_REG_INFO_STRUCT *)feature_arg;

    LOG_INFO("feature_id = %d", feature_id);
    switch (feature_id)
    {
    case SENSOR_FEATURE_GET_PERIOD:
        *feature_return_arg_16++ = tms_device_configs[matched_device_index - 1].imgsensor_info.line_length;
        *feature_return_arg_16 = tms_device_configs[matched_device_index - 1].imgsensor_info.frame_length;
        *feature_arg_len = 4;
        break;
    case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
        *feature_return_arg_32 = tms_device_configs[matched_device_index - 1].imgsensor_info.pclk;
        *feature_arg_len = 4;
        break;
    case SENSOR_FEATURE_GET_MIPI_PIXEL_RATE:
        *(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = tms_device_configs[matched_device_index - 1].imgsensor_info.mipi_pixel_rate;
        break;
    case SENSOR_FEATURE_SET_REGISTER:
        write_cmos_sensor(sensor_reg_data->RegAddr, sensor_reg_data->RegData);
        break;
    case SENSOR_FEATURE_GET_REGISTER:
        sensor_reg_data->RegData = read_cmos_sensor(sensor_reg_data->RegAddr);
        LOG_INFO("i2c_read 0x%x = 0x%x", sensor_reg_data->RegAddr, sensor_reg_data->RegData);
        break;
    case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
        /* get the lens driver ID from EEPROM or just return LENS_DRIVER_ID_DO_NOT_CARE */
        /* if EEPROM does not exist in camera module. */
        *feature_return_arg_32 = LENS_DRIVER_ID_DO_NOT_CARE;
        *feature_arg_len = 4;
        break;
    case SENSOR_FEATURE_CHECK_SENSOR_ID:
        get_imgsensor_id(feature_return_arg_32);
        break;
    case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
        get_default_framerate_by_scenario((enum MSDK_SCENARIO_ID_ENUM) * (feature_data), (MUINT32 *)(uintptr_t)(*(feature_data + 1)));
        break;

    case SENSOR_FEATURE_GET_CROP_INFO:
        LOG_INFO("scenarioId: %d", (UINT32)*feature_data);
        wininfo = (struct SENSOR_WINSIZE_INFO_STRUCT *)(uintptr_t)(*(feature_data + 1));
        memcpy((void *)wininfo, (void *)&tms_device_configs[matched_device_index - 1].imgsensor_winsize_info, sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
        break;
    default:
        return tms_ioctl(feature_id, feature_arg, feature_arg_len);
        break;
    }
    return ERROR_NONE;
}

static struct SENSOR_FUNCTION_STRUCT sensor_func = {
    .SensorOpen = open,
    .SensorGetInfo = get_info,
    .SensorGetResolution = get_resolution,
    .SensorFeatureControl = feature_control,
    .SensorControl = control,
    .SensorClose = close,
};

UINT32 TMS_MIPI_RAW_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
    /* Check Sensor status here */
    if (pfFunc != NULL)
        *pfFunc = &sensor_func;
    return ERROR_NONE;
}
