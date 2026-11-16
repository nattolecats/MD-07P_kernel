#
# Makefile for the gc7272 GalaxyCore touchscreen driver.
#
ccflags-$(CONFIG_DRM_MEDIATEK_V2) += -I$(DEVICE_MODULES_PATH)/drivers/gpu/drm/mediatek/mediatek_v2/
obj-m += gc7272.o
gc7272-y += gcore_drv_main.o
gc7272-y += gcore_drv_mtk.o
gc7272-y += gcore_drv_fw_update.o
gc7272-y += gcore_drv_interface.o


gc7272-y   +=  mtk_tpd.o
gc7272-y   +=  tpd_button.o
gc7272-y   +=  tpd_setting.o

