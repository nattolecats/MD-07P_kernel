#
# Makefile for the Goodix GT9886 touchscreen driver.
#

#For display ESD recovery
ccflags-$(CONFIG_DRM_MEDIATEK_V2) += -I$(DEVICE_MODULES_PATH)/drivers/gpu/drm/mediatek/mediatek_v2/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/input/touchscreen/sunritel/

obj-m += chipone_tddi.o

chipone_tddi-y += cts_tpd_driver.o cts_driver.o
chipone_tddi-y += cts_i2c_driver.o cts_spi_driver.o
chipone_tddi-y += cts_core.o cts_sfctrlv2.o
chipone_tddi-y += cts_spi_flash.o
chipone_tddi-y += cts_firmware.o cts_test.o
chipone_tddi-y += cts_strerror.o
chipone_tddi-y += cts_charger_detect.o
chipone_tddi-y += cts_earjack_detect.o
chipone_tddi-y += cts_platform.o
chipone_tddi-y += cts_tool.o
chipone_tddi-y += cts_sysfs.o
chipone_tddi-y += cts_ioctl.o
chipone_tddi-y += cts_cdev.o
chipone_tddi-y += cts_oem.o

