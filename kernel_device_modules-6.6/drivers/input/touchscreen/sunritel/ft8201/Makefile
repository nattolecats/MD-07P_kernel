#
# Makefile for the Goodix GT9886 touchscreen driver.
#

#For display ESD recovery
ccflags-$(CONFIG_DRM_MEDIATEK_V2) += -I$(DEVICE_MODULES_PATH)/drivers/gpu/drm/mediatek/mediatek_v2/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/input/touchscreen/sunritel/

obj-m += ft8201.o

ft8201-y += focaltech_core.o
ft8201-y += focaltech_esdcheck.o
ft8201-y += focaltech_ex_fun.o
ft8201-y += focaltech_ex_mode.o
ft8201-y += focaltech_flash.o
ft8201-y += focaltech_gesture.o
ft8201-y += focaltech_i2c.o
ft8201-y += focaltech_point_report_check.o
ft8201-y += focaltech_proximity.o
ft8201-y += focaltech_upgrade_ft8201p.o
