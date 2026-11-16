# Linux driver folder
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/mt-plat/
# ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include/
# ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/base
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/sensor/1.0/alsps/inc
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/sensor/1.0/alsps/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/sensor/1.0/hwmon/include
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/input/touchscreen/sunritel/
ccflags-y += -DCONFIG_TOUCHSCREEN_JADARD_DEBUG=y
ccflags-y += -DCONFIG_TOUCHSCREEN_JADARD_SORTING=y

# Makefile for the Jadard touchscreen drivers.
obj-m += jd9365tg.o
jd9365tg-y += jadard_module.o
jd9365tg-y += jadard_ic_JD9365TG.o
jd9365tg-y += jadard_common.o jadard_platform.o
jd9365tg-y += jadard_debug.o
jd9365tg-y += jadard_sorting.o 
