# Linux driver folder
ccflags-$(CONFIG_DRM_MEDIATEK_V2) += -I$(DEVICE_MODULES_PATH)/drivers/gpu/drm/mediatek/mediatek_v2/
ccflags-$(CONFIG_DRM_MEDIATEK_V2) += -I$(DEVICE_MODULES_PATH)/drivers/input/touchscreen/mediatek/
ccflags-y += -DCONFIG_TOUCHSCREEN_JADARD_DEBUG=y
ccflags-y += -DCONFIG_TOUCHSCREEN_JADARD_SORTING=y
ccflags-y += -D__JADARD_KMODULE__

ifeq ($(TARGET_BUILD_VARIANT), user)
	ccflags-y += -D__JADARD_GKI__
endif
obj-m += jdchipset_spi.o


jdchipset_spi-y	+=  jadard_module.o
jdchipset_spi-y	+=  jadard_ic_JD9365TN.o
jdchipset_spi-y	+=  jadard_common.o
jdchipset_spi-y	+=  jadard_platform.o
jdchipset_spi-y	+=  jadard_debug.o
jdchipset_spi-y   +=  jadard_sorting.o

