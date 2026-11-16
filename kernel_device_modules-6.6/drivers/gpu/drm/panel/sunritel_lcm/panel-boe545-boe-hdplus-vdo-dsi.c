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

static u32 fake_heigh = 1440;
static u32 fake_width = 720;
static bool need_fake_resolution;

struct lcm {
	struct device *dev;
	struct drm_panel panel;
	struct backlight_device *backlight;
	struct gpio_desc *reset_gpio;
	struct gpio_desc *bias_pos, *bias_neg;
	struct gpio_desc *lcm_1v8_gpio;
	struct gpio_desc *dsi_en_gpio;
	struct gpio_desc *key_led_gpio;
    struct gpio_desc *tp_rst_gpio;
	
	bool prepared;
	bool enabled;

	unsigned int gate_ic;

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

#if 1//!IS_ENABLED(CONFIG_RT4831A_I2C)
#if 1//IS_ENABLED(CONFIG_RT5081_PMU_DSV) || IS_ENABLED(CONFIG_DEVICE_MODULES_REGULATOR_MT6370)
static struct regulator *disp_bias_pos;
static struct regulator *disp_bias_neg;

static int lcm_panel_bias_regulator_init(void)
{
	static int regulator_inited;
	int ret = 0;

	if (regulator_inited)
		return ret;

	/* please only get regulator once in a driver */
	disp_bias_pos = regulator_get(NULL, "dsv_pos");
	if (IS_ERR(disp_bias_pos)) { /* handle return value */
		ret = PTR_ERR(disp_bias_pos);
		pr_err("get dsv_pos fail, error: %d\n", ret);
		return ret;
	}

	disp_bias_neg = regulator_get(NULL, "dsv_neg");
	if (IS_ERR(disp_bias_neg)) { /* handle return value */
		ret = PTR_ERR(disp_bias_neg);
		pr_err("get dsv_neg fail, error: %d\n", ret);
		return ret;
	}

	regulator_inited = 1;
	return ret; /* must be 0 */

}

static int lcm_panel_bias_enable(void)
{
	int ret = 0;
	int retval = 0;

	lcm_panel_bias_regulator_init();

	/* set voltage with min & max*/
	ret = regulator_set_voltage(disp_bias_pos, 6000000, 6000000);
	if (ret < 0)
		pr_err("set voltage disp_bias_pos fail, ret = %d\n", ret);
	retval |= ret;

	ret = regulator_set_voltage(disp_bias_neg, 6000000, 6000000);
	if (ret < 0)
		pr_err("set voltage disp_bias_neg fail, ret = %d\n", ret);
	retval |= ret;

	/* enable regulator */
	ret = regulator_enable(disp_bias_pos);
	if (ret < 0)
		pr_err("enable regulator disp_bias_pos fail, ret = %d\n", ret);
	retval |= ret;

	ret = regulator_enable(disp_bias_neg);
	if (ret < 0)
		pr_err("enable regulator disp_bias_neg fail, ret = %d\n", ret);
	retval |= ret;

	return retval;
}

static int lcm_panel_bias_disable(void)
{
	int ret = 0;
	int retval = 0;

	lcm_panel_bias_regulator_init();

	ret = regulator_disable(disp_bias_neg);
	if (ret < 0)
		pr_err("disable regulator disp_bias_neg fail, ret = %d\n", ret);
	retval |= ret;

	ret = regulator_disable(disp_bias_pos);
	if (ret < 0)
		pr_err("disable regulator disp_bias_pos fail, ret = %d\n", ret);
	retval |= ret;

	return retval;
}
#endif
#endif

static void lcm_panel_init(struct lcm *ctx)
{
	ctx->key_led_gpio =
		devm_gpiod_get(ctx->dev, "key-led", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->key_led_gpio)) {
		dev_err(ctx->dev, "%s: cannot get key_led_gpio %ld\n",
			__func__, PTR_ERR(ctx->key_led_gpio));
		return;
	}
	gpiod_set_value(ctx->key_led_gpio, 1);
	devm_gpiod_put(ctx->dev, ctx->key_led_gpio);
    msleep(5);
    
