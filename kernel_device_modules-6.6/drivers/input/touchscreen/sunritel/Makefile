
ST_TOUCH_LISTS := $(subst ",,$(CONFIG_TOUCHSCREEN_ST_TOUCH))
obj-y += $(foreach TOUCH,$(ST_TOUCH_LISTS),$(TOUCH)/)

ST_TOUCH_COUNT := $(words $(ST_TOUCH_LISTS))
subdir-ccflags-y += -DST_TOUCH_COUNT=$(ST_TOUCH_COUNT)

ccflags-$(CONFIG_DRM_MEDIATEK_V2) += -I$(DEVICE_MODULES_PATH)/drivers/gpu/drm/mediatek/mediatek_v2


obj-m += mtk_st_tpd.o
mtk_st_tpd-y += mtk_tpd.o tpd_button.o tpd_setting.o


