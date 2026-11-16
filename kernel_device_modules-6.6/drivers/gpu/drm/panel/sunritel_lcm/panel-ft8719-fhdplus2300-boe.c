// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#include <linux/backlight.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_panel.h>
#include <drm/drm_modes.h>
#include <linux/delay.h>
#include <drm/drm_connector.h>
#include <drm/drm_device.h>

#include <linux/gpio/consumer.h>
#include <linux/regulator/consumer.h>

#include <video/mipi_display.h>
#include <video/of_videomode.h>
#include <video/videomode.h>

#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/of_graph.h>
#include <linux/platform_device.h>
#include "ocp2131.h"
#define CONFIG_MTK_PANEL_EXT
#if defined(CONFIG_MTK_PANEL_EXT)
#include "../../mediatek/mediatek_v2/mtk_panel_ext.h"
#include "../../mediatek/mediatek_v2/mtk_drm_graphics_base.h"
#endif

#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
#include "../../mediatek/mediatek_v2/mtk_corner_pattern/mtk_data_hw_roundedpattern.h"
#endif

static u32 fake_heigh = 2340;
static u32 fake_width = 1080;
extern void ocp2131_set_avdd_voltage(unsigned int voltagelevel);
extern void ocp2131_set_avee_voltage(unsigned int voltagelevel);
static bool need_fake_resolution;
struct lcm {
	struct device *dev;
	struct drm_panel panel;
	struct backlight_device *backlight;
	struct gpio_desc *reset_gpio;
	struct gpio_desc *avee;
	struct gpio_desc *avdd;
	struct gpio_desc *power_gpio;

	bool prepared;
	bool enabled;


	int error;
};

#define lcm_dcs_write_seq(ctx, seq...) \
({\
	const u8 d[] = { seq };\
	BUILD_BUG_ON_MSG(ARRAY_SIZE(d) > 64, "DCS sequence too big for stack");\
	lcm_dcs_write(ctx, d, ARRAY_SIZE(d));\
})

#define lcm_dcs_write_seq_static(ctx, seq...) \
({\
	static const u8 d[] = { seq };\
	lcm_dcs_write(ctx, d, ARRAY_SIZE(d));\
})

static inline struct lcm *panel_to_lcm(struct drm_panel *panel)
{
	return container_of(panel, struct lcm, panel);
}

static void lcm_dcs_write(struct lcm *ctx, const void *data, size_t len)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	ssize_t ret;
	char *addr;

	if (ctx->error < 0)
		return;

	addr = (char *)data;
	if ((int)*addr < 0xB0)
		ret = mipi_dsi_dcs_write_buffer(dsi, data, len);
	else
		ret = mipi_dsi_generic_write(dsi, data, len);
	if (ret < 0) {
		dev_err(ctx->dev, "error %zd writing seq: %ph\n", ret, data);
		ctx->error = ret;
	}
}

#ifdef PANEL_SUPPORT_READBACK
static int lcm_dcs_read(struct lcm *ctx, u8 cmd, void *data, size_t len)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	ssize_t ret;

	if (ctx->error < 0)
		return 0;

	ret = mipi_dsi_dcs_read(dsi, cmd, data, len);
	if (ret < 0) {
		dev_err(ctx->dev, "error %d reading dcs seq:(%#x)\n", ret, cmd);
		ctx->error = ret;
	}

	return ret;
}

static void lcm_panel_get_data(struct lcm *ctx)
{
	u8 buffer[3] = {0};
	static int ret;

	if (ret == 0) {
		ret = lcm_dcs_read(ctx,  0x0A, buffer, 1);
		dev_info(ctx->dev, "return %d data(0x%08x) to dsi engine\n",
			 ret, buffer[0] | (buffer[1] << 8));
	}
}
#endif

static void lcm_panel_init(struct lcm *ctx)
{
	pr_info("%s+\n", __func__);
	lcm_dcs_write_seq_static(ctx, 0x00, 0x00); //CMD2 Enable
    lcm_dcs_write_seq_static(ctx, 0xFF, 0x87, 0x19, 0x01);
    
    lcm_dcs_write_seq_static(ctx, 0x00, 0x80); //FOCAL CMD Enable
    lcm_dcs_write_seq_static(ctx, 0xFF, 0x87, 0x19);
    
    //-------------------------------------------------//
    
    //resolution
    lcm_dcs_write_seq_static(ctx, 0x00, 0xA1); //A1,A2=1080  A3,A4=2340
    lcm_dcs_write_seq_static(ctx, 0xB3, 0x04, 0x38, 0x09, 0x24);
    
    lcm_dcs_write_seq_static(ctx, 0x00, 0xA5); //Source 1080
    lcm_dcs_write_seq_static(ctx, 0xB3, 0xC0);
    
    lcm_dcs_write_seq_static(ctx, 0x00, 0xA6);
    lcm_dcs_write_seq_static(ctx, 0xB3, 0xF8);
    
    lcm_dcs_write_seq_static(ctx, 0x00, 0x85);
    lcm_dcs_write_seq_static(ctx, 0xA7, 0x03);
    
    lcm_dcs_write_seq_static(ctx, 0x00, 0xD0);
    lcm_dcs_write_seq_static(ctx, 0xC3, 0x46);
    
    lcm_dcs_write_seq_static(ctx, 0x00, 0xD3);
    lcm_dcs_write_seq_static(ctx, 0xC3, 0x30);
    
    lcm_dcs_write_seq_static(ctx, 0x00, 0xD4);
    lcm_dcs_write_seq_static(ctx, 0xC3, 0x46);
    
    lcm_dcs_write_seq_static(ctx, 0x00, 0xD7);
    lcm_dcs_write_seq_static(ctx, 0xC3, 0x30);
    
    //****************************************************************************//
    //BOE GIP setting
    lcm_dcs_write_seq_static(ctx, 0x00, 0xCA);
    lcm_dcs_write_seq_static(ctx, 0xC0, 0x80);
    
    lcm_dcs_write_seq_static(ctx, 0x00, 0x80);
    lcm_dcs_write_seq_static(ctx, 0xc2, 0x83, 0x01, 0x01, 0x81, 0x82, 0x01, 0x01, 0x81, 0x82, 0x00, 0x01, 0x81, 0x81, 0x00, 0x01, 0x81);
    
    lcm_dcs_write_seq_static(ctx, 0x00, 0x90);
    lcm_dcs_write_seq_static(ctx, 0xc2, 0x80, 0x00, 0x01, 0x81, 0x01, 0x00, 0x01, 0x81, 0x02, 0x00, 0x01, 0x81, 0x03, 0x00, 0x01, 0x81);
    
    lcm_dcs_write_seq_static(ctx, 0x00, 0xa0);
    lcm_dcs_write_seq_static(ctx, 0xc2, 0x8a, 0x0a, 0x00, 0x01, 0x8a, 0x89, 0x0a, 0x00, 0x01, 0x8a, 0x88, 0x0a, 0x00, 0x01, 0x8a);
    
    lcm_dcs_write_seq_static(ctx, 0x00, 0xb0);
    lcm_dcs_write_seq_static(ctx, 0xc2, 0x87, 0x0a, 0x00, 0x01, 0x8A, 0x00, 0x00, 0x00, 0x01, 0x81, 0x00, 0x00, 0x00, 0x01, 0x81);
    
    lcm_dcs_write_seq_static(ctx, 0x00, 0xc0);
    lcm_dcs_write_seq_static(ctx, 0xc2, 0x00, 0x00, 0x00, 0x01, 0x81, 0x00, 0x00, 0x00, 0x01, 0x81, 0x00, 0x00, 0x00, 0x01, 0x81);
    
    lcm_dcs_write_seq_static(ctx, 0x00, 0xd0);
    lcm_dcs_write_seq_static(ctx, 0xc2, 0x00, 0x00, 0x00, 0x01, 0x81, 0x00, 0x00, 0x00, 0x01, 0x81, 0x00, 0x00, 0x00, 0x01, 0x81);
    
    lcm_dcs_write_seq_static(ctx, 0x00, 0xe0);
    lcm_dcs_write_seq_static(ctx, 0xc2, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x00, 0x00, 0x12, 0x00, 0x0a, 0x0b, 0x01, 0x81);
    
    lcm_dcs_write_seq_static(ctx, 0x00, 0xf0);
    lcm_dcs_write_seq_static(ctx, 0xc2, 0x80, 0x00, 0x50, 0x05, 0x11, 0x80, 0x00, 0x50, 0x05, 0x11, 0xFF, 0xFF, 0xFF, 0x01);
    
    lcm_dcs_write_seq_static(ctx, 0x00, 0x80);
    lcm_dcs_write_seq_static(ctx, 0xcb, 0xfd, 0xfd, 0xfc, 0x01, 0xfd, 0x01, 0x01, 0xfd, 0xFE, 0x02, 0xfd, 0x01, 0xfd, 0xfd, 0x01, 0x01);
    
    lcm_dcs_write_seq_static(ctx, 0x00, 0x90);
    lcm_dcs_write_seq_static(ctx, 0xcb, 0xFF, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
    
    lcm_dcs_write_seq_static(ctx, 0x00, 0xa0);
    lcm_dcs_write_seq_static(ctx, 0xcb, 0x00, 0x00, 0x00, 0x00);
    
    lcm_dcs_write_seq_static(ctx, 0x00, 0xb0);
    lcm_dcs_write_seq_static(ctx, 0xcb, 0x51, 0x55, 0xA5, 0x55);
    
    lcm_dcs_write_seq_static(ctx, 0x00, 0xe0); //EN_Touch
    lcm_dcs_write_seq_static(ctx, 0xc3, 0x35);
    
    lcm_dcs_write_seq_static(ctx, 0x00, 0xe4); //VGH_G
    lcm_dcs_write_seq_static(ctx, 0xc3, 0x35);
    
    lcm_dcs_write_seq_static(ctx, 0x00, 0xe8); //CTSW_VCOM
    lcm_dcs_write_seq_static(ctx, 0xc3, 0x35);
    
    //============================================================================//
    //BOE CGOUT Select
    
    lcm_dcs_write_seq_static(ctx, 0x00, 0x80); //U2D Left CGOUT Select
    lcm_dcs_write_seq_static(ctx, 0xcc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x26, 0x29, 0x24, 0x04, 0x04, 0x18, 0x17, 0x16);
    
    lcm_dcs_write_seq_static(ctx, 0x00, 0x90); //U2D Left CGOUT Select
    lcm_dcs_write_seq_static(ctx, 0xcc, 0x07, 0x09, 0x01, 0x25, 0x25, 0x22, 0x24, 0x03);
    
    lcm_dcs_write_seq_static(ctx, 0x00, 0x80); //U2D Right CGOUT Select
    lcm_dcs_write_seq_static(ctx, 0xcd, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x26, 0x29, 0x24, 0x04, 0x04, 0x18, 0x17, 0x16);
    
    lcm_dcs_write_seq_static(ctx, 0x00, 0x90); //U2D Right CGOUT Select
    lcm_dcs_write_seq_static(ctx, 0xcd, 0x06, 0x08, 0x01, 0x25, 0x25, 0x22, 0x24, 0x02);
    
    lcm_dcs_write_seq_static(ctx, 0x00, 0xa0); //D2U Left CGOUT Select
    lcm_dcs_write_seq_static(ctx, 0xcc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x26, 0x29, 0x24, 0x04, 0x04, 0x18, 0x17, 0x16);
    
    lcm_dcs_write_seq_static(ctx, 0x00, 0xb0); //D2U Left CGOUT Select
    lcm_dcs_write_seq_static(ctx, 0xcc, 0x08, 0x06, 0x01, 0x25, 0x25, 0x24, 0x23, 0x02);
    
    lcm_dcs_write_seq_static(ctx, 0x00, 0xa0); //D2U Right CGOUT Select
    lcm_dcs_write_seq_static(ctx, 0xcd, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x26, 0x29, 0x24, 0x04, 0x04, 0x18, 0x17, 0x16);
    
    lcm_dcs_write_seq_static(ctx, 0x00, 0xb0); //D2U Right CGOUT Select
    lcm_dcs_write_seq_static(ctx, 0xcd, 0x09, 0x07, 0x01, 0x25, 0x25, 0x24, 0x23, 0x03);
    
    //****************************************************************************//
    //TCON Setting
    lcm_dcs_write_seq_static(ctx, 0x00, 0x80);
    lcm_dcs_write_seq_static(ctx, 0xC0, 0x00, 0x79, 0x00, 0x6D, 0x00, 0x10);
    
    lcm_dcs_write_seq_static(ctx, 0x00, 0x87);
    lcm_dcs_write_seq_static(ctx, 0xC0, 0x01, 0x01); //MuxR/G/B
    
    lcm_dcs_write_seq_static(ctx, 0x00, 0x8A); //MUXR/G/B
    lcm_dcs_write_seq_static(ctx, 0xC0, 0x1D, 0x08);
    
    lcm_dcs_write_seq_static(ctx, 0x00, 0x90); //idle 
    lcm_dcs_write_seq_static(ctx, 0xC0, 0x00, 0x79, 0x00, 0x6D, 0x00, 0x10);
    
    lcm_dcs_write_seq_static(ctx, 0x00, 0xA0); //Lf
    lcm_dcs_write_seq_static(ctx, 0xC0, 0x01, 0x05, 0x00, 0x3B, 0x00, 0x10);
    
    lcm_dcs_write_seq_static(ctx, 0x00, 0xD7); //Scaling 
    lcm_dcs_write_seq_static(ctx, 0xC0, 0x00, 0x74, 0x00, 0x71, 0x00, 0x10);
    
    lcm_dcs_write_seq_static(ctx, 0x00, 0xB0);
    lcm_dcs_write_seq_static(ctx, 0xC0, 0x00, 0x79, 0x02, 0x2A, 0x10);
    
    lcm_dcs_write_seq_static(ctx, 0x00, 0xC1);
    lcm_dcs_write_seq_static(ctx, 0xC0, 0x00, 0xAE, 0x00, 0x88, 0x00, 0x74, 0x00, 0xCC);
    
    lcm_dcs_write_seq_static(ctx, 0x00, 0xA5);
    lcm_dcs_write_seq_static(ctx, 0xC1, 0x00, 0x56, 0x00, 0x02);
    
    lcm_dcs_write_seq_static(ctx, 0x00, 0x80);
    lcm_dcs_write_seq_static(ctx, 0xCE, 0x01, 0x80, 0x01, 0x09, 0x00, 0xD8, 0x00, 0xD8, 0x00, 0x90, 0x00, 0x90, 0x0D, 0x0E, 0x09);
    
    lcm_dcs_write_seq_static(ctx, 0x00, 0x90);
    lcm_dcs_write_seq_static(ctx, 0xCE, 0x00, 0x82, 0x0D, 0x5C, 0x00, 0x82, 0x80, 0x09, 0x00, 0x04);
    
    lcm_dcs_write_seq_static(ctx, 0x00, 0xA0);
    lcm_dcs_write_seq_static(ctx, 0xCE, 0x00, 0x00, 0x00);
    
    lcm_dcs_write_seq_static(ctx, 0x00, 0xB0);
    lcm_dcs_write_seq_static(ctx, 0xCE, 0x11, 0x00, 0x00);
    
    lcm_dcs_write_seq_static(ctx, 0x00, 0xC0);
    lcm_dcs_write_seq_static(ctx, 0xCE, 0x00, 0x00, 0x00);
    
    lcm_dcs_write_seq_static(ctx, 0x00, 0xD0);
    lcm_dcs_write_seq_static(ctx, 0xCE, 0x91, 0x00, 0x0A, 0x01, 0x01, 0x00, 0x5E, 0x01);
    
    lcm_dcs_write_seq_static(ctx, 0x00, 0xE0);
    lcm_dcs_write_seq_static(ctx, 0xCE, 0x88, 0x08, 0x02, 0x15, 0x02, 0x15, 0x02, 0x15, 0x00, 0x2C, 0x00, 0x62);
    
    lcm_dcs_write_seq_static(ctx, 0x00, 0xF0);
    lcm_dcs_write_seq_static(ctx, 0xCE, 0x80, 0x17, 0x0B, 0x0F, 0x00, 0xB0, 0x01, 0x3D, 0x01, 0x63);
    
    //============================================================================//
    
    lcm_dcs_write_seq_static(ctx, 0x00, 0x82);
    lcm_dcs_write_seq_static(ctx, 0xCF, 0x06);
    
    lcm_dcs_write_seq_static(ctx, 0x00, 0x84);
    lcm_dcs_write_seq_static(ctx, 0xCF, 0x06);
    
    lcm_dcs_write_seq_static(ctx, 0x00, 0x87);
    lcm_dcs_write_seq_static(ctx, 0xCF, 0x06);
    
    lcm_dcs_write_seq_static(ctx, 0x00, 0x89);
    lcm_dcs_write_seq_static(ctx, 0xCF, 0x06);
    
    lcm_dcs_write_seq_static(ctx, 0x00, 0x8A);
    lcm_dcs_write_seq_static(ctx, 0xCF, 0x07);
    
    lcm_dcs_write_seq_static(ctx, 0x00, 0x8B);
    lcm_dcs_write_seq_static(ctx, 0xCF, 0x00);
    
    lcm_dcs_write_seq_static(ctx, 0x00, 0x8C);
    lcm_dcs_write_seq_static(ctx, 0xCF, 0x06);
    
    lcm_dcs_write_seq_static(ctx, 0x00, 0x92);
    lcm_dcs_write_seq_static(ctx, 0xCF, 0x06);
    
    lcm_dcs_write_seq_static(ctx, 0x00, 0x94);
    lcm_dcs_write_seq_static(ctx, 0xCF, 0x06);
    
    lcm_dcs_write_seq_static(ctx, 0x00, 0x97);
    lcm_dcs_write_seq_static(ctx, 0xCF, 0x06);
    
    lcm_dcs_write_seq_static(ctx, 0x00, 0x99);
    lcm_dcs_write_seq_static(ctx, 0xCF, 0x06);
    
    lcm_dcs_write_seq_static(ctx, 0x00, 0x9A);
    lcm_dcs_write_seq_static(ctx, 0xCF, 0x07);
    
    lcm_dcs_write_seq_static(ctx, 0x00, 0x9B);
    lcm_dcs_write_seq_static(ctx, 0xCF, 0x00);
    
    lcm_dcs_write_seq_static(ctx, 0x00, 0x9C);
    lcm_dcs_write_seq_static(ctx, 0xCF, 0x06);
    
    lcm_dcs_write_seq_static(ctx, 0x00, 0xA0);
    lcm_dcs_write_seq_static(ctx, 0xCF, 0x24);
    
    lcm_dcs_write_seq_static(ctx, 0x00, 0xA2);
    lcm_dcs_write_seq_static(ctx, 0xCF, 0x06);
    
    lcm_dcs_write_seq_static(ctx, 0x00, 0xA4);
    lcm_dcs_write_seq_static(ctx, 0xCF, 0x06);
    
    lcm_dcs_write_seq_static(ctx, 0x00, 0xA7);
    lcm_dcs_write_seq_static(ctx, 0xCF, 0x06);
    
    lcm_dcs_write_seq_static(ctx, 0x00, 0xA9);
    lcm_dcs_write_seq_static(ctx, 0xCF, 0x06);
    
    lcm_dcs_write_seq_static(ctx, 0x00, 0xB0);
    lcm_dcs_write_seq_static(ctx, 0xCF, 0x00, 0x00, 0x6C, 0x70, 0x00, 0x05, 0x05, 0x35, 0x39);
    
    lcm_dcs_write_seq_static(ctx, 0x00, 0xC0);
    lcm_dcs_write_seq_static(ctx, 0xCF, 0x08, 0x08, 0xCA, 0xCE, 0x00, 0x00, 0x00, 0x08, 0x0C);
    
    lcm_dcs_write_seq_static(ctx, 0x00, 0x83); //Gamma chop
    lcm_dcs_write_seq_static(ctx, 0xA4, 0x22);
    
    lcm_dcs_write_seq_static(ctx, 0x00, 0x87);
    lcm_dcs_write_seq_static(ctx, 0xA4, 0x0F);
    
    lcm_dcs_write_seq_static(ctx, 0x00, 0x89);
    lcm_dcs_write_seq_static(ctx, 0xA4, 0x0F);
    
    lcm_dcs_write_seq_static(ctx, 0x00, 0x8D);
    lcm_dcs_write_seq_static(ctx, 0xA4, 0x0F);
    
    lcm_dcs_write_seq_static(ctx, 0x00, 0x8F);
    lcm_dcs_write_seq_static(ctx, 0xA4, 0x0F);
    
    lcm_dcs_write_seq_static(ctx, 0x00, 0x85);
    lcm_dcs_write_seq_static(ctx, 0xA7, 0x00);
    
    lcm_dcs_write_seq_static(ctx, 0x00, 0xE8); //Panel Scan
    lcm_dcs_write_seq_static(ctx, 0xC0, 0x40);
    
    lcm_dcs_write_seq_static(ctx, 0x00, 0x85);
    lcm_dcs_write_seq_static(ctx, 0xC4, 0x1E);
    
    //************************************************************************//
    //Gamma
    lcm_dcs_write_seq_static(ctx, 0x00, 0x00);
    lcm_dcs_write_seq_static(ctx, 0xE1, 0x00, 0x03, 0x05, 0x10, 0x36, 0x1d, 0x25, 0x2b, 0x35, 0x0a, 0x3d, 0x44, 0x4a, 0x4f, 0xe7, 0x54, 0x5c, 0x64, 0x6b, 0x60, 0x72, 0x79, 0x80, 0x89, 0x0d, 0x93, 0x99, 0x9f, 0xa7, 0x8c, 0xb0, 0xba, 0xc8, 0xd2, 0x3c, 0xdb, 0xeb, 0xf7, 0xff, 0x6b);
    lcm_dcs_write_seq_static(ctx, 0x00, 0x00);
    lcm_dcs_write_seq_static(ctx, 0xE2, 0x00, 0x03, 0x05, 0x10, 0x34, 0x17, 0x1f, 0x25, 0x2f, 0x4a, 0x37, 0x3e, 0x44, 0x49, 0xe7, 0x4e, 0x56, 0x5e, 0x65, 0x60, 0x6c, 0x73, 0x7a, 0x83, 0x0d, 0x8d, 0x93, 0x99, 0xa1, 0x8c, 0xaa, 0xb4, 0xc2, 0xcc, 0x3c, 0xda, 0xeb, 0xf7, 0xff, 0xeb);
    lcm_dcs_write_seq_static(ctx, 0x00, 0x00);
    lcm_dcs_write_seq_static(ctx, 0xE3, 0x00, 0x03, 0x05, 0x10, 0x36, 0x1d, 0x25, 0x2b, 0x35, 0x0a, 0x3d, 0x44, 0x4a, 0x4f, 0xe7, 0x54, 0x5c, 0x64, 0x6b, 0x60, 0x72, 0x79, 0x80, 0x89, 0x0d, 0x93, 0x99, 0x9f, 0xa7, 0x8c, 0xb0, 0xba, 0xc8, 0xd2, 0x3c, 0xdb, 0xeb, 0xf7, 0xff, 0x6b);
    lcm_dcs_write_seq_static(ctx, 0x00, 0x00);
    lcm_dcs_write_seq_static(ctx, 0xE4, 0x00, 0x03, 0x05, 0x10, 0x34, 0x17, 0x1f, 0x25, 0x2f, 0x4a, 0x37, 0x3e, 0x44, 0x49, 0xe7, 0x4e, 0x56, 0x5e, 0x65, 0x60, 0x6c, 0x73, 0x7a, 0x83, 0x0d, 0x8d, 0x93, 0x99, 0xa1, 0x8c, 0xaa, 0xb4, 0xc2, 0xcc, 0x3c, 0xda, 0xeb, 0xf7, 0xff, 0xeb);
    lcm_dcs_write_seq_static(ctx, 0x00, 0x00);
    lcm_dcs_write_seq_static(ctx, 0xE5, 0x00, 0x03, 0x05, 0x10, 0x36, 0x1d, 0x25, 0x2b, 0x35, 0x0a, 0x3d, 0x44, 0x4a, 0x4f, 0xe7, 0x54, 0x5c, 0x64, 0x6b, 0x60, 0x72, 0x79, 0x80, 0x89, 0x0d, 0x93, 0x99, 0x9f, 0xa7, 0x8c, 0xb0, 0xba, 0xc8, 0xd2, 0x3c, 0xdb, 0xeb, 0xf7, 0xff, 0x6b);
    lcm_dcs_write_seq_static(ctx, 0x00, 0x00);
    lcm_dcs_write_seq_static(ctx, 0xE6, 0x00, 0x03, 0x05, 0x10, 0x34, 0x17, 0x1f, 0x25, 0x2f, 0x4a, 0x37, 0x3e, 0x44, 0x49, 0xe7, 0x4e, 0x56, 0x5e, 0x65, 0x60, 0x6c, 0x73, 0x7a, 0x83, 0x0d, 0x8d, 0x93, 0x99, 0xa1, 0x8c, 0xaa, 0xb4, 0xc2, 0xcc, 0x3c, 0xda, 0xeb, 0xf7, 0xff, 0xeb);
    
    //*************************************************************************//
    //Power setting
    lcm_dcs_write_seq_static(ctx, 0x00, 0x82); //VGH=13.5v
    lcm_dcs_write_seq_static(ctx, 0xC5, 0x50, 0x50);
    
    lcm_dcs_write_seq_static(ctx, 0x00, 0x84); //VGL=-9.5v
    lcm_dcs_write_seq_static(ctx, 0xC5, 0x32, 0x32, 0x00);
    
    lcm_dcs_write_seq_static(ctx, 0x00, 0x87); //Pump Clk 1/2
    lcm_dcs_write_seq_static(ctx, 0xC5, 0x60, 0x0C);
    
    //GVDD=4.6V
    //NGVDD=-4.6V
    lcm_dcs_write_seq_static(ctx, 0x00, 0x00);
    lcm_dcs_write_seq_static(ctx, 0xD8, 0x23, 0x23);
    
    //VCOM=-0.25V
    lcm_dcs_write_seq_static(ctx, 0x00, 0x00);
    lcm_dcs_write_seq_static(ctx, 0xD9, 0x00, 0x91, 0x91);
    
    //VGHO1=8V
    lcm_dcs_write_seq_static(ctx, 0x00, 0xA3);
    lcm_dcs_write_seq_static(ctx, 0xC5, 0x19);
    
    //VGLO1=-8V
    lcm_dcs_write_seq_static(ctx, 0x00, 0xA9);
    lcm_dcs_write_seq_static(ctx, 0xC5, 0x23);
    
    lcm_dcs_write_seq_static(ctx, 0x00, 0x80);
    lcm_dcs_write_seq_static(ctx, 0xa4, 0x70); //Source bias 2.75uA
    
    lcm_dcs_write_seq_static(ctx, 0x00, 0x80);
    lcm_dcs_write_seq_static(ctx, 0xC5, 0x86, 0x86);
    
    lcm_dcs_write_seq_static(ctx, 0x00, 0x80);
    lcm_dcs_write_seq_static(ctx, 0xa7, 0x03); //RGBBGR
    
    lcm_dcs_write_seq_static(ctx, 0x00, 0x8c);
    lcm_dcs_write_seq_static(ctx, 0xC3, 0x03, 0x00, 0x30); //EQ
    
    lcm_dcs_write_seq_static(ctx, 0x00, 0xB0);
    lcm_dcs_write_seq_static(ctx, 0xF5, 0x00);
    
    lcm_dcs_write_seq_static(ctx, 0x00, 0x82);
    lcm_dcs_write_seq_static(ctx, 0xa4, 0x29, 0x23);
    
    lcm_dcs_write_seq_static(ctx, 0x00, 0xC1);
    lcm_dcs_write_seq_static(ctx, 0xB6, 0x09, 0x89, 0x68);
    
    lcm_dcs_write_seq_static(ctx, 0x00, 0x80);
    lcm_dcs_write_seq_static(ctx, 0xB4, 0x0A); //update Sram W/R speed_20180523
    
    lcm_dcs_write_seq_static(ctx, 0x00, 0xB0);
    lcm_dcs_write_seq_static(ctx, 0xF3, 0x18, 0xE7);
    
    //----------------------LCD initial code End----------------------//
    
    //SLPOUT and DISPON
    lcm_dcs_write_seq_static(ctx, 0x11);
    msleep(120);
    
    lcm_dcs_write_seq_static(ctx, 0x29);
    
    lcm_dcs_write_seq_static(ctx, 0x35, 0x00);
    
    msleep(300);
    
    // Video Mode Enable--------------------------------------------------
    lcm_dcs_write_seq_static(ctx, 0xB7);
    lcm_dcs_write_seq_static(ctx, 0x59);
    lcm_dcs_write_seq_static(ctx, 0x02);
    //----------------------------------------------------------------------

}

static int lcm_disable(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	if (!ctx->enabled)
		return 0;

	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_POWERDOWN;
		backlight_update_status(ctx->backlight);
	}

	ctx->enabled = false;

	return 0;
}
//extern void st_mtk_dsi_clk_hs_mode(bool enter);
static int lcm_unprepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
    printk("zlw  %s %d \n", __func__,__LINE__);
	if (!ctx->prepared)
		return 0;

	lcm_dcs_write_seq_static(ctx, 0x28);
	msleep(120);
	lcm_dcs_write_seq_static(ctx, 0x10);
	msleep(50);
	ctx->error = 0;
	ctx->prepared = false;

    gpiod_set_value(ctx->avee, 0);
	gpiod_set_value(ctx->avdd, 0);
	msleep(10);
	gpiod_set_value(ctx->power_gpio, 0);
	gpiod_set_value(ctx->reset_gpio, 0);
	msleep(10);

   printk("zlw  %s %d \n", __func__,__LINE__);
	return 0;
}