	ctx->lcm_1v8_gpio =
		devm_gpiod_get(ctx->dev, "lcm-1v8", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->lcm_1v8_gpio)) {
		dev_err(ctx->dev, "%s: cannot get lcm_1v8_gpio %ld\n",
			__func__, PTR_ERR(ctx->lcm_1v8_gpio));
		return;
	}
	gpiod_set_value(ctx->lcm_1v8_gpio, 1);
	devm_gpiod_put(ctx->dev, ctx->lcm_1v8_gpio);
    msleep(5);
    
	ctx->dsi_en_gpio =
		devm_gpiod_get(ctx->dev, "dsi-en", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->dsi_en_gpio)) {
		dev_err(ctx->dev, "%s: cannot get dsi_en_gpio %ld\n",
			__func__, PTR_ERR(ctx->dsi_en_gpio));
		return;
	}
	gpiod_set_value(ctx->dsi_en_gpio, 1);
	devm_gpiod_put(ctx->dev, ctx->dsi_en_gpio);
    msleep(5);
    
	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(ctx->dev, "%s: cannot get reset_gpio %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return;
	}
    
	gpiod_set_value(ctx->reset_gpio, 0);
	msleep(10);
	gpiod_set_value(ctx->reset_gpio, 1);
	msleep(10);
	gpiod_set_value(ctx->reset_gpio, 0);
	msleep(10);
	gpiod_set_value(ctx->reset_gpio, 1);
	msleep(120);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	/*
	ctx->tp_rst_gpio =
		devm_gpiod_get(ctx->dev, "tp-rst", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->tp_rst_gpio)) {
		dev_err(ctx->dev, "%s: cannot get tp_rst_gpio %ld\n",
			__func__, PTR_ERR(ctx->tp_rst_gpio));
		return;
	}
    gpiod_set_value(ctx->tp_rst_gpio, 1);
	msleep(5);
	gpiod_set_value(ctx->tp_rst_gpio, 0);
	msleep(10);
	gpiod_set_value(ctx->tp_rst_gpio, 1);
	msleep(5);
    devm_gpiod_put(ctx->dev, ctx->tp_rst_gpio);
	*/

	lcm_dcs_write_seq_static(ctx,0x41,0x5A);

	lcm_dcs_write_seq_static(ctx,0x4C,0x03);

	lcm_dcs_write_seq_static(ctx,0x41,0x5A,0x24);
	lcm_dcs_write_seq_static(ctx,0x90,0x5A);

	lcm_dcs_write_seq_static(ctx,0x41,0x5A,0x03);
	lcm_dcs_write_seq_static(ctx,0x80,0x25,0x01);

	lcm_dcs_write_seq_static(ctx,0x41,0x5A,0x08);
	lcm_dcs_write_seq_static(ctx,0x80,0xC8,0x2C,0x01);
	lcm_dcs_write_seq_static(ctx,0x41,0x5A,0x09);
	lcm_dcs_write_seq_static(ctx,0x80,0x5A,0x51,0xB5,0x2A,0x6C,0xC5,0x4A,0x01,0x40,0x62,0x0F,0x82,0x20,0x08,0xF0,0xB7);
	lcm_dcs_write_seq_static(ctx,0x90,0x00,0x24,0x42,0x0A,0xE3,0x91,0xA4,0xF0,0xB9,0x61,0x69,0x20,0x69,0x81,0x26,0x00);
	lcm_dcs_write_seq_static(ctx,0xA0,0x51,0x55,0x55,0x00,0xA0,0x4D,0x06,0x11,0x0D,0x60,0x5A,0xFF,0xFF,0x03,0xA5,0xE6);
	lcm_dcs_write_seq_static(ctx,0xB0,0x08,0xC9,0x16,0x64,0x0B,0x00,0x00,0x11,0x07,0x60,0x00,0xFF,0xFF,0x03,0xFF,0x34);
	lcm_dcs_write_seq_static(ctx,0xC0,0x0C,0x3F,0x1F,0x9F,0x0F,0x00,0x08,0x00);
	lcm_dcs_write_seq_static(ctx,0x41,0x5A,0x0A);
	lcm_dcs_write_seq_static(ctx,0x80,0xFF,0xB6,0x6D,0x03,0x00,0x0F,0x1B,0x2B,0x3D,0x4E,0x55,0x89,0x68,0xA5,0x8E,0x56);
	lcm_dcs_write_seq_static(ctx,0x90,0x85,0x58,0x53,0x43,0x33,0x23,0x16,0x0B,0x00,0x0F,0x1B,0x2B,0x3D,0x4E,0x55,0x89);
	lcm_dcs_write_seq_static(ctx,0xA0,0x68,0xA5,0x8E,0x56,0x85,0x58,0x53,0x43,0x33,0x23,0x16,0x0B,0x00);
	lcm_dcs_write_seq_static(ctx,0x41,0x5A,0x0B); 
	lcm_dcs_write_seq_static(ctx,0x80,0x00,0x00,0x20,0x44,0x08,0x00,0xA0,0x47,0x00,0x00,0x10,0x22,0x04,0x00,0xB0,0x23);
	lcm_dcs_write_seq_static(ctx,0x90,0x15,0x00);
	lcm_dcs_write_seq_static(ctx,0x41,0x5A,0x0C);
	lcm_dcs_write_seq_static(ctx,0x80,0xBA,0x68,0x68,0x01,0x2D,0x6E,0x90,0x0B,0x00,0x60,0x15,0x00,0x50,0x15,0x56,0x51);
	lcm_dcs_write_seq_static(ctx,0x90,0x15,0x55,0x61,0x15,0x00,0x60,0x15,0x00,0x50,0x15,0x56,0x51,0x15,0x55,0x61,0x95);
	lcm_dcs_write_seq_static(ctx,0xA0,0xAB,0x18,0x00,0x05,0x00,0x05,0x00,0x05,0x00,0x49,0x29,0x84,0x52,0x01,0x09,0x00);
	lcm_dcs_write_seq_static(ctx,0xB0,0x00,0x00);
	lcm_dcs_write_seq_static(ctx,0x41,0x5A,0x0D);
	lcm_dcs_write_seq_static(ctx,0x80,0xF0,0xB1,0x71,0xEF,0x4B,0xC0,0x80);
	lcm_dcs_write_seq_static(ctx,0x41,0x5A,0x0E);
	lcm_dcs_write_seq_static(ctx,0x80,0xFF,0x01,0x55,0x55,0x32,0x88,0x88,0x1C);
	lcm_dcs_write_seq_static(ctx,0x41,0x5A,0x0F);
	lcm_dcs_write_seq_static(ctx,0x80,0x9B,0x06,0x70,0xE0,0x11,0xA0,0x63,0x08,0x52,0x51,0x68,0x49,0x03,0x52,0x4C,0x4C);
	lcm_dcs_write_seq_static(ctx,0x90,0x68,0x68,0x68,0x4C,0x4C,0x7C,0x14,0x00,0x20,0x06,0xC2,0x00,0x04,0x06,0x0C,0x00);
	lcm_dcs_write_seq_static(ctx,0xA0,0x00,0x92,0x00,0x00);
	lcm_dcs_write_seq_static(ctx,0x41,0x5A,0x10);
	lcm_dcs_write_seq_static(ctx,0x80,0x00,0x00,0x03,0xE7,0x1F,0x17,0x00,0x48,0x40,0x02,0xCF,0x18,0x30,0x88,0x41,0x8A);
	lcm_dcs_write_seq_static(ctx,0x90,0x39,0x28,0xA9,0xC5,0x9A,0x7B,0xF0,0x07,0x7E,0xE0,0x07,0x7E,0x20,0x10,0x00);
	lcm_dcs_write_seq_static(ctx,0x41,0x5A,0x11);
	lcm_dcs_write_seq_static(ctx,0x80,0x49,0x77,0x03,0x40,0xCA,0xF4,0xFF,0xA3,0x40,0x08,0xC4,0x06,0xA1,0xD8,0x24,0x18);
	lcm_dcs_write_seq_static(ctx,0x90,0x30,0xC6,0x66,0xC1,0x80,0x51,0x15,0xCB,0xE5,0xD2,0x68,0x6C,0x36,0x1D,0x04,0xC8);
	lcm_dcs_write_seq_static(ctx,0xA0,0xB0,0xD9,0x88,0x60,0xB0,0x81,0x40,0x1A,0x1B,0x48,0x63,0x03,0xB9,0x00,0x1C,0x80);
	lcm_dcs_write_seq_static(ctx,0xB0,0x50,0x30,0x00,0xE0,0xE1,0x01,0x00,0x28,0x0E,0x06,0x43,0x55,0x55,0x95,0x55,0x55);
	lcm_dcs_write_seq_static(ctx,0xC0,0x15,0x11,0x11,0x11,0x11,0x11,0x11,0x11,0x88,0xC6,0xE3,0x81,0x00,0x20,0x00,0x21);
	lcm_dcs_write_seq_static(ctx,0xD0,0x42,0x88,0x20,0x00,0x10,0x00,0x40,0x80,0x24,0x32,0x04,0x41,0x06,0x00,0x00,0x00);
	lcm_dcs_write_seq_static(ctx,0xE0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x92,0x04,0x00,0x85,0x11,0x0C);
	lcm_dcs_write_seq_static(ctx,0xF0,0x00,0x00,0x40,0x00,0x00,0x00,0x00,0x5E,0x4A,0x01,0x78,0x00,0x08,0x00,0x00);
	lcm_dcs_write_seq_static(ctx,0x41,0x5A,0x12);
	lcm_dcs_write_seq_static(ctx,0x80,0x00,0x00,0x00,0x00,0x00,0x02,0x03,0x00,0x00,0x00,0x00,0x02,0x03,0x01,0x41,0x37);
	lcm_dcs_write_seq_static(ctx,0x90,0xF1,0xE7,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x2D,0x23,0x05);
	lcm_dcs_write_seq_static(ctx,0xA0,0xFB,0x08,0x2D,0x23,0x05,0xFB,0x0C);
	lcm_dcs_write_seq_static(ctx,0x41,0x5A,0x13);
	lcm_dcs_write_seq_static(ctx,0x80,0xFD,0x0F,0x00,0x0C,0x00,0x00,0x00,0x00,0x01,0x08,0x01,0x1C,0x44,0x0C,0xCE,0xE7);
	lcm_dcs_write_seq_static(ctx,0x90,0x62,0x0E,0x24,0x98,0xAC,0x21,0x01,0x00,0xD0,0x93,0x24,0x49,0x06,0x20);
	lcm_dcs_write_seq_static(ctx,0x41,0x5A,0x14);
	lcm_dcs_write_seq_static(ctx,0x80,0x01,0x02,0x41,0x36,0xE9,0xEF,0xF7,0xFB,0xFD,0x7E,0x00,0x00,0x00,0x68,0xC5,0x0F);
	lcm_dcs_write_seq_static(ctx,0x90,0x59,0x56,0x20,0x76,0xC8,0xB2,0x02,0xF0,0xE1,0x49,0x0F,0x80,0x0D,0x4F,0x7A,0x00);
	lcm_dcs_write_seq_static(ctx,0xA0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00);
	lcm_dcs_write_seq_static(ctx,0xB0,0x00,0x00,0x00,0x00,0x00,0x00,0x10,0x00,0x00,0x1C,0xAD,0xD1,0x2A,0x00,0x40,0xA1);
	lcm_dcs_write_seq_static(ctx,0xC0,0x50,0xB8,0x0F,0xE4,0x74,0x91,0x51,0x1C,0xF7,0x80,0x9C,0x2E,0x32,0x8A,0xE3,0x06);
	lcm_dcs_write_seq_static(ctx,0xD0,0x90,0xD3,0x45,0x46,0x71,0xDC,0x13,0x72,0xBA,0xC8,0x28,0x8E,0xFB,0x41,0x4E,0x17);
	lcm_dcs_write_seq_static(ctx,0xE0,0x19,0xC5,0x71,0x2F,0xC8,0xE9,0x22,0xA3,0x38,0x00,0x00,0x39,0x3D,0x64,0x14,0x07);
	lcm_dcs_write_seq_static(ctx,0xF0,0x00,0x20,0xA7,0x87,0x8C,0xE2,0x18,0x01,0xBF,0x5F,0x08,0x00,0x00,0x00,0x00,0x00);
	lcm_dcs_write_seq_static(ctx,0x41,0x5A,0x15);
	lcm_dcs_write_seq_static(ctx,0x80,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x82,0xD6,0xE0,0xF7);
	lcm_dcs_write_seq_static(ctx,0x90,0xA3,0x00,0xD0,0x4A,0xC8,0x49,0x4E,0x07,0x91,0x91,0x17,0x72,0x92,0xD3,0x41,0x64);
	lcm_dcs_write_seq_static(ctx,0xA0,0x78,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x68);
	lcm_dcs_write_seq_static(ctx,0xB0,0x15,0x72,0x92,0xD3,0x00,0x00,0x00,0x40,0x12,0x00,0x00,0x00,0x94,0xD6,0x0C,0x4D);
	lcm_dcs_write_seq_static(ctx,0xC0,0x29,0x18,0x8A,0x23,0x59,0x48,0xB0,0x6D,0xDB,0xB6,0x6D,0xDB,0xB6,0x6D,0xC9,0xD0);
	lcm_dcs_write_seq_static(ctx,0xD0,0x84,0x14,0xC4,0x20,0x04,0xC0,0x04,0xDB,0xB6,0x6D,0xDB,0xB6,0x6D,0xDB,0xD6,0x0C);
	lcm_dcs_write_seq_static(ctx,0xE0,0x4D,0x08,0x10,0x08,0x03,0x51,0x48,0xB0,0x6D,0xDB,0xB6,0x6D,0xDB,0xB6,0x6D,0xC9);
	lcm_dcs_write_seq_static(ctx,0xF0,0xD0,0x94,0x96,0xE4,0x28,0x86,0x20,0x04,0xDB,0xB6,0x6D,0xDB,0xB6,0x6D,0x5B,0xC0);
	lcm_dcs_write_seq_static(ctx,0x41,0x5A,0x16);
	lcm_dcs_write_seq_static(ctx,0x80,0xC3,0x43,0x41,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00);
	lcm_dcs_write_seq_static(ctx,0x90,0x00,0x00,0x00,0x00,0xF0,0x20);
	lcm_dcs_write_seq_static(ctx,0x41,0x5A,0x18);
	lcm_dcs_write_seq_static(ctx,0x80,0xEF,0xBD,0xF7,0xDE,0x7B,0xEF,0xBD,0x07,0x08,0x08,0x0A,0x0C,0x0C,0x0C,0x0C,0x0C);
	lcm_dcs_write_seq_static(ctx,0x90,0x0C,0x0C,0x0C,0x5C,0x09,0xA8,0xAA,0xAA,0xAA,0xAA,0xAA,0xAA,0xAA,0xAA,0xAA,0x5A);
	lcm_dcs_write_seq_static(ctx,0xA0,0x59,0x59,0x59,0x59,0x59,0x59,0x59,0x59,0x59,0x59,0x59,0x09,0x04,0xFF,0x00,0x80);
	lcm_dcs_write_seq_static(ctx,0xB0,0x80,0x00,0x04,0x20,0x00,0x01,0x08,0x40,0x00,0x02,0x10,0x80,0x00,0x04,0x00);
	lcm_dcs_write_seq_static(ctx,0x41,0x5A,0x19);
	lcm_dcs_write_seq_static(ctx,0x80,0xC0,0xAF,0xA3,0x9B,0x92,0x8D,0x8A,0x86,0x84,0x83,0x82,0x81,0x00,0x50,0xF6,0xCF);
	lcm_dcs_write_seq_static(ctx,0x90,0xFC,0x2F,0xF3,0xEF,0xCF,0xBF,0x0F,0xFF,0xAF,0xB5,0x71,0x0E,0x6C,0x4A,0x69,0x08);
	lcm_dcs_write_seq_static(ctx,0xA0,0x00,0x00,0x04,0x00);
	lcm_dcs_write_seq_static(ctx,0x41,0x5A,0x1A);
	lcm_dcs_write_seq_static(ctx,0x80,0x00,0x04,0x08,0x0C,0x00,0x10,0x14,0x18,0x1C,0x00,0x20,0x28,0x30,0x38,0x00,0x40);
	lcm_dcs_write_seq_static(ctx,0x90,0x48,0x50,0x58,0x00,0x60,0x68,0x70,0x78,0x00,0x80,0x88,0x90,0x98,0x00,0xA0,0xA8);
	lcm_dcs_write_seq_static(ctx,0xA0,0xB0,0xB8,0x00,0xC0,0xC8,0xD0,0xD8,0x00,0xE0,0xE8,0xF0,0xF8,0x00,0xFC,0xFE,0xFF);
	lcm_dcs_write_seq_static(ctx,0xB0,0x00,0x00,0x04,0x08,0x0C,0x00,0x10,0x14,0x18,0x1C,0x00,0x20,0x28,0x30,0x38,0x00);
	lcm_dcs_write_seq_static(ctx,0xC0,0x40,0x48,0x50,0x58,0x00,0x60,0x68,0x70,0x78,0x00,0x80,0x88,0x90,0x98,0x00,0xA0);
	lcm_dcs_write_seq_static(ctx,0xD0,0xA8,0xB0,0xB8,0x00,0xC0,0xC8,0xD0,0xD8,0x00,0xE0,0xE8,0xF0,0xF8,0x00,0xFC,0xFE);
	lcm_dcs_write_seq_static(ctx,0xE0,0xFF,0x00,0x00,0x04,0x08,0x0C,0x00,0x10,0x14,0x18,0x1C,0x00,0x20,0x28,0x30,0x38);
	lcm_dcs_write_seq_static(ctx,0xF0,0x00,0x40,0x48,0x50,0x58,0x00,0x60,0x68,0x70,0x78,0x00,0x80,0x88,0x90,0x98,0x00);
	lcm_dcs_write_seq_static(ctx,0x41,0x5A,0x1B);
	lcm_dcs_write_seq_static(ctx,0x80,0xA0,0xA8,0xB0,0xB8,0x00,0xC0,0xC8,0xD0,0xD8,0x00,0xE0,0xE8,0xF0,0xF8,0x00,0xFC);
	lcm_dcs_write_seq_static(ctx,0x90,0xFE,0xFF,0x00,0x00);
	lcm_dcs_write_seq_static(ctx,0x41,0x5A,0x20);
	lcm_dcs_write_seq_static(ctx,0x80,0x74,0x00,0x00,0x00,0x00,0x00,0x00);
	lcm_dcs_write_seq_static(ctx,0x41,0x5A,0x22);
	lcm_dcs_write_seq_static(ctx,0x80,0x2D,0xD3,0x00,0x15,0x50,0x55,0x55,0x55,0x01,0x00,0x50,0x9F,0x00);
	lcm_dcs_write_seq_static(ctx,0x41,0x5A,0x23);
	lcm_dcs_write_seq_static(ctx,0x80,0x01,0x05,0x00,0x05,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00);
	lcm_dcs_write_seq_static(ctx,0x90,0xFF,0x0F,0x00,0x00,0x00,0x03,0x00,0x00,0x00,0xFF,0x07,0x35);
	lcm_dcs_write_seq_static(ctx,0x41,0x5A,0x24);
	lcm_dcs_write_seq_static(ctx,0x80,0x00,0x03,0x00,0xFF,0xFF,0x20,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xCD,0x53);
	lcm_dcs_write_seq_static(ctx,0x90,0x5A,0x5A,0x5A,0x55,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00);

	lcm_dcs_write_seq_static(ctx,0x42,0x24);
	lcm_dcs_write_seq_static(ctx,0x90,0x00);
	lcm_dcs_write_seq_static(ctx,0x41,0x5A,0x2F);
	lcm_dcs_write_seq_static(ctx,0x19,0x00);

	lcm_dcs_write_seq_static(ctx,0x35,0x00);
    lcm_dcs_write_seq_static(ctx,0x11);
    msleep(120);
    lcm_dcs_write_seq_static(ctx,0x29);
    lcm_dcs_write_seq_static(ctx,0xFE,0x66,0x99,0x55);
    msleep(50);	 

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

static int lcm_unprepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	if (!ctx->prepared)
		return 0;

	lcm_dcs_write_seq_static(ctx, 0x28);
	msleep(120);
	lcm_dcs_write_seq_static(ctx, 0x10);
	msleep(50);
	ctx->error = 0;
	ctx->prepared = false;
    
    ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(ctx->dev, "%s: cannot get reset_gpio %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	gpiod_set_value(ctx->reset_gpio, 1);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);
    msleep(50);
    
    ctx->key_led_gpio =
		devm_gpiod_get(ctx->dev, "key-led", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->key_led_gpio)) {
		dev_err(ctx->dev, "%s: cannot get key_led_gpio %ld\n",
			__func__, PTR_ERR(ctx->key_led_gpio));
		return PTR_ERR(ctx->key_led_gpio);
	}
	gpiod_set_value(ctx->key_led_gpio, 0);
	devm_gpiod_put(ctx->dev, ctx->key_led_gpio);
    msleep(5);
    
	ctx->lcm_1v8_gpio =
		devm_gpiod_get(ctx->dev, "lcm-1v8", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->lcm_1v8_gpio)) {
		dev_err(ctx->dev, "%s: cannot get lcm_1v8_gpio %ld\n",
			__func__, PTR_ERR(ctx->lcm_1v8_gpio));
		return PTR_ERR(ctx->lcm_1v8_gpio);
	}
	gpiod_set_value(ctx->lcm_1v8_gpio, 0);
	devm_gpiod_put(ctx->dev, ctx->lcm_1v8_gpio);
    msleep(5);
    
	ctx->dsi_en_gpio =
		devm_gpiod_get(ctx->dev, "dsi-en", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->dsi_en_gpio)) {
		dev_err(ctx->dev, "%s: cannot get dsi_en_gpio %ld\n",
			__func__, PTR_ERR(ctx->dsi_en_gpio));
		return PTR_ERR(ctx->dsi_en_gpio);
	}
	gpiod_set_value(ctx->dsi_en_gpio, 0);
	devm_gpiod_put(ctx->dev, ctx->dsi_en_gpio);
    msleep(5);
