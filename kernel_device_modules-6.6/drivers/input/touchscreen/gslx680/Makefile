# Makefile for the focaltech touchscreen drivers.
ccflags-$(CONFIG_DRM_MEDIATEK_V2) += -I$(DEVICE_MODULES_PATH)/drivers/gpu/drm/mediatek/mediatek_v2/
obj-m += gslx680.o

gslx680-y   +=  gsl_point_id.o
gslx680-y   +=  gsl_ts_driver.o
gslx680-y   +=  mtk_tpd.o
gslx680-y   +=  tpd_button.o
gslx680-y   +=  tpd_debug.o
gslx680-y   +=  tpd_default.o
gslx680-y   +=  tpd_misc.o
gslx680-y   +=  tpd_setting.o
