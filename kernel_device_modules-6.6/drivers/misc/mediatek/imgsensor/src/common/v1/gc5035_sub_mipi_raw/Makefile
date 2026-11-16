# SPDX-License-Identifier: GPL-2.0
IMGSENSOR_SUBDRV_OBJ := \
	$(subdrv-rpath)/gc5035_sub_mipi_raw.o \

ifeq ($(CONFIG_MTK_IMGSENSOR_ISP4_C), m)
$(eval $(MTK_PLATFORM_KO)-objs += $(IMGSENSOR_SUBDRV_OBJ))
else ifeq ($(CONFIG_MTK_IMGSENSOR_ISP4_C), y)
obj-$(CONFIG_MTK_IMGSENSOR_ISP4_C) += $(IMGSENSOR_SUBDRV_OBJ)
endif