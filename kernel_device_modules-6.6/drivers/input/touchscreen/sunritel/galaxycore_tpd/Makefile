
# Linux driver folder
ccflags-$(CONFIG_DRM_MEDIATEK_V2) += -I$(DEVICE_MODULES_PATH)/drivers/gpu/drm/mediatek/mediatek_v2/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/input/touchscreen/sunritel/

obj-m += galaxycore.o

galaxycore-y	+=  gcore_drv_main.o
galaxycore-y	+=  gcore_drv_mtk.o
galaxycore-y	+=  gcore_drv_fw_update.o
galaxycore-y	+=  gcore_drv_interface.o