#if 1//!IS_ENABLED(CONFIG_RT4831A_I2C)
#if 1//IS_ENABLED(CONFIG_RT5081_PMU_DSV) || IS_ENABLED(CONFIG_DEVICE_MODULES_REGULATOR_MT6370)
	lcm_panel_bias_disable();
#else
	
	if (ctx->gate_ic == 0) {
		ctx->bias_neg = devm_gpiod_get_index(ctx->dev,
			"bias", 1, GPIOD_OUT_HIGH);
		if (IS_ERR(ctx->bias_neg)) {
			dev_err(ctx->dev, "%s: cannot get bias_neg %ld\n",
				__func__, PTR_ERR(ctx->bias_neg));
			return PTR_ERR(ctx->bias_neg);
		}
		gpiod_set_value(ctx->bias_neg, 0);
		devm_gpiod_put(ctx->dev, ctx->bias_neg);

		udelay(1000);

		ctx->bias_pos = devm_gpiod_get_index(ctx->dev,
			"bias", 0, GPIOD_OUT_HIGH);
		if (IS_ERR(ctx->bias_pos)) {
			dev_err(ctx->dev, "%s: cannot get bias_pos %ld\n",
				__func__, PTR_ERR(ctx->bias_pos));
			return PTR_ERR(ctx->bias_pos);
		}
		gpiod_set_value(ctx->bias_pos, 0);
		devm_gpiod_put(ctx->dev, ctx->bias_pos);
	}

#endif
#endif

	return 0;
}

static int lcm_prepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	int ret;

	pr_info("%s\n", __func__);
	if (ctx->prepared)
		return 0;
#if 1//!IS_ENABLED(CONFIG_RT4831A_I2C)
#if 1//IS_ENABLED(CONFIG_RT5081_PMU_DSV) || IS_ENABLED(CONFIG_DEVICE_MODULES_REGULATOR_MT6370)
	lcm_panel_bias_enable();
#else

	if (ctx->gate_ic == 0) {

		ctx->bias_pos = devm_gpiod_get_index(ctx->dev,
			"bias", 0, GPIOD_OUT_HIGH);
		if (IS_ERR(ctx->bias_pos)) {
			dev_err(ctx->dev, "%s: cannot get bias_pos %ld\n",
				__func__, PTR_ERR(ctx->bias_pos));
			return PTR_ERR(ctx->bias_pos);
		}
		gpiod_set_value(ctx->bias_pos, 1);
		devm_gpiod_put(ctx->dev, ctx->bias_pos);

		//udelay(1000);
		udelay(1000);

		ctx->bias_neg = devm_gpiod_get_index(ctx->dev,
			"bias", 1, GPIOD_OUT_HIGH);
		if (IS_ERR(ctx->bias_neg)) {
			dev_err(ctx->dev, "%s: cannot get bias_neg %ld\n",
				__func__, PTR_ERR(ctx->bias_neg));
			return PTR_ERR(ctx->bias_neg);
		}
		gpiod_set_value(ctx->bias_neg, 1);
		devm_gpiod_put(ctx->dev, ctx->bias_neg);
	}

#endif
#endif

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


#define HFP (30)
#define HSA (10)
#define HBP (20)
#define VFP (110)
#define VSA (8)
#define VBP (110)
#define VAC (1440)
#define HAC (720)

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
		dev_err(ctx->dev, "%s: cannot get reset_gpio %ld\n",
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
	unsigned char data[1] = {0x00};
	unsigned char id[1] = {0x72};
	ssize_t ret;

	ret = mipi_dsi_dcs_read(dsi, 0x4, data, 1);
	if (ret < 0) {
		pr_err("%s error\n", __func__);
		return 0;
	}

	pr_info("ATA read data %x\n", data[0]);

	if (data[0] == id[0])

		return 1;

	pr_info("ATA expect read data is %x\n",
			id[0]);

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
	.pll_clk = 250,
	//.data_rate = 1050,
	//.vfp_low_power = 750,
	.cust_esd_check = 1,
	.esd_check_enable = 1,
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
		dev_info(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			default_mode.hdisplay, default_mode.vdisplay,
			drm_mode_vrefresh(&default_mode));
		return -ENOMEM;
	}

	drm_mode_set_name(mode);
	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(connector, mode);

	connector->display_info.width_mm = 64;
	connector->display_info.height_mm = 129;

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
	unsigned int value;
	int ret;

	pr_info("%s+boe545\n", __func__);

	dsi_node = of_get_parent(dev->of_node);
	if (dsi_node) {
		endpoint = of_graph_get_next_endpoint(dsi_node, NULL);
		if (endpoint) {
			remote_node = of_graph_get_remote_port_parent(endpoint);
			if (!remote_node) {
				pr_info("No panel connected,skip probe lcm\n");
				return -ENODEV;
			}
			pr_info("boe545-lcm device node name:%s\n", remote_node->name);
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

	ret = of_property_read_u32(dev->of_node, "gate-ic", &value);
	if (ret < 0)
		value = 0;
	else
		ctx->gate_ic = value;

	backlight = of_parse_phandle(dev->of_node, "backlight", 0);
	if (backlight) {
		ctx->backlight = of_find_backlight_by_node(backlight);
		of_node_put(backlight);

		if (!ctx->backlight)
			return -EPROBE_DEFER;
	}

	ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(dev, "%s: cannot get reset-gpios %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	devm_gpiod_put(dev, ctx->reset_gpio);
	
	ctx->lcm_1v8_gpio = devm_gpiod_get(dev, "lcm-1v8", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->lcm_1v8_gpio)) {
		dev_err(dev, "%s: cannot get lcm-1v8-gpios %ld\n",
			__func__, PTR_ERR(ctx->lcm_1v8_gpio));
		return PTR_ERR(ctx->lcm_1v8_gpio);
	}
	devm_gpiod_put(dev, ctx->lcm_1v8_gpio);
    
    ctx->dsi_en_gpio = devm_gpiod_get(dev, "dsi-en", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->dsi_en_gpio)) {
		dev_err(dev, "%s: cannot get dsi-en-gpios %ld\n",
			__func__, PTR_ERR(ctx->dsi_en_gpio));
		return PTR_ERR(ctx->dsi_en_gpio);
	}
	devm_gpiod_put(dev, ctx->dsi_en_gpio);
    
    ctx->key_led_gpio = devm_gpiod_get(dev, "key-led", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->key_led_gpio)) {
		dev_err(dev, "%s: cannot get key-led-gpios %ld\n",
			__func__, PTR_ERR(ctx->key_led_gpio));
		return PTR_ERR(ctx->key_led_gpio);
	}
	devm_gpiod_put(dev, ctx->key_led_gpio);
    
    ctx->tp_rst_gpio = devm_gpiod_get(dev, "tp-rst", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->tp_rst_gpio)) {
		dev_err(dev, "%s: cannot get key-led-gpios %ld\n",
			__func__, PTR_ERR(ctx->tp_rst_gpio));
		return PTR_ERR(ctx->tp_rst_gpio);
	}
	devm_gpiod_put(dev, ctx->tp_rst_gpio);
    
