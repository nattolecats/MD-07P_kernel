
# Linux driver folder
ccflags-$(CONFIG_DRM_MEDIATEK_V2) += -I$(DEVICE_MODULES_PATH)/drivers/gpu/drm/mediatek/mediatek_v2/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/input/touchscreen/sunritel/


obj-m += hynitron.o
hynitron-y += hyn_core.o hyn_lib/hyn_i2c.o hyn_lib/hyn_spi.o hyn_lib/hyn_ts_ext.o hyn_lib/hyn_fs_node.o
hynitron-y += hyn_lib/hyn_tool.o
hynitron-y += hyn_lib/hyn_gesture.o
hynitron-y += hyn_lib/hyn_prox.o
hynitron-y += hyn_chips/hyn_cst66xx.o
hynitron-y += hyn_chips/hyn_cst3240.o
hynitron-y += hyn_chips/hyn_cst92xx.o
hynitron-y += hyn_chips/hyn_cst3xx.o
hynitron-y += hyn_chips/hyn_cst1xx.o  
hynitron-y += hyn_chips/hyn_cst7xx.o
hynitron-y += hyn_chips/hyn_cst8xxT.o

