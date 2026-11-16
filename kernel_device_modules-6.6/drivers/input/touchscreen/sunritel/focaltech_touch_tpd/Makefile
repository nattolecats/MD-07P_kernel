# Makefile for the focaltech touchscreen drivers.


ccflags-$(CONFIG_DRM_MEDIATEK_V2) += -I$(DEVICE_MODULES_PATH)/drivers/gpu/drm/mediatek/mediatek_v2/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/input/touchscreen/sunritel/


obj-m += focaltech_tp.o
focaltech_tp-y += focaltech_core.o
focaltech_tp-y += focaltech_ex_fun.o
focaltech_tp-y += focaltech_ex_mode.o
focaltech_tp-y += focaltech_gesture.o
focaltech_tp-y += focaltech_esdcheck.o
focaltech_tp-y += focaltech_point_report_check.o
focaltech_tp-y += focaltech_proximity.o
focaltech_tp-y += focaltech_i2c.o
focaltech_tp-y += focaltech_flash.o
focaltech_tp-y += focaltech_flash/focaltech_upgrade_ft8006s_aa.o