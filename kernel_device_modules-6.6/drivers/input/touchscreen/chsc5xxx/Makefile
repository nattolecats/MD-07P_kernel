
# Linux driver folder
ccflags-$(CONFIG_DRM_MEDIATEK_V2) += -I$(DEVICE_MODULES_PATH)/drivers/gpu/drm/mediatek/mediatek_v2/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/input/touchscreen/chsc5xxx/

obj-m += chsc5xxx.o
chsc5xxx-y += basic_util.o
chsc5xxx-y += i2c_communication.o
chsc5xxx-y += mtk_core.o
chsc5xxx-y += semi_touch_function.o
chsc5xxx-y += semi_touch_upgrade.o
chsc5xxx-y += semi_touch_apk.o
chsc5xxx-y += semi_touch_device.o
chsc5xxx-y += semi_touch_custom.o

chsc5xxx-y += tpd_button.o
chsc5xxx-y += mtk_tpd.o
chsc5xxx-y += tpd_setting.o


