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

#define CONFIG_MTK_PANEL_EXT
#if defined(CONFIG_MTK_PANEL_EXT)
#include "../../mediatek/mediatek_v2/mtk_panel_ext.h"
#include "../../mediatek/mediatek_v2/mtk_drm_graphics_base.h"
#endif

#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
#include "../../mediatek/mediatek_v2/mtk_corner_pattern/mtk_data_hw_roundedpattern.h"
#endif

static u32 fake_heigh = 1920;
static u32 fake_width = 1200;
static bool need_fake_resolution;
struct lcm {
	struct device *dev;
	struct drm_panel panel;
	struct backlight_device *backlight;
	struct gpio_desc *reset_gpio;
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
	lcm_dcs_write_seq_static(ctx,0xB0,0x5A);
    lcm_dcs_write_seq_static(ctx,0xB1,0x00);
    lcm_dcs_write_seq_static(ctx,0x89,0x01);
    lcm_dcs_write_seq_static(ctx,0x91,0x17);
    lcm_dcs_write_seq_static(ctx,0xB1,0x03);
    lcm_dcs_write_seq_static(ctx,0x2C,0x28);
    lcm_dcs_write_seq_static(ctx,0x00,0xF1);
    lcm_dcs_write_seq_static(ctx,0x01,0x78);
    lcm_dcs_write_seq_static(ctx,0x02,0x3C);
    lcm_dcs_write_seq_static(ctx,0x03,0x1E);
    lcm_dcs_write_seq_static(ctx,0x04,0x8F);
    lcm_dcs_write_seq_static(ctx,0x05,0x01);
    lcm_dcs_write_seq_static(ctx,0x06,0x00);
    lcm_dcs_write_seq_static(ctx,0x07,0x00);
    lcm_dcs_write_seq_static(ctx,0x08,0x00);
    lcm_dcs_write_seq_static(ctx,0x09,0x00);
    lcm_dcs_write_seq_static(ctx,0x0A,0x01);
    lcm_dcs_write_seq_static(ctx,0x0B,0x3C);
    lcm_dcs_write_seq_static(ctx,0x0C,0x00);
    lcm_dcs_write_seq_static(ctx,0x0D,0x00);
    lcm_dcs_write_seq_static(ctx,0x0E,0x24);
    lcm_dcs_write_seq_static(ctx,0x0F,0x1C);
    lcm_dcs_write_seq_static(ctx,0x10,0xC8);
    lcm_dcs_write_seq_static(ctx,0x11,0x60);
    lcm_dcs_write_seq_static(ctx,0x12,0x70);
    lcm_dcs_write_seq_static(ctx,0x13,0x01);
    lcm_dcs_write_seq_static(ctx,0x14,0xE3);
    lcm_dcs_write_seq_static(ctx,0x15,0xFF);
    lcm_dcs_write_seq_static(ctx,0x16,0x3D);
    lcm_dcs_write_seq_static(ctx,0x17,0x0E);
    lcm_dcs_write_seq_static(ctx,0x18,0x01);
    lcm_dcs_write_seq_static(ctx,0x19,0x00);
    lcm_dcs_write_seq_static(ctx,0x1A,0x00);
    lcm_dcs_write_seq_static(ctx,0x1B,0xFC);
    lcm_dcs_write_seq_static(ctx,0x1C,0x0B);
    lcm_dcs_write_seq_static(ctx,0x1D,0xA0);
    lcm_dcs_write_seq_static(ctx,0x1E,0x03);
    lcm_dcs_write_seq_static(ctx,0x1F,0x04);
    lcm_dcs_write_seq_static(ctx,0x20,0x0C);
    lcm_dcs_write_seq_static(ctx,0x21,0x00);
    lcm_dcs_write_seq_static(ctx,0x22,0x04);
    lcm_dcs_write_seq_static(ctx,0x23,0x81);
    lcm_dcs_write_seq_static(ctx,0x24,0x1F);
    lcm_dcs_write_seq_static(ctx,0x25,0x10);
    lcm_dcs_write_seq_static(ctx,0x26,0x9B);
    lcm_dcs_write_seq_static(ctx,0x2D,0x01);
    lcm_dcs_write_seq_static(ctx,0x2E,0x84);
    lcm_dcs_write_seq_static(ctx,0x2F,0x00);
    lcm_dcs_write_seq_static(ctx,0x30,0x02);
    lcm_dcs_write_seq_static(ctx,0x31,0x08);
    lcm_dcs_write_seq_static(ctx,0x32,0x01);
    lcm_dcs_write_seq_static(ctx,0x33,0x1C);
    lcm_dcs_write_seq_static(ctx,0x34,0x70);
    lcm_dcs_write_seq_static(ctx,0x35,0xFF);
    lcm_dcs_write_seq_static(ctx,0x36,0xFF);
    lcm_dcs_write_seq_static(ctx,0x37,0xFF);
    lcm_dcs_write_seq_static(ctx,0x38,0xFF);
    lcm_dcs_write_seq_static(ctx,0x39,0xFF);
    lcm_dcs_write_seq_static(ctx,0x3A,0x05);
    lcm_dcs_write_seq_static(ctx,0x3B,0x00);
    lcm_dcs_write_seq_static(ctx,0x3C,0x00);
    lcm_dcs_write_seq_static(ctx,0x3D,0x00);
    lcm_dcs_write_seq_static(ctx,0x3E,0x0F);
    lcm_dcs_write_seq_static(ctx,0x3F,0xA4);
    lcm_dcs_write_seq_static(ctx,0x40,0x28);
    lcm_dcs_write_seq_static(ctx,0x41,0xFC);
    lcm_dcs_write_seq_static(ctx,0x42,0x01);
    lcm_dcs_write_seq_static(ctx,0x43,0x40);
    lcm_dcs_write_seq_static(ctx,0x44,0x05);
    lcm_dcs_write_seq_static(ctx,0x45,0xE8);
    lcm_dcs_write_seq_static(ctx,0x46,0x16);
    lcm_dcs_write_seq_static(ctx,0x47,0x00);
    lcm_dcs_write_seq_static(ctx,0x48,0x00);
    lcm_dcs_write_seq_static(ctx,0x49,0x88);
    lcm_dcs_write_seq_static(ctx,0x4A,0x08);
    lcm_dcs_write_seq_static(ctx,0x4B,0x05);
    lcm_dcs_write_seq_static(ctx,0x4C,0x03);
    lcm_dcs_write_seq_static(ctx,0x4D,0xD0);
    lcm_dcs_write_seq_static(ctx,0x4E,0x13);
    lcm_dcs_write_seq_static(ctx,0x4F,0xFF);
    lcm_dcs_write_seq_static(ctx,0x50,0x0A);
    lcm_dcs_write_seq_static(ctx,0x51,0x53);
    lcm_dcs_write_seq_static(ctx,0x52,0x26);
    lcm_dcs_write_seq_static(ctx,0x53,0x22);
    lcm_dcs_write_seq_static(ctx,0x54,0x09);
    lcm_dcs_write_seq_static(ctx,0x55,0x22);
    lcm_dcs_write_seq_static(ctx,0x56,0x00);
    lcm_dcs_write_seq_static(ctx,0x57,0x1C);
    lcm_dcs_write_seq_static(ctx,0x58,0x03);
    lcm_dcs_write_seq_static(ctx,0x59,0x3F);
    lcm_dcs_write_seq_static(ctx,0x5A,0x28);
    lcm_dcs_write_seq_static(ctx,0x5B,0x01);
    lcm_dcs_write_seq_static(ctx,0x5C,0xCC);
    lcm_dcs_write_seq_static(ctx,0x5D,0x21);
    lcm_dcs_write_seq_static(ctx,0x5E,0x04);
    lcm_dcs_write_seq_static(ctx,0x5F,0x13);
    lcm_dcs_write_seq_static(ctx,0x60,0x42);
    lcm_dcs_write_seq_static(ctx,0x61,0x08);
    lcm_dcs_write_seq_static(ctx,0x62,0x64);
    lcm_dcs_write_seq_static(ctx,0x63,0xEB);
    lcm_dcs_write_seq_static(ctx,0x64,0x10);
    lcm_dcs_write_seq_static(ctx,0x65,0xA8);
    lcm_dcs_write_seq_static(ctx,0x66,0x84);
    lcm_dcs_write_seq_static(ctx,0x67,0x8E);
    lcm_dcs_write_seq_static(ctx,0x68,0x29);
    lcm_dcs_write_seq_static(ctx,0x69,0x11);
    lcm_dcs_write_seq_static(ctx,0x6A,0x42);
    lcm_dcs_write_seq_static(ctx,0x6B,0x38);
    lcm_dcs_write_seq_static(ctx,0x6C,0x21);
    lcm_dcs_write_seq_static(ctx,0x6D,0x84);
    lcm_dcs_write_seq_static(ctx,0x6E,0x50);
    lcm_dcs_write_seq_static(ctx,0x6F,0xB6);
    lcm_dcs_write_seq_static(ctx,0x70,0x0E);
    lcm_dcs_write_seq_static(ctx,0x71,0xA1);
    lcm_dcs_write_seq_static(ctx,0x72,0xCE);
    lcm_dcs_write_seq_static(ctx,0x73,0xF8);
    lcm_dcs_write_seq_static(ctx,0x74,0xDA);
    lcm_dcs_write_seq_static(ctx,0x75,0x1A);
    lcm_dcs_write_seq_static(ctx,0x76,0x00);
    lcm_dcs_write_seq_static(ctx,0x77,0x00);
    lcm_dcs_write_seq_static(ctx,0x78,0x0F);
    lcm_dcs_write_seq_static(ctx,0x79,0xE0);
    lcm_dcs_write_seq_static(ctx,0x7A,0x01);
    lcm_dcs_write_seq_static(ctx,0x7B,0xFF);
    lcm_dcs_write_seq_static(ctx,0x7C,0xFF);
    lcm_dcs_write_seq_static(ctx,0x7D,0x06);
    lcm_dcs_write_seq_static(ctx,0x7E,0x41);
    lcm_dcs_write_seq_static(ctx,0x7F,0xFE);
    lcm_dcs_write_seq_static(ctx,0xb1,0x02);
    lcm_dcs_write_seq_static(ctx,0x00,0xFF);
    lcm_dcs_write_seq_static(ctx,0x01,0x01);
    lcm_dcs_write_seq_static(ctx,0x02,0x00);
    lcm_dcs_write_seq_static(ctx,0x03,0x00);
    lcm_dcs_write_seq_static(ctx,0x04,0x00);
    lcm_dcs_write_seq_static(ctx,0x05,0x00);
    lcm_dcs_write_seq_static(ctx,0x06,0x00);
    lcm_dcs_write_seq_static(ctx,0x07,0x00);
    lcm_dcs_write_seq_static(ctx,0x08,0xC0);
    lcm_dcs_write_seq_static(ctx,0x09,0x00);
    lcm_dcs_write_seq_static(ctx,0x0A,0x00);
    lcm_dcs_write_seq_static(ctx,0x0B,0x14);
    lcm_dcs_write_seq_static(ctx,0x0C,0xE6);
    lcm_dcs_write_seq_static(ctx,0x0D,0x0D);
    lcm_dcs_write_seq_static(ctx,0x0F,0x08);
    lcm_dcs_write_seq_static(ctx,0x10,0xAB);
    lcm_dcs_write_seq_static(ctx,0x11,0x3A);
    lcm_dcs_write_seq_static(ctx,0x12,0x6D);
    lcm_dcs_write_seq_static(ctx,0x13,0xC5);
    lcm_dcs_write_seq_static(ctx,0x14,0x66);
    lcm_dcs_write_seq_static(ctx,0x15,0x8D);
    lcm_dcs_write_seq_static(ctx,0x16,0xF2);
    lcm_dcs_write_seq_static(ctx,0x17,0x08);
    lcm_dcs_write_seq_static(ctx,0x18,0x01);
    lcm_dcs_write_seq_static(ctx,0x19,0x79);
    lcm_dcs_write_seq_static(ctx,0x1A,0x1E);
    lcm_dcs_write_seq_static(ctx,0x1B,0x0C);
    lcm_dcs_write_seq_static(ctx,0x1C,0xFF);
    lcm_dcs_write_seq_static(ctx,0x1D,0xFF);
    lcm_dcs_write_seq_static(ctx,0x1E,0xFF);
    lcm_dcs_write_seq_static(ctx,0x1F,0xFF);
    lcm_dcs_write_seq_static(ctx,0x20,0xFF);
    lcm_dcs_write_seq_static(ctx,0x21,0xFF);
    lcm_dcs_write_seq_static(ctx,0x22,0xFF);
    lcm_dcs_write_seq_static(ctx,0x23,0xFF);
    lcm_dcs_write_seq_static(ctx,0x24,0xFF);
    lcm_dcs_write_seq_static(ctx,0x25,0xFF);
    lcm_dcs_write_seq_static(ctx,0x26,0xFF);
    lcm_dcs_write_seq_static(ctx,0x27,0x1F);
    lcm_dcs_write_seq_static(ctx,0x28,0xC8);
    lcm_dcs_write_seq_static(ctx,0x29,0xFF);
    lcm_dcs_write_seq_static(ctx,0x2A,0xFF);
    lcm_dcs_write_seq_static(ctx,0x2B,0xFF);
    lcm_dcs_write_seq_static(ctx,0x2C,0x07);
    lcm_dcs_write_seq_static(ctx,0x2D,0x03);
    lcm_dcs_write_seq_static(ctx,0x33,0x0A);
    lcm_dcs_write_seq_static(ctx,0x35,0x7F);
    lcm_dcs_write_seq_static(ctx,0x36,0x09);
    lcm_dcs_write_seq_static(ctx,0x38,0x7F);
    lcm_dcs_write_seq_static(ctx,0x3A,0x80);
    lcm_dcs_write_seq_static(ctx,0x3B,0x55);
    lcm_dcs_write_seq_static(ctx,0x3C,0xE2);
    lcm_dcs_write_seq_static(ctx,0x3D,0x32);
    lcm_dcs_write_seq_static(ctx,0x3E,0x00);
    lcm_dcs_write_seq_static(ctx,0x3F,0x58);
    lcm_dcs_write_seq_static(ctx,0x40,0x06);
    lcm_dcs_write_seq_static(ctx,0x41,0x80);
    lcm_dcs_write_seq_static(ctx,0x42,0xCB);
    lcm_dcs_write_seq_static(ctx,0x43,0x2C);
    lcm_dcs_write_seq_static(ctx,0x44,0x61);
    lcm_dcs_write_seq_static(ctx,0x45,0x39);
    lcm_dcs_write_seq_static(ctx,0x46,0x00);
    lcm_dcs_write_seq_static(ctx,0x47,0x00);
    lcm_dcs_write_seq_static(ctx,0x48,0x8B);
    lcm_dcs_write_seq_static(ctx,0x49,0xD2);
    lcm_dcs_write_seq_static(ctx,0x4A,0x01);
    lcm_dcs_write_seq_static(ctx,0x4B,0x00);
    lcm_dcs_write_seq_static(ctx,0x4C,0x10);
    lcm_dcs_write_seq_static(ctx,0x4D,0xC0);
    lcm_dcs_write_seq_static(ctx,0x4E,0x0F);
    lcm_dcs_write_seq_static(ctx,0x4F,0xF1);
    lcm_dcs_write_seq_static(ctx,0x50,0x78);
    lcm_dcs_write_seq_static(ctx,0x51,0x7A);
    lcm_dcs_write_seq_static(ctx,0x52,0x34);
    lcm_dcs_write_seq_static(ctx,0x53,0x99);
    lcm_dcs_write_seq_static(ctx,0x54,0xA2);
    lcm_dcs_write_seq_static(ctx,0x55,0x03);
    lcm_dcs_write_seq_static(ctx,0x56,0x6C);
    lcm_dcs_write_seq_static(ctx,0x57,0x1A);
    lcm_dcs_write_seq_static(ctx,0x58,0x05);
    lcm_dcs_write_seq_static(ctx,0x59,0x30);
    lcm_dcs_write_seq_static(ctx,0x5A,0x1E);
    lcm_dcs_write_seq_static(ctx,0x5B,0x8F);
    lcm_dcs_write_seq_static(ctx,0x5C,0xC7);
    lcm_dcs_write_seq_static(ctx,0x5D,0xE3);
    lcm_dcs_write_seq_static(ctx,0x5E,0xF1);
    lcm_dcs_write_seq_static(ctx,0x5F,0x78);
    lcm_dcs_write_seq_static(ctx,0x60,0x3C);
    lcm_dcs_write_seq_static(ctx,0x61,0x36);
    lcm_dcs_write_seq_static(ctx,0x62,0x1E);
    lcm_dcs_write_seq_static(ctx,0x63,0x1B);
    lcm_dcs_write_seq_static(ctx,0x64,0x8F);
    lcm_dcs_write_seq_static(ctx,0x65,0xC7);
    lcm_dcs_write_seq_static(ctx,0x66,0xE3);
    lcm_dcs_write_seq_static(ctx,0x67,0x31);
    lcm_dcs_write_seq_static(ctx,0x68,0x14);
    lcm_dcs_write_seq_static(ctx,0x69,0x89);
    lcm_dcs_write_seq_static(ctx,0x6A,0x70);
    lcm_dcs_write_seq_static(ctx,0x6B,0x8C);
    lcm_dcs_write_seq_static(ctx,0x6C,0x8D);
    lcm_dcs_write_seq_static(ctx,0x6D,0x8D);
    lcm_dcs_write_seq_static(ctx,0x6E,0x8D);
    lcm_dcs_write_seq_static(ctx,0x6F,0x8D);
    lcm_dcs_write_seq_static(ctx,0x70,0xC7);
    lcm_dcs_write_seq_static(ctx,0x71,0xE3);
    lcm_dcs_write_seq_static(ctx,0x72,0xF1);
    lcm_dcs_write_seq_static(ctx,0x73,0xD8);
    lcm_dcs_write_seq_static(ctx,0x74,0xD8);
    lcm_dcs_write_seq_static(ctx,0x75,0xD8);
    lcm_dcs_write_seq_static(ctx,0x76,0x18);
    lcm_dcs_write_seq_static(ctx,0x77,0x00);
    lcm_dcs_write_seq_static(ctx,0x78,0x00);
    lcm_dcs_write_seq_static(ctx,0x79,0x00);
    lcm_dcs_write_seq_static(ctx,0x7A,0xC6);
    lcm_dcs_write_seq_static(ctx,0x7B,0xC6);
    lcm_dcs_write_seq_static(ctx,0x7C,0xC6);
    lcm_dcs_write_seq_static(ctx,0x7D,0xC6);
    lcm_dcs_write_seq_static(ctx,0x7E,0xC6);
    lcm_dcs_write_seq_static(ctx,0x7F,0xE3);
    lcm_dcs_write_seq_static(ctx,0x0B,0x04);
    lcm_dcs_write_seq_static(ctx,0xB1,0x03);
    lcm_dcs_write_seq_static(ctx,0x2C,0x2C);
    lcm_dcs_write_seq_static(ctx,0xB1,0x00);
    lcm_dcs_write_seq_static(ctx,0x89,0x03);
	lcm_dcs_write_seq_static(ctx, 0x11);
	msleep(20);
	lcm_dcs_write_seq_static(ctx, 0x29);
	msleep(120);

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
	ctx->power_gpio = devm_gpiod_get(ctx->dev, "power", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->power_gpio)) {
		dev_info(ctx->dev, "%s: cannot get power_gpio %ld\n",
			__func__, PTR_ERR(ctx->power_gpio));
		return PTR_ERR(ctx->power_gpio);
	}
	gpiod_set_value(ctx->power_gpio, 0);
	devm_gpiod_put(ctx->dev, ctx->power_gpio);
	msleep(10);
	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_info(ctx->dev, "%s: cannot get reset_gpio %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	gpiod_set_value(ctx->reset_gpio, 0);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);
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
	ctx->power_gpio = devm_gpiod_get(ctx->dev, "power", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->power_gpio)) {
		dev_info(ctx->dev, "%s: cannot get power_gpio %ld\n",
			__func__, PTR_ERR(ctx->power_gpio));
		return PTR_ERR(ctx->power_gpio);
	}
	gpiod_set_value(ctx->power_gpio, 1);
	devm_gpiod_put(ctx->dev, ctx->power_gpio);
    msleep(10);
	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_info(ctx->dev, "%s: cannot get reset_gpio %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	gpiod_set_value(ctx->reset_gpio, 1);
	msleep(10);
	gpiod_set_value(ctx->reset_gpio, 0);
	msleep(20);
	gpiod_set_value(ctx->reset_gpio, 1);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);
	msleep(50);
	
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

