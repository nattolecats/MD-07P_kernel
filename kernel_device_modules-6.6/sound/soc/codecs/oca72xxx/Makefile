# make sure that the environment variables ARCH and CROSS_COMPILE
# are set for your architecture and cross compiler
#
# e.g. for Beaglebone black:
# export ARCH := arm
# export CROSS_COMPILE := arm-linux-gnueabihf-
#
# e.g. Samsung Galaxy S6:
# export ARCH := arm64
# export CROSS_COMPILE := aarch64-linux-android-

#MODULE_NAME = snd-soc-oca72xxx

# additional debugging
#TFA_DEBUG=1

# MTK include path
subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/sound/soc/mediatek/audio_dsp
subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/sound/soc/mediatek/common
subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/audio_ipi/include
subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include
subdir-ccflags-y += -I$(srctree)/drivers/base

# debugging support (also enables trace_printk)
ccflags-y += -Werror
ccflags-y += $(call cc-option,-Wno-date-time,)
snd-soc-oca72xxx-objs:= oca72xxx.o oca72xxx_device.o oca72xxx_monitor.o oca72xxx_bin_parse.o oca72xxx_dsp.o oca72xxx_acf_bin.o
obj-$(CONFIG_SND_SOC_OCA72XXX) += snd-soc-oca72xxx.o