static int lcm_prepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	int ret;

	pr_info("%s+\n", __func__);
	if (ctx->prepared)
		 return 0;
	 
	gpiod_set_value(ctx->power_gpio, 1);
	msleep(10);
	ocp2131_set_avdd_voltage(OCP2131_AVDD_6000V);
	ocp2131_set_avee_voltage(OCP2131_AVEE_6000V);
	gpiod_set_value(ctx->avee, 1);
	gpiod_set_value(ctx->avdd, 1);
    msleep(10);
	gpiod_set_value(ctx->reset_gpio, 1);
	msleep(20);
	gpiod_set_value(ctx->reset_gpio, 0);
	msleep(20);
	gpiod_set_value(ctx->reset_gpio, 1);
	msleep(120);
	lcm_panel_init(ctx);

	ret = ctx->error;
	if (ret < 0)
		lcm_unprepare(panel);

	ctx->prepared = true;

#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_tch_rst(panel);
#endif
#ifdef PANEL_SUPPORT_READBACK
	lcm_panel_get_data(ctx);
#endif

	pr_info("%s-\n", __func__);
	return ret;
}

static int lcm_enable(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	if (ctx->enabled)
		return 0;

	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_UNBLANK;
		backlight_update_status(ctx->backlight);
	}

	ctx->enabled = true;

	return 0;
}

#define HFP (54)
#define HSA (9)
#define HBP (30)
#define VFP (182)
#define VSA (6)
#define VBP (60)
#define VAC (2340)
#define HAC (1080)

static struct drm_display_mode default_mode = {
	.clock = (HAC + HFP + HSA + HBP) * (VAC + VFP + VSA + VBP)*60/1000, //151110,//htotal*vtotal*vrefresh/1000
	.hdisplay = HAC,
	.hsync_start = HAC + HFP,
	.hsync_end = HAC + HFP + HSA,
	.htotal = HAC + HFP + HSA + HBP,
	.vdisplay = VAC,
	.vsync_start = VAC + VFP,
	.vsync_end = VAC + VFP + VSA,
	.vtotal = VAC + VFP + VSA + VBP,
	//.vrefresh = 60,
};

static void change_drm_disp_mode_params(struct drm_display_mode *mode)
{
	if (fake_heigh > 0 && fake_heigh < VAC) {
		mode->vdisplay = fake_heigh;
		mode->vsync_start = fake_heigh + VFP;
		mode->vsync_end = fake_heigh + VFP + VSA;
		mode->vtotal = fake_heigh + VFP + VSA + VBP;
	}
	if (fake_width > 0 && fake_width < HAC) {
		mode->hdisplay = fake_width;
		mode->hsync_start = fake_width + HFP;
		mode->hsync_end = fake_width + HFP + HSA;
		mode->htotal = fake_width + HFP + HSA + HBP;
	}
	//calculate clock, ref drm_mode_vrefresh function
	if (fake_heigh == 1440 && fake_width == 720)
		mode->clock = 69820;
}