#if 1//!IS_ENABLED(CONFIG_RT4831A_I2C)
#if 1//IS_ENABLED(CONFIG_RT5081_PMU_DSV) || IS_ENABLED(CONFIG_DEVICE_MODULES_REGULATOR_MT6370)
	lcm_panel_bias_enable();
#else
	if (ctx->gate_ic == 0) {
		ctx->bias_pos = devm_gpiod_get_index(dev, "bias", 0, GPIOD_OUT_HIGH);
		if (IS_ERR(ctx->bias_pos)) {
			dev_err(dev, "%s: cannot get bias-pos 0 %ld\n",
				__func__, PTR_ERR(ctx->bias_pos));
			return PTR_ERR(ctx->bias_pos);
		}
		devm_gpiod_put(dev, ctx->bias_pos);

		ctx->bias_neg = devm_gpiod_get_index(dev, "bias", 1, GPIOD_OUT_HIGH);
		if (IS_ERR(ctx->bias_neg)) {
			dev_err(dev, "%s: cannot get bias-neg 1 %ld\n",
				__func__, PTR_ERR(ctx->bias_neg));
			return PTR_ERR(ctx->bias_neg);
		}
		devm_gpiod_put(dev, ctx->bias_neg);
	}
#endif
#endif
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
	pr_info("%s-jd9616z\n", __func__);

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
	{ .compatible = "boe545,boe,vdo", },
	{ }
};

MODULE_DEVICE_TABLE(of, lcm_of_match);

static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.driver = {
		.name = "panel-boe545-boe-hdplus-vdo-dsi",
		.owner = THIS_MODULE,
		.of_match_table = lcm_of_match,
	},
};

module_mipi_dsi_driver(lcm_driver);

MODULE_AUTHOR("smile <smile@sunritel.com.cn>");
MODULE_DESCRIPTION("boe545 vdo Panel Driver");
MODULE_LICENSE("GPL v2");