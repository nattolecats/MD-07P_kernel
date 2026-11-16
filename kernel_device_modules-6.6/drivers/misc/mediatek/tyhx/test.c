#include <linux/version.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include "../sensor/1.0/hwmon/include/hwmsensor.h"
#include "../sensor/1.0/sensorHub/inc_v1/SCP_sensorHub.h"

//调试命令示例:
// echo 0x11223344 > /sys/class/tyhx_cmd_set/sar_cmd_set
// echo 0x11223344 > /sys/class/tyhx_cmd_set/alsps_cmd_set

//============================================================ cust log ====
#ifndef __TYHX_LOG__
#define __TYHX_LOG__

static int tyhx_log_level = 3;

#define filename(x) strrchr(x,'/')?strrchr(x,'/')+1:x
#define ENTER \
do{ if(tyhx_log_level >= 3) printk(KERN_INFO "[TYHX_DBG][%s][%04d][%s]\n", filename(__FILE__), __LINE__, __func__); }while(0)

#define PRINT_DBG(format,x...) \
do{ if(tyhx_log_level >= 2) printk(KERN_INFO "[TYHX_DBG][%s][%04d][%s] " format, filename(__FILE__), __LINE__, __func__, ## x); }while(0)

#define PRINT_INF(format,x...) \
do{ if(tyhx_log_level >= 1) printk(KERN_INFO "[TYHX_INF][%s][%04d][%s] " format, filename(__FILE__), __LINE__, __func__, ## x); }while(0)

#define PRINT_ERR(format,x...) \
do{ if(tyhx_log_level >= 0) printk(KERN_ERR "[TYHX_ERR][%s][%04d][%s] " format, filename(__FILE__), __LINE__, __func__, ## x); }while(0)

#endif
//============================================================ cust log ====

#define TYHX_CMD_SET_VER "Change-Id:d6361e"

static struct platform_driver tyhx_cmd_set_driver = {
    .driver = {
        .name = "tyhx_cmd_set",
    },
};

static ssize_t tyhx_sar_cmd_set_store(const struct class *class, const struct class_attribute *attr, const char *buf, size_t count)
{
    int trace = 0;
    int ret = 0;

    ENTER;
    ret = sscanf(buf, "0x%x", &trace);
    if (ret != 1) {
        PRINT_ERR("invalid content: '%s', length = %zu\n", buf, count);
        return count;
    }
    ret = sensor_set_cmd_to_hub(ID_SAR, CUST_ACTION_SET_TRACE, &trace);
    if (ret < 0) {
        PRINT_ERR("sensor_set_cmd_to_hub fail,(ID: %d),(action: %d)\n",
                  ID_SAR, CUST_ACTION_SET_TRACE);
    }
    return count;
}

static ssize_t tyhx_alsps_cmd_set_store(const struct class *class, const struct class_attribute *attr, const char *buf, size_t count)
{
    int trace = 0;
    int ret = 0;

    ENTER;
    ret = sscanf(buf, "0x%x", &trace);
    if (ret != 1) {
        PRINT_ERR("invalid content: '%s', length = %zu\n", buf, count);
        return count;
    }
    ret = sensor_set_cmd_to_hub(ID_PROXIMITY, CUST_ACTION_SET_TRACE, &trace);
    if (ret < 0) {
        PRINT_ERR("sensor_set_cmd_to_hub fail,(ID: %d),(action: %d)\n",
                  ID_PROXIMITY, CUST_ACTION_SET_TRACE);
    }
    return count;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,14,0)
static struct class_attribute class_attr_sar_cmd_set = __ATTR(sar_cmd_set, 0664, NULL, tyhx_sar_cmd_set_store);
static struct class_attribute class_attr_alsps_cmd_set = __ATTR(alsps_cmd_set, 0664, NULL, tyhx_alsps_cmd_set_store);

static struct attribute *tyhx_cmd_set_class_attrs[] = {
    &class_attr_sar_cmd_set.attr,
    &class_attr_alsps_cmd_set.attr,
    NULL,
};
ATTRIBUTE_GROUPS(tyhx_cmd_set_class);
#else
static struct class_attribute tyhx_cmd_set_class_attributes[] = {
    __ATTR(sar_cmd_set, 0664, NULL, tyhx_sar_cmd_set_store),
    __ATTR(alsps_cmd_set, 0664, NULL, tyhx_alsps_cmd_set_store),
    __ATTR_NULL,
};
#endif

static struct class tyhx_cmd_set_class = {
        .name = "tyhx_cmd_set",
        //.owner = THIS_MODULE,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,14,0)
        .class_groups = tyhx_cmd_set_class_groups,
#else
        .class_attrs = tyhx_cmd_set_class_attributes,
#endif
    };

static int tyhx_cmd_set_init(void)
{
    int ret  = -1;

    ret = class_register(&tyhx_cmd_set_class);
    if (ret < 0) {
        PRINT_ERR("class_register failed\n");
    }

    return ret;
}

static void tyhx_cmd_set_remove(void)
{
    class_unregister(&tyhx_cmd_set_class);
    return;
}

static int __init tyhx_cmd_set_module_init(void)
{
    int ret = -1;

    ENTER;
    PRINT_INF("driver version:%s\n", TYHX_CMD_SET_VER);
    ret = platform_driver_register(&tyhx_cmd_set_driver);
    if(ret)
        PRINT_ERR("platform_driver_register failed\n");
    return tyhx_cmd_set_init();
}

static void __exit tyhx_cmd_set_module_exit(void)
{
    ENTER;
    tyhx_cmd_set_remove();
    platform_driver_unregister(&tyhx_cmd_set_driver);
}

late_initcall(tyhx_cmd_set_module_init);
module_exit(tyhx_cmd_set_module_exit);

MODULE_AUTHOR("Yasin Lee <yasin.lee.x@gmail.com><yasin.lee@tianyihexin.com>");
MODULE_DESCRIPTION("AP to CP communication test host driver");
MODULE_ALIAS("AP2CP test driver");
MODULE_LICENSE("GPL");