#if defined(CONFIG_MTK_PANEL_EXT)
static int panel_ext_reset(struct drm_panel *panel, int on)
{
	struct lcm *ctx = panel_to_lcm(panel);

	gpiod_set_value(ctx->reset_gpio, on);

	return 0;
}

static int panel_ata_check(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	unsigned char data[3] = {0x00, 0x00, 0x00};
	unsigned char id[3] = {0x00, 0x00, 0x00};
	ssize_t ret;

	ret = mipi_dsi_dcs_read(dsi, 0x4, data, 3);
	return 1;
	if (ret < 0) {
		pr_info("%s error\n", __func__);
		return 0;
	}

	pr_info("ATA read data %x %x %x\n", data[0], data[1], data[2]);

	if (data[0] == id[0] &&
			data[1] == id[1] &&
			data[2] == id[2])
		return 1;

	pr_info("ATA expect read data is %x %x %x\n",
			id[0], id[1], id[2]);

	return 0;
}

static int lcm_setbacklight_cmdq(void *dsi, dcs_write_gce cb,
	void *handle, unsigned int level)
{
	char bl_tb0[] = {0x51, 0xFF};

	bl_tb0[1] = level;

	if (!cb)
		return -1;

	cb(dsi, handle, bl_tb0, ARRAY_SIZE(bl_tb0));

	return 0;
}

