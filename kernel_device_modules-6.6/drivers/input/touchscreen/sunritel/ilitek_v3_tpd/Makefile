BUILD_INFAE := i2c
BUILD_PLATFORM := mtk
BUILD_MODULE := m

ccflags-y += -Wall
ccflags-$(CONFIG_DRM_MEDIATEK_V2) += -I$(DEVICE_MODULES_PATH)/drivers/gpu/drm/mediatek/mediatek_v2/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/input/touchscreen/sunritel/
ifeq ($(BUILD_PLATFORM),mtk)
platform=ilitek_v3_mtk
endif

ifeq ($(BUILD_INFAE),i2c)
interface=ilitek_v3_i2c
fwupdate=ilitek_v3_flash
endif

ifeq ($(BUILD_INFAE),spi)
interface=ilitek_v3_spi
fwupdate=ilitek_v3_hostdl
endif

ifeq ($(BUILD_MODULE),n)
obj-y += ilitek_v3.o \
	$(interface).o \
	$(platform).o \
	ilitek_v3_ic.o \
	ilitek_v3_touch.o \
	ilitek_v3_mp.o \
	$(fwupdate).o \
	ilitek_v3_node.o
else
	obj-m += ilitek.o
	ilitek-y := ilitek_v3.o \
		$(interface).o \
		$(platform).o \
		ilitek_v3_ic.o \
		ilitek_v3_touch.o \
		ilitek_v3_mp.o \
		$(fwupdate).o \
		ilitek_v3_node.o
endif
