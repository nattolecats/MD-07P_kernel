# Linux driver folder
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/mt-plat/
# ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include/
# ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/base
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/sensor/1.0/alsps/inc
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/sensor/1.0/alsps/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/sensor/1.0/hwmon/include
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/input/touchscreen/sunritel/
ccflags-$(CONFIG_DRM_MEDIATEK_V2) += -I$(DEVICE_MODULES_PATH)/drivers/gpu/drm/mediatek/mediatek_v2
obj-m += sitronix.o
sitronix-y += sitronix_ts.o
sitronix-y += sitronix_ts_i2c.o 
sitronix-y += sitronix_ts_spi.o 
sitronix-y += sitronix_ts_utility.o 
sitronix-y += sitronix_ts_mt.o 
sitronix-y += sitronix_ts_nodes.o 
sitronix-y += sitronix_ts_upgrade.o 
# sitronix-y += sitronix_ts_test.o 
sitronix-y += sitronix_ts_proximity.o