static int lcm_get_virtual_heigh(void)
{
	return VAC;
}

static int lcm_get_virtual_width(void)
{
	return HAC;
}

static struct mtk_panel_params ext_params = {
	.pll_clk = 595,
	//.vfp_low_power = 750,
	.cust_esd_check = 0,
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x9c,
	},
};

static struct mtk_panel_funcs ext_funcs = {
	.reset = panel_ext_reset,
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
	.ata_check = panel_ata_check,
	.get_virtual_heigh = lcm_get_virtual_heigh,
	.get_virtual_width = lcm_get_virtual_width,
};
#endif

struct panel_desc {
	const struct drm_display_mode *modes;
	unsigned int num_modes;

	unsigned int bpc;

	struct {
		unsigned int width;
		unsigned int height;
	} size;

	struct {
		unsigned int prepare;
		unsigned int enable;
		unsigned int disable;
		unsigned int unprepare;
	} delay;
};

static int lcm_get_modes(struct drm_panel *panel, struct drm_connector *connector)
{
	struct drm_display_mode *mode;

	if (need_fake_resolution)
		change_drm_disp_mode_params(&default_mode);
	mode = drm_mode_duplicate(connector->dev, &default_mode);
	if (!mode) {
		dev_err(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			default_mode.hdisplay, default_mode.vdisplay,
			drm_mode_vrefresh(&default_mode));
		return -ENOMEM;
	}

	drm_mode_set_name(mode);
	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(connector, mode);
	connector->display_info.width_mm = 67;
	connector->display_info.height_mm = 145;

	return 1;
}

