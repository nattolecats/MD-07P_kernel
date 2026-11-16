# Linux driver folder
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/mt-plat/
#For display ESD recovery
ccflags-$(CONFIG_DRM_MEDIATEK_V2) += -I$(DEVICE_MODULES_PATH)/drivers/gpu/drm/mediatek/mediatek_v2/
# ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include/
# ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/base
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/sensor/1.0/alsps/inc
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/sensor/1.0/alsps/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/sensor/1.0/hwmon/include
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/input/touchscreen/sunritel/
ccflags-y += -DCONFIG_TOUCHSCREEN_JADARD_DEBUG=y
ccflags-y += -DCONFIG_TOUCHSCREEN_JADARD_SORTING=y

ifeq ($(TARGET_BUILD_VARIANT), user)
	ccflags-y += -D__JADARD_GKI__
endif

# Makefile for the Jadard touchscreen drivers.
	ccflags-y += -D__JADARD_KMODULE__
	obj-m += jdchipset_jd9366ts.o
	jdchipset_jd9366ts-y += jadard_module.o
	jdchipset_jd9366ts-y += jadard_ic_JD9366TP.o
	jdchipset_jd9366ts-y += jadard_common.o jadard_platform.o
	jdchipset_jd9366ts-y += jadard_debug.o
	jdchipset_jd9366ts-y += jadard_sorting.o

