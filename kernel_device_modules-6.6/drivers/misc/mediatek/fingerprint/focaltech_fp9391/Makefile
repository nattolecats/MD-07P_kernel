# Makefile for the focaltech fingerprint driver.

#=======================================================================
# Project configurations, should be configured based on your projects

# If your project is MTK platform, you should set it to y, otherwise set
# it to n(remarking it is also ok)
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/spi/mediatek/$(MTK_PLATFORM)
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/mt-plat/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include
CONFIG_FINGERPRINT_FOCALTECH_ARCH_MTK := y

# If TEE is emulation which is called REE, you should set it to y
CONFIG_FINGERPRINT_FOCALTECH_TEE_REE := y


#==========================================================================
# source files and ccflags
obj-$(CONFIG_FOCALTECH_FINGERPRINT9391) := focaltech_fp.o
focaltech_fp-y := ff_core.o

#===For MTK platform
ifeq ($(CONFIG_ARCH_MEDIATEK),y)
CONFIG_FINGERPRINT_FOCALTECH_ARCH_MTK := y
endif

ifeq ($(CONFIG_FINGERPRINT_FOCALTECH_ARCH_MTK),y)
ccflags-$(CONFIG_DRM_MEDIATEK_V2) += -I$(DEVICE_MODULES_PATH)/drivers/gpu/drm/mediatek/mediatek_v2/
ccflags-y += -DCONFIG_FINGERPRINT_FOCALTECH_ARCH_MTK
focaltech_fp-y += ff_chip.o

# trustkernel maybe can't register SPI and call spi_setup and spi sync functions.

ccflags-y += -DCONFIG_FINGERPRINT_FOCALTECH_SPI_SUPPORT
ccflags-y += -DCONFIG_FINGERPRINT_FOCALTECH_SPICLK_SUPPORT
focaltech_fp-y += ff_spi.o


ifeq ($(CONFIG_FINGERPRINT_FOCALTECH_TEE_REE),y)
ccflags-y += -DCONFIG_FINGERPRINT_FOCALTECH_TEE_REE
endif
endif



#=== for trustkernel tee
# ifneq ($(filter y m, $(CONFIG_TRUSTKERNEL_TEE_SUPPORT)),)
# ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/tee/tkcore/include/
# ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/tee/tkcore/core
# endif