static const struct drm_panel_funcs lcm_drm_funcs = {
	.disable = lcm_disable,
	.unprepare = lcm_unprepare,
	.prepare = lcm_prepare,
	.enable = lcm_enable,
	.get_modes = lcm_get_modes,
};

static void check_is_need_fake_resolution(struct device *dev)
{
	unsigned int ret = 0;
	ret = of_property_read_u32(dev->of_node, "fake_heigh", &fake_heigh);
	if (ret) {
		need_fake_resolution = false;
		return;
	}
	ret = of_property_read_u32(dev->of_node, "fake_width", &fake_width);
	if (ret) {
		need_fake_resolution = false;
		return;
	}
	if (fake_heigh > 0 && fake_heigh < VAC && fake_width > 0 && fake_width < HAC)
		need_fake_resolution = true;
	else
		need_fake_resolution = false;
}
static int lcm_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct device_node *dsi_node, *remote_node = NULL, *endpoint = NULL;
	struct lcm *ctx;
	struct device_node *backlight;
	int ret;

	pr_info("%s+zlw\n", __func__);

	dsi_node = of_get_parent(dev->of_node);
	if (dsi_node) {
		endpoint = of_graph_get_next_endpoint(dsi_node, NULL);
		if (endpoint) {
			remote_node = of_graph_get_remote_port_parent(endpoint);
			if (!remote_node) {
				pr_info("No panel connected,skip probe lcm\n");
				return -ENODEV;
			}
			pr_info("zlw-lcm device node name:%s\n", remote_node->name);
		}
	}
	if (remote_node != dev->of_node) {
		pr_info("%s+ skip probe due to not current lcm\n", __func__);
		return -ENODEV;
	}

	ctx = devm_kzalloc(dev, sizeof(struct lcm), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	mipi_dsi_set_drvdata(dsi, ctx);

	ctx->dev = dev;
	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_SYNC_PULSE;

	backlight = of_parse_phandle(dev->of_node, "backlight", 0);
	if (backlight) {
		ctx->backlight = of_find_backlight_by_node(backlight);
		of_node_put(backlight);

		if (!ctx->backlight)
			return -EPROBE_DEFER;
	}

	ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_info(dev, "cannot get reset-gpios %ld\n",
			 PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}

	ctx->avee = devm_gpiod_get(dev, "avee", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->avee)) {
		dev_info(dev, "%s: cannot get bias-pos 0 %ld\n",
			__func__, PTR_ERR(ctx->avee));
		return PTR_ERR(ctx->avee);
	}
	devm_gpiod_put(dev, ctx->avee);

	ctx->avdd = devm_gpiod_get(dev, "avdd", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->avdd)) {
		dev_info(dev, "%s: cannot get bias-neg 1 %ld\n",
			__func__, PTR_ERR(ctx->avdd));
		return PTR_ERR(ctx->avdd);
	}
	devm_gpiod_put(dev, ctx->avdd);

	ctx->power_gpio = devm_gpiod_get(dev, "pwren", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->power_gpio)) {
		dev_info(dev, "cannot get power-gpios %ld\n",
			 PTR_ERR(ctx->power_gpio));
		return PTR_ERR(ctx->power_gpio);
	}

	ctx->prepared = true;
	ctx->enabled = true;

	drm_panel_init(&ctx->panel, dev, &lcm_drm_funcs, DRM_MODE_CONNECTOR_DSI);

	drm_panel_add(&ctx->panel);

	ret = mipi_dsi_attach(dsi);
	if (ret < 0)
		drm_panel_remove(&ctx->panel);

#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_tch_handle_reg(&ctx->panel);
	ret = mtk_panel_ext_create(dev, &ext_params, &ext_funcs, &ctx->panel);
	if (ret < 0)
		return ret;
#endif
	check_is_need_fake_resolution(dev);
	pr_info("%s-jd9365\n", __func__);

	return ret;
}


static void lcm_remove(struct mipi_dsi_device *dsi)
{
	struct lcm *ctx = mipi_dsi_get_drvdata(dsi);
#if defined(CONFIG_MTK_PANEL_EXT)
	struct mtk_panel_ctx *ext_ctx = find_panel_ctx(&ctx->panel);
#endif

	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);

#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_detach(ext_ctx);
	mtk_panel_remove(ext_ctx);
#endif
}
static const struct of_device_id lcm_of_match[] = {
	{ .compatible = "ft8719,fhdplus2300,boe", },
	{ }
};

MODULE_DEVICE_TABLE(of, lcm_of_match);

static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.driver = {
		.name = "panel-ft8719-fhdplus2300-boe",
		.owner = THIS_MODULE,
		.of_match_table = lcm_of_match,
	},
};

module_mipi_dsi_driver(lcm_driver);

MODULE_AUTHOR("bincai liu <bincai.liu@mediatek.com>");
MODULE_DESCRIPTION("ft8752 fhdplus2300 Panel Driver");
MODULE_LICENSE("GPL v2");