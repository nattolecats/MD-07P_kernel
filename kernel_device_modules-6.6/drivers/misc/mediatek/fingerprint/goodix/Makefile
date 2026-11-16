#
# Makefile for the fingerprint device.
#

# Each configuration option enables a list of files.
#include $(srctree)/drivers/misc/mediatek/Makefile.custom
   subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/spi/
  # subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/spi/
   ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include
   subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include \
					-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/mt-plat/


 ifeq ($(CONFIG_TRUSTONIC_TEE_SUPPORT),y)
  # subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/spi/trustzone/spi/Tlspi/inc
 endif
 ifeq ($(CONFIG_TRUSTONIC_TEE_SUPPORT),y)
	#include $(DEVICE_MODULES_PATH)/drivers/tee/gud/Makefile.include
 endif
#endif
#compatible for trustkernel
subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/tee/tkcore/include/
subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/tee/tkcore/

goodix-objs:= gf_spi_tee.o gf_spi_factory.o
obj-$(CONFIG_GOODIX_FINGERPRINT)     += goodix.o