#define HFP (35)
#define HSA (6)
#define HBP (25)
#define VFP (80)
#define VSA (6)
#define VBP (60)
#define VAC (1920)
#define HAC (1200)

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
	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_info(ctx->dev, "%s: cannot get reset_gpio %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	gpiod_set_value(ctx->reset_gpio, on);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

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
	.pll_clk = 480,
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
	connector->display_info.width_mm = 136;
	connector->display_info.height_mm = 218;

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
	struct lcm *ctx;
	struct device_node *backlight;
	int ret;
	struct device_node *dsi_node, *remote_node = NULL, *endpoint = NULL;

	pr_info("%s+jd9365\n", __func__);

	dsi_node = of_get_parent(dev->of_node);
	if (dsi_node) {
		endpoint = of_graph_get_next_endpoint(dsi_node, NULL);
		if (endpoint) {
			remote_node = of_graph_get_remote_port_parent(endpoint);
			if (!remote_node) {
				pr_info("No panel connected,skip probe lcm\n");
				return -ENODEV;
			}
			pr_info("jd9365-lcm device node name:%s\n", remote_node->name);
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
	pr_info("%s parse get power_gpio\n", __func__);
	ctx->power_gpio = devm_gpiod_get(ctx->dev, "power", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->power_gpio)) {
		dev_info(ctx->dev, "%s: cannot get power_gpio %ld\n",
			__func__, PTR_ERR(ctx->power_gpio));
		return PTR_ERR(ctx->power_gpio);
	}
	devm_gpiod_put(dev, ctx->power_gpio);
	pr_info("%s parse get reset_gpio\n", __func__);
	ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_info(ctx->dev, "%s: cannot get reset-gpios %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	devm_gpiod_put(dev, ctx->reset_gpio);

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
	{ .compatible = "jd9365,wuxga,boe", },
	{ }
};

MODULE_DEVICE_TABLE(of, lcm_of_match);

static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.driver = {
		.name = "panel-jd9365-wuxga-dsi-boe",
		.owner = THIS_MODULE,
		.of_match_table = lcm_of_match,
	},
};

module_mipi_dsi_driver(lcm_driver);

MODULE_AUTHOR("dongshiyi <shiyi.dong@sunritel.com.cn>");
MODULE_DESCRIPTION("jd9365 wuxga Panel Driver");
MODULE_LICENSE("GPL v2");