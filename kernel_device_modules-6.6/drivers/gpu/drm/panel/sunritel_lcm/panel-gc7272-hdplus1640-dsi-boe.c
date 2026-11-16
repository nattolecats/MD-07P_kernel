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
static int current_fps = 60;
//static u32 fake_heigh = 1640;
//static u32 fake_width = 720;
static int lcm_gc7272_shutdown_status = 0; 

extern void ocp2131_set_avdd_voltage(unsigned int voltagelevel);
extern void ocp2131_set_avee_voltage(unsigned int voltagelevel);
extern int st_drm_shutdown_status;

//  #include <linux/workqueue.h>

// static struct workqueue_struct *st_bc12_check_workqueue;
// static struct delayed_work st_bc12_check_work;
// static struct lcm *st_ctx;;


//static bool need_fake_resolution;
static int lcm_unprepare(struct drm_panel *panel);
struct lcm {
	struct device *dev;
	struct drm_panel panel;
	struct backlight_device *backlight;
	struct gpio_desc *reset_gpio;
//	struct gpio_desc *tp_reset_gpio;
	struct gpio_desc *avdd_enable_gpio;
	struct gpio_desc *avee_enable_gpio;
//	struct gpio_desc *dsi_te_enable_gpio;
	//struct gpio_desc *lcm_1v8_enable_gpio;

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

// #if defined(CONFIG_RT5081_PMU_DSV) || defined(CONFIG_MT6370_PMU_DSV)
// static struct regulator *disp_bias_pos;
// static struct regulator *disp_bias_neg;


// static int lcm_panel_bias_regulator_init(void)
// {
// 	static int regulator_inited;
// 	int ret = 0;

// 	if (regulator_inited)
// 		return ret;

// 	/* please only get regulator once in a driver */
// 	disp_bias_pos = regulator_get(NULL, "dsv_pos");
// 	if (IS_ERR(disp_bias_pos)) { /* handle return value */
// 		ret = PTR_ERR(disp_bias_pos);
// 		pr_err("get dsv_pos fail, error: %d\n", ret);
// 		return ret;
// 	}

// 	disp_bias_neg = regulator_get(NULL, "dsv_neg");
// 	if (IS_ERR(disp_bias_neg)) { /* handle return value */
// 		ret = PTR_ERR(disp_bias_neg);
// 		pr_err("get dsv_neg fail, error: %d\n", ret);
// 		return ret;
// 	}

// 	regulator_inited = 1;
// 	return ret; /* must be 0 */

// }

// static int lcm_panel_bias_enable(void)
// {
// 	int ret = 0;
// 	int retval = 0;

// 	lcm_panel_bias_regulator_init();

// 	/* set voltage with min & max*/
// 	ret = regulator_set_voltage(disp_bias_pos, 5400000, 5400000);
// 	if (ret < 0)
// 		pr_err("set voltage disp_bias_pos fail, ret = %d\n", ret);
// 	retval |= ret;

// 	ret = regulator_set_voltage(disp_bias_neg, 5400000, 5400000);
// 	if (ret < 0)
// 		pr_err("set voltage disp_bias_neg fail, ret = %d\n", ret);
// 	retval |= ret;

// 	/* enable regulator */
// 	ret = regulator_enable(disp_bias_pos);
// 	if (ret < 0)
// 		pr_err("enable regulator disp_bias_pos fail, ret = %d\n", ret);
// 	retval |= ret;

// 	ret = regulator_enable(disp_bias_neg);
// 	if (ret < 0)
// 		pr_err("enable regulator disp_bias_neg fail, ret = %d\n", ret);
// 	retval |= ret;

// 	return retval;
// }

// static int lcm_panel_bias_disable(void)
// {
// 	int ret = 0;
// 	int retval = 0;

// 	lcm_panel_bias_regulator_init();

// 	ret = regulator_disable(disp_bias_neg);
// 	if (ret < 0)
// 		pr_err("disable regulator disp_bias_neg fail, ret = %d\n", ret);
// 	retval |= ret;

// 	ret = regulator_disable(disp_bias_pos);
// 	if (ret < 0)
// 		pr_err("disable regulator disp_bias_pos fail, ret = %d\n", ret);
// 	retval |= ret;

// 	return retval;
// }
// #endif

static void lcm_panel_init(struct lcm *ctx)
{
	printk("dsy  %s %d \n", __func__,__LINE__);



lcm_dcs_write_seq_static(ctx,0xFF,0x55,0xAA,0x66);
lcm_dcs_write_seq_static(ctx,0xFF,0x10);
lcm_dcs_write_seq_static(ctx,0xFB,0x00);
lcm_dcs_write_seq_static(ctx,0xFF,0x20);
lcm_dcs_write_seq_static(ctx,0xFB,0x00);
lcm_dcs_write_seq_static(ctx,0xFF,0x21);
lcm_dcs_write_seq_static(ctx,0xFB,0x00);
lcm_dcs_write_seq_static(ctx,0xFF,0x22);
lcm_dcs_write_seq_static(ctx,0xFB,0x00);
lcm_dcs_write_seq_static(ctx,0xFF,0x23);
lcm_dcs_write_seq_static(ctx,0xFB,0x00);
lcm_dcs_write_seq_static(ctx,0xFF,0x24);
lcm_dcs_write_seq_static(ctx,0xFB,0x00);
lcm_dcs_write_seq_static(ctx,0xFF,0x25);
lcm_dcs_write_seq_static(ctx,0xFB,0x00);
lcm_dcs_write_seq_static(ctx,0xFF,0x27);
lcm_dcs_write_seq_static(ctx,0xFB,0x00);
lcm_dcs_write_seq_static(ctx,0xFF,0x26);
lcm_dcs_write_seq_static(ctx,0xFB,0x00);
lcm_dcs_write_seq_static(ctx,0xFF,0x28);
lcm_dcs_write_seq_static(ctx,0xFB,0x00);
lcm_dcs_write_seq_static(ctx,0xFF,0xA3);
lcm_dcs_write_seq_static(ctx,0xFB,0x00);
lcm_dcs_write_seq_static(ctx,0xFF,0xB3);
lcm_dcs_write_seq_static(ctx,0xFB,0x00);
lcm_dcs_write_seq_static(ctx,0xFF,0x20);
lcm_dcs_write_seq_static(ctx,0x2D,0x55);
lcm_dcs_write_seq_static(ctx,0xA3,0x16);
lcm_dcs_write_seq_static(ctx,0xA7,0x16);
lcm_dcs_write_seq_static(ctx,0xFF,0xB3);
lcm_dcs_write_seq_static(ctx,0x4E,0x44);

lcm_dcs_write_seq_static(ctx,0xFF,0xB3);
lcm_dcs_write_seq_static(ctx,0x3F,0x37);
lcm_dcs_write_seq_static(ctx,0x50,0x10);
lcm_dcs_write_seq_static(ctx,0xFF,0x20);
lcm_dcs_write_seq_static(ctx,0x29,0xE0);
lcm_dcs_write_seq_static(ctx,0x20,0x84);
lcm_dcs_write_seq_static(ctx,0x21,0x00);
lcm_dcs_write_seq_static(ctx,0x22,0x80);
lcm_dcs_write_seq_static(ctx,0x2E,0x00);
lcm_dcs_write_seq_static(ctx,0xFF,0xA3);
lcm_dcs_write_seq_static(ctx,0x58,0xAA);
lcm_dcs_write_seq_static(ctx,0xFF,0x26);
lcm_dcs_write_seq_static(ctx,0x43,0x00);

lcm_dcs_write_seq_static(ctx,0xFF,0x22);
lcm_dcs_write_seq_static(ctx,0xE4,0x00);
lcm_dcs_write_seq_static(ctx,0x0E,0x33);
lcm_dcs_write_seq_static(ctx,0x0C,0x01);
lcm_dcs_write_seq_static(ctx,0x0B,0x6A);

lcm_dcs_write_seq_static(ctx,0xFF,0x28);
lcm_dcs_write_seq_static(ctx,0x7D,0x1F);
lcm_dcs_write_seq_static(ctx,0x7B,0x40);
lcm_dcs_write_seq_static(ctx,0xFF,0xB3);
lcm_dcs_write_seq_static(ctx,0x29,0x00);
lcm_dcs_write_seq_static(ctx,0x57,0x28);
lcm_dcs_write_seq_static(ctx,0x4D,0x00);
lcm_dcs_write_seq_static(ctx,0x28,0x23);
lcm_dcs_write_seq_static(ctx,0x56,0x88);
lcm_dcs_write_seq_static(ctx,0xFF,0x28);
lcm_dcs_write_seq_static(ctx,0x6A,0x07);
lcm_dcs_write_seq_static(ctx,0xFF,0xB3);
lcm_dcs_write_seq_static(ctx,0xED,0x0E);
lcm_dcs_write_seq_static(ctx,0x47,0x01);
lcm_dcs_write_seq_static(ctx,0xFF,0x22);
lcm_dcs_write_seq_static(ctx,0x0D,0x02);
lcm_dcs_write_seq_static(ctx,0xFF,0xB3);
lcm_dcs_write_seq_static(ctx,0x42,0x05);
lcm_dcs_write_seq_static(ctx,0x44,0x52);

lcm_dcs_write_seq_static(ctx,0xFF,0x22);
lcm_dcs_write_seq_static(ctx,0xE4,0x00);
lcm_dcs_write_seq_static(ctx,0x01,0x06);
lcm_dcs_write_seq_static(ctx,0x02,0x68);
lcm_dcs_write_seq_static(ctx,0x03,0x00);

lcm_dcs_write_seq_static(ctx,0xFF,0x20);
lcm_dcs_write_seq_static(ctx,0xC7,0x00);
lcm_dcs_write_seq_static(ctx,0xC8,0x76);
lcm_dcs_write_seq_static(ctx,0xB7,0x00);
lcm_dcs_write_seq_static(ctx,0xB8,0x25);
lcm_dcs_write_seq_static(ctx,0xB9,0x01);
lcm_dcs_write_seq_static(ctx,0xBA,0x33);
lcm_dcs_write_seq_static(ctx,0xD4,0x18);
lcm_dcs_write_seq_static(ctx,0xFF,0x22);
lcm_dcs_write_seq_static(ctx,0x83,0x07);
lcm_dcs_write_seq_static(ctx,0x8B,0x01);
lcm_dcs_write_seq_static(ctx,0x93,0x09);
lcm_dcs_write_seq_static(ctx,0x9B,0xEF);
lcm_dcs_write_seq_static(ctx,0x9C,0x00);
//GOA ING
lcm_dcs_write_seq_static(ctx,0xFF,0x28);
lcm_dcs_write_seq_static(ctx,0x01,0x1B);
lcm_dcs_write_seq_static(ctx,0x02,0x20);
lcm_dcs_write_seq_static(ctx,0x03,0x21);
lcm_dcs_write_seq_static(ctx,0x04,0x05);
lcm_dcs_write_seq_static(ctx,0x05,0x25);
lcm_dcs_write_seq_static(ctx,0x06,0x1A);
lcm_dcs_write_seq_static(ctx,0x07,0x1A);
lcm_dcs_write_seq_static(ctx,0x08,0x0F);
lcm_dcs_write_seq_static(ctx,0x09,0x0D);
lcm_dcs_write_seq_static(ctx,0x0A,0x0B);
lcm_dcs_write_seq_static(ctx,0x0B,0x09);
lcm_dcs_write_seq_static(ctx,0x0C,0x01);
lcm_dcs_write_seq_static(ctx,0x0D,0x25);
lcm_dcs_write_seq_static(ctx,0x0E,0x25);
lcm_dcs_write_seq_static(ctx,0x0F,0x25);
lcm_dcs_write_seq_static(ctx,0x10,0x25);
lcm_dcs_write_seq_static(ctx,0x11,0x25);
lcm_dcs_write_seq_static(ctx,0x12,0x25);
lcm_dcs_write_seq_static(ctx,0x13,0x25);
lcm_dcs_write_seq_static(ctx,0x14,0x25);
lcm_dcs_write_seq_static(ctx,0x15,0x25);
lcm_dcs_write_seq_static(ctx,0x16,0x25);
lcm_dcs_write_seq_static(ctx,0x17,0x25);
lcm_dcs_write_seq_static(ctx,0x18,0x20);
lcm_dcs_write_seq_static(ctx,0x19,0x21);
lcm_dcs_write_seq_static(ctx,0x1A,0x04);
lcm_dcs_write_seq_static(ctx,0x1B,0x25);
lcm_dcs_write_seq_static(ctx,0x1C,0x1A);
lcm_dcs_write_seq_static(ctx,0x1D,0x1A);
lcm_dcs_write_seq_static(ctx,0x1E,0x0E);
lcm_dcs_write_seq_static(ctx,0x1F,0x0C);
lcm_dcs_write_seq_static(ctx,0x20,0x0A);
lcm_dcs_write_seq_static(ctx,0x21,0x08);
lcm_dcs_write_seq_static(ctx,0x22,0x00);
lcm_dcs_write_seq_static(ctx,0x23,0x25);
lcm_dcs_write_seq_static(ctx,0x24,0x25);
lcm_dcs_write_seq_static(ctx,0x25,0x25);
lcm_dcs_write_seq_static(ctx,0x26,0x25);
lcm_dcs_write_seq_static(ctx,0x27,0x25);
lcm_dcs_write_seq_static(ctx,0x28,0x25);
lcm_dcs_write_seq_static(ctx,0x29,0x25);
lcm_dcs_write_seq_static(ctx,0x2A,0x25);
lcm_dcs_write_seq_static(ctx,0x2B,0x25);
lcm_dcs_write_seq_static(ctx,0x2D,0x25);

lcm_dcs_write_seq_static(ctx,0xFF,0x28);
lcm_dcs_write_seq_static(ctx,0x30,0x00);
lcm_dcs_write_seq_static(ctx,0x31,0x05);
lcm_dcs_write_seq_static(ctx,0x32,0x03);
lcm_dcs_write_seq_static(ctx,0x33,0x05);
lcm_dcs_write_seq_static(ctx,0x34,0x00);
lcm_dcs_write_seq_static(ctx,0x35,0x05);
lcm_dcs_write_seq_static(ctx,0x38,0x00);
lcm_dcs_write_seq_static(ctx,0x39,0x05);
lcm_dcs_write_seq_static(ctx,0x3A,0x03);
lcm_dcs_write_seq_static(ctx,0x3B,0x05);
lcm_dcs_write_seq_static(ctx,0x2F,0x00);

lcm_dcs_write_seq_static(ctx,0xFF,0x26);
lcm_dcs_write_seq_static(ctx,0x50,0x80,0x01);

lcm_dcs_write_seq_static(ctx,0xFF,0x21);
lcm_dcs_write_seq_static(ctx,0x7E,0x03);
lcm_dcs_write_seq_static(ctx,0x7F,0x22);
lcm_dcs_write_seq_static(ctx,0x8B,0x22);
lcm_dcs_write_seq_static(ctx,0x80,0x06);
lcm_dcs_write_seq_static(ctx,0x8C,0x02);
lcm_dcs_write_seq_static(ctx,0xAF,0x72);
lcm_dcs_write_seq_static(ctx,0xB0,0x72);
lcm_dcs_write_seq_static(ctx,0x83,0x03);
lcm_dcs_write_seq_static(ctx,0x8F,0x03);
lcm_dcs_write_seq_static(ctx,0xD2,0x0B);
lcm_dcs_write_seq_static(ctx,0xD3,0x60);
lcm_dcs_write_seq_static(ctx,0xD4,0x0B);
lcm_dcs_write_seq_static(ctx,0xD5,0x60);
lcm_dcs_write_seq_static(ctx,0x87,0x03);
lcm_dcs_write_seq_static(ctx,0x93,0x04);
lcm_dcs_write_seq_static(ctx,0x82,0xA0);
lcm_dcs_write_seq_static(ctx,0x8E,0xA0);
lcm_dcs_write_seq_static(ctx,0x2B,0x00);
lcm_dcs_write_seq_static(ctx,0x2E,0x00);
lcm_dcs_write_seq_static(ctx,0x88,0x80);
lcm_dcs_write_seq_static(ctx,0x89,0x00);
lcm_dcs_write_seq_static(ctx,0x8A,0x00);
lcm_dcs_write_seq_static(ctx,0x94,0x80);
lcm_dcs_write_seq_static(ctx,0x95,0x00);
lcm_dcs_write_seq_static(ctx,0x96,0x00);
lcm_dcs_write_seq_static(ctx,0x45,0x33);
lcm_dcs_write_seq_static(ctx,0x46,0x26);
lcm_dcs_write_seq_static(ctx,0x4C,0x26);
lcm_dcs_write_seq_static(ctx,0x5E,0xA4);
lcm_dcs_write_seq_static(ctx,0x64,0xA4);
lcm_dcs_write_seq_static(ctx,0x47,0x0B);
lcm_dcs_write_seq_static(ctx,0x4D,0x0A);
lcm_dcs_write_seq_static(ctx,0x5F,0x22);
lcm_dcs_write_seq_static(ctx,0x65,0x23);
lcm_dcs_write_seq_static(ctx,0x01,0x10);
lcm_dcs_write_seq_static(ctx,0x02,0x10);
lcm_dcs_write_seq_static(ctx,0x76,0x88);
lcm_dcs_write_seq_static(ctx,0x77,0x88);
lcm_dcs_write_seq_static(ctx,0x7A,0xC4);
lcm_dcs_write_seq_static(ctx,0x7B,0xC4);
lcm_dcs_write_seq_static(ctx,0xE7,0x0B);
lcm_dcs_write_seq_static(ctx,0xE8,0x60);
lcm_dcs_write_seq_static(ctx,0xE9,0x0B);
lcm_dcs_write_seq_static(ctx,0xEA,0x60);
lcm_dcs_write_seq_static(ctx,0xF1,0x60);
lcm_dcs_write_seq_static(ctx,0xF2,0x60);
lcm_dcs_write_seq_static(ctx,0xF3,0x60);
lcm_dcs_write_seq_static(ctx,0xF4,0x60);
lcm_dcs_write_seq_static(ctx,0x82,0x60);
lcm_dcs_write_seq_static(ctx,0x8E,0x60);
lcm_dcs_write_seq_static(ctx,0x46,0x66);
lcm_dcs_write_seq_static(ctx,0x4C,0x66);
lcm_dcs_write_seq_static(ctx,0xC2,0x84);
lcm_dcs_write_seq_static(ctx,0xC3,0x84);
lcm_dcs_write_seq_static(ctx,0xC6,0x4A);
lcm_dcs_write_seq_static(ctx,0xC7,0x8A);
lcm_dcs_write_seq_static(ctx,0x5E,0x64);
lcm_dcs_write_seq_static(ctx,0x64,0x64);
lcm_dcs_write_seq_static(ctx,0x08,0x03);
lcm_dcs_write_seq_static(ctx,0xDC,0x27);
lcm_dcs_write_seq_static(ctx,0xE0,0x80);
lcm_dcs_write_seq_static(ctx,0xE1,0x27);
lcm_dcs_write_seq_static(ctx,0xE2,0x80);
lcm_dcs_write_seq_static(ctx,0xC8,0x4A);
lcm_dcs_write_seq_static(ctx,0xC9,0x46);
lcm_dcs_write_seq_static(ctx,0xC4,0x80);
lcm_dcs_write_seq_static(ctx,0xC5,0x8F);
lcm_dcs_write_seq_static(ctx,0x29,0x00);
lcm_dcs_write_seq_static(ctx,0x18,0x00);
lcm_dcs_write_seq_static(ctx,0xCA,0x03);
lcm_dcs_write_seq_static(ctx,0xCB,0x00);
lcm_dcs_write_seq_static(ctx,0xCD,0x00);
lcm_dcs_write_seq_static(ctx,0xCC,0x20);
lcm_dcs_write_seq_static(ctx,0xCE,0x30);
lcm_dcs_write_seq_static(ctx,0xCC,0x40);
lcm_dcs_write_seq_static(ctx,0xCE,0x50);
lcm_dcs_write_seq_static(ctx,0xCF,0x76);
lcm_dcs_write_seq_static(ctx,0xD0,0x76);
lcm_dcs_write_seq_static(ctx,0xFF,0x22);
lcm_dcs_write_seq_static(ctx,0x05,0x00);
lcm_dcs_write_seq_static(ctx,0x08,0x22);

lcm_dcs_write_seq_static(ctx,0xFF,0x20);
lcm_dcs_write_seq_static(ctx,0x24,0x99);
lcm_dcs_write_seq_static(ctx,0x25,0x16);
//ÂÌÇø
lcm_dcs_write_seq_static(ctx,0xFF,0x28);
lcm_dcs_write_seq_static(ctx,0x3D,0x31);
lcm_dcs_write_seq_static(ctx,0x3E,0x31);
lcm_dcs_write_seq_static(ctx,0x3F,0x43);
lcm_dcs_write_seq_static(ctx,0x40,0x43);
lcm_dcs_write_seq_static(ctx,0x45,0x30);
lcm_dcs_write_seq_static(ctx,0x46,0x30);
lcm_dcs_write_seq_static(ctx,0x47,0x40);
lcm_dcs_write_seq_static(ctx,0x48,0x40);
lcm_dcs_write_seq_static(ctx,0x4D,0xA1);
lcm_dcs_write_seq_static(ctx,0x50,0xAA);
lcm_dcs_write_seq_static(ctx,0x52,0x72);
lcm_dcs_write_seq_static(ctx,0x53,0x22);
lcm_dcs_write_seq_static(ctx,0x6A,0x07);
lcm_dcs_write_seq_static(ctx,0x56,0x12);
lcm_dcs_write_seq_static(ctx,0x57,0x20);
lcm_dcs_write_seq_static(ctx,0x5A,0x89);
lcm_dcs_write_seq_static(ctx,0x5B,0x9A);
lcm_dcs_write_seq_static(ctx,0x5C,0x89);
lcm_dcs_write_seq_static(ctx,0x5D,0x9A);
lcm_dcs_write_seq_static(ctx,0x5E,0x89);
lcm_dcs_write_seq_static(ctx,0x5F,0x9A);
lcm_dcs_write_seq_static(ctx,0x62,0x40);
lcm_dcs_write_seq_static(ctx,0x63,0x40);
lcm_dcs_write_seq_static(ctx,0x64,0x40);
lcm_dcs_write_seq_static(ctx,0x65,0x40);
lcm_dcs_write_seq_static(ctx,0x66,0x40);
lcm_dcs_write_seq_static(ctx,0x67,0x40);

lcm_dcs_write_seq_static(ctx,0xFF,0x28);
lcm_dcs_write_seq_static(ctx,0x77,0x55);
lcm_dcs_write_seq_static(ctx,0x79,0x05);

lcm_dcs_write_seq_static(ctx,0xFF,0x20);
lcm_dcs_write_seq_static(ctx,0x7E,0x33);
lcm_dcs_write_seq_static(ctx,0x70,0x32);
lcm_dcs_write_seq_static(ctx,0x71,0x00);
lcm_dcs_write_seq_static(ctx,0x72,0x01);
lcm_dcs_write_seq_static(ctx,0x73,0x00);
lcm_dcs_write_seq_static(ctx,0x74,0x00);
lcm_dcs_write_seq_static(ctx,0x75,0x64);
lcm_dcs_write_seq_static(ctx,0x77,0x2E);
lcm_dcs_write_seq_static(ctx,0x78,0x59);
lcm_dcs_write_seq_static(ctx,0x7C,0x0A);

lcm_dcs_write_seq_static(ctx,0xFF,0x25);
lcm_dcs_write_seq_static(ctx,0xFB,0x00);
lcm_dcs_write_seq_static(ctx,0x75,0x32);
lcm_dcs_write_seq_static(ctx,0x85,0x32);
lcm_dcs_write_seq_static(ctx,0x86,0x00);
lcm_dcs_write_seq_static(ctx,0x87,0x01);
lcm_dcs_write_seq_static(ctx,0x8A,0x64);
lcm_dcs_write_seq_static(ctx,0x8C,0x16);
lcm_dcs_write_seq_static(ctx,0x8D,0x8F);
lcm_dcs_write_seq_static(ctx,0x91,0x0A);

lcm_dcs_write_seq_static(ctx,0xFF,0x23);
lcm_dcs_write_seq_static(ctx,0x29,0x03);
lcm_dcs_write_seq_static(ctx,0x01,0x00,0x00,0x00,0x01,0x00,0x09,0x00,0x1C,0x00,0x2F,0x00,0x3C,0x00,0x4D,0x00,0x57);
lcm_dcs_write_seq_static(ctx,0x02,0x00,0x63,0x00,0x90,0x00,0xB3,0x00,0xE6,0x01,0x10,0x01,0x56,0x01,0x92,0x01,0x94);
lcm_dcs_write_seq_static(ctx,0x03,0x01,0xD4,0x02,0x1F,0x02,0x4E,0x02,0x8C,0x02,0xB5,0x02,0xE8,0x02,0xF9,0x03,0x0B);
lcm_dcs_write_seq_static(ctx,0x04,0x03,0x21,0x03,0x3B,0x03,0x5B,0x03,0x84,0x03,0xBA,0x03,0xFF);
lcm_dcs_write_seq_static(ctx,0x0D,0x00,0x00,0x00,0x01,0x00,0x09,0x00,0x1C,0x00,0x2F,0x00,0x3C,0x00,0x4D,0x00,0x57);
lcm_dcs_write_seq_static(ctx,0x0E,0x00,0x63,0x00,0x90,0x00,0xB3,0x00,0xE6,0x01,0x10,0x01,0x56,0x01,0x92,0x01,0x94);
lcm_dcs_write_seq_static(ctx,0x0F,0x01,0xD4,0x02,0x1F,0x02,0x4E,0x02,0x8C,0x02,0xB5,0x02,0xE8,0x02,0xF9,0x03,0x0B);
lcm_dcs_write_seq_static(ctx,0x10,0x03,0x21,0x03,0x3B,0x03,0x5B,0x03,0x84,0x03,0xBA,0x03,0xFF);
lcm_dcs_write_seq_static(ctx,0xFF,0xB3);
lcm_dcs_write_seq_static(ctx,0x3F,0x77);
lcm_dcs_write_seq_static(ctx,0xFF,0x20);
lcm_dcs_write_seq_static(ctx,0x2D,0x11);
lcm_dcs_write_seq_static(ctx,0xA3,0x1E);
lcm_dcs_write_seq_static(ctx,0xA7,0x1E);
lcm_dcs_write_seq_static(ctx,0xFF,0xB3);
lcm_dcs_write_seq_static(ctx,0x4E,0x46);
lcm_dcs_write_seq_static(ctx,0x29,0x80);
lcm_dcs_write_seq_static(ctx,0xF0,0x01);
lcm_dcs_write_seq_static(ctx,0x2D,0x65);
lcm_dcs_write_seq_static(ctx,0x2E,0x00);
lcm_dcs_write_seq_static(ctx,0x32,0x01);
lcm_dcs_write_seq_static(ctx,0x33,0x08);



lcm_dcs_write_seq_static(ctx,0xFF,0x10);
lcm_dcs_write_seq_static(ctx,0x35,0x00);
lcm_dcs_write_seq_static(ctx,0x51,0x00,0x00);
lcm_dcs_write_seq_static(ctx,0x53,0x2c);
lcm_dcs_write_seq_static(ctx,0x55,0x00);
lcm_dcs_write_seq_static(ctx,0x36,0x08);
lcm_dcs_write_seq_static(ctx,0x69,0x01);
lcm_dcs_write_seq_static(ctx,0xBA,0x03);

lcm_dcs_write_seq_static(ctx,0x11,0x00);
msleep(120);
lcm_dcs_write_seq_static(ctx,0x29,0x00);
msleep(20);
	// gpiod_set_value(ctx->tpreset_gpio, 1);
	// devm_gpiod_put(ctx->dev, ctx->tpreset_gpio);

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
printk("dsy  %s %d lcm_gc7272_shutdown_status %d st_drm_shutdown_status %d\n", __func__,__LINE__,lcm_gc7272_shutdown_status,st_drm_shutdown_status);
	if (!ctx->prepared)
		return 0;

if(lcm_gc7272_shutdown_status == 1 || st_drm_shutdown_status == 1 )
{
//st_mtk_dsi_clk_hs_mode(0);

 lcm_dcs_write_seq_static(ctx,0xFF,0x55,0xAA,0x66);
lcm_dcs_write_seq_static(ctx,0xFF,0x28);
lcm_dcs_write_seq_static(ctx,0x2F,0x00);
lcm_dcs_write_seq_static(ctx,0xFF,0x10);
lcm_dcs_write_seq_static(ctx,0x28,0x00);
msleep(50);
lcm_dcs_write_seq_static(ctx,0xFF,0xB3);
lcm_dcs_write_seq_static(ctx,0xCE,0x9C);
lcm_dcs_write_seq_static(ctx,0xFF,0x10);
lcm_dcs_write_seq_static(ctx,0x10,0x00);
msleep(120);
lcm_dcs_write_seq_static(ctx,0xFF,0xB3);
lcm_dcs_write_seq_static(ctx,0xCE,0x14);
msleep(120);
lcm_dcs_write_seq_static(ctx,0x4F,0x01);
msleep(10);	


	

	ctx->error = 0;
	ctx->prepared = false;
// #if defined(CONFIG_RT5081_PMU_DSV) || defined(CONFIG_MT6370_PMU_DSV)
// 	lcm_panel_bias_disable();
// #else
	// ctx->tp_reset_gpio =
	// 	devm_gpiod_get(ctx->dev, "tp_reset", GPIOD_OUT_HIGH);
	// if (IS_ERR(ctx->tp_reset_gpio)) {
	// 	dev_err(ctx->dev, "%s: cannot get tp_reset_gpio %ld\n",
	// 		__func__, PTR_ERR(ctx->tp_reset_gpio));
	// 	return PTR_ERR(ctx->tp_reset_gpio);
	// }
	// gpiod_set_value(ctx->tp_reset_gpio, 0);
	// devm_gpiod_put(ctx->dev, ctx->tp_reset_gpio);

	// ctx->reset_gpio =
	// 	devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	// if (IS_ERR(ctx->reset_gpio)) {
	// 	dev_err(ctx->dev, "%s: cannot get reset_gpio %ld\n",
	// 		__func__, PTR_ERR(ctx->reset_gpio));
	// 	return PTR_ERR(ctx->reset_gpio);
	// }
	// gpiod_set_value(ctx->reset_gpio, 0);
	// devm_gpiod_put(ctx->dev, ctx->reset_gpio);
	// //msleep(5);


	// msleep(5);


	// ctx->lcm_3v3_enable_gpio = devm_gpiod_get(ctx->dev, "lcm_3v3_enable", GPIOD_OUT_HIGH);
	// if (IS_ERR(ctx->lcm_3v3_enable_gpio)) {
	// 	dev_err(ctx->dev, "%s: cannot get lcm_3v3_enable-gpios %ld\n",
	// 		__func__, PTR_ERR(ctx->lcm_3v3_enable_gpio));
	// 	return PTR_ERR(ctx->lcm_3v3_enable_gpio);
	// }
	// gpiod_set_value(ctx->lcm_3v3_enable_gpio, 0);
	// devm_gpiod_put(ctx->dev, ctx->lcm_3v3_enable_gpio);
	// msleep(5);



	// ctx->avee_enable_gpio = devm_gpiod_get(ctx->dev, "avee_enable", GPIOD_OUT_HIGH);
	// if (IS_ERR(ctx->avee_enable_gpio)) {
	// 	dev_err(ctx->dev, "%s: cannot get avee_enable-gpios %ld\n",
	// 		__func__, PTR_ERR(ctx->avee_enable_gpio));
	// 	return PTR_ERR(ctx->avee_enable_gpio);
	// }
	// gpiod_set_value(ctx->avee_enable_gpio, 0);
	// devm_gpiod_put(ctx->dev, ctx->avee_enable_gpio);
	// msleep(5);

	// ctx->avdd_enable_gpio = devm_gpiod_get(ctx->dev, "avdd_enable", GPIOD_OUT_HIGH);
	// if (IS_ERR(ctx->avdd_enable_gpio)) {
	// 	dev_err(ctx->dev, "%s: cannot get avdd_enable-gpios %ld\n",
	// 		__func__, PTR_ERR(ctx->avdd_enable_gpio));
	// 	return PTR_ERR(ctx->avdd_enable_gpio);
	// }
	// gpiod_set_value(ctx->avdd_enable_gpio, 0);
	// devm_gpiod_put(ctx->dev, ctx->avdd_enable_gpio);
	// msleep(5);



	// // msleep(5);
	// ctx->lcm_1v8_enable_gpio = devm_gpiod_get(ctx->dev, "lcm_1v8_enable", GPIOD_OUT_HIGH);
	// if (IS_ERR(ctx->lcm_1v8_enable_gpio)) {
	// 	dev_err(ctx->dev, "%s: cannot get lcm_1v8_enable-gpios %ld\n",
	// 		__func__, PTR_ERR(ctx->lcm_1v8_enable_gpio));
	// 	return PTR_ERR(ctx->lcm_1v8_enable_gpio);
	// }
	// gpiod_set_value(ctx->lcm_1v8_enable_gpio, 0);
	// devm_gpiod_put(ctx->dev, ctx->lcm_1v8_enable_gpio);



	//queue_delayed_work(st_bc12_check_workqueue, &st_bc12_check_work, 500);


	lcm_gc7272_shutdown_status = 0;	
	st_drm_shutdown_status = 0;
	// ctx->dsi_te_enable_gpio = devm_gpiod_get(ctx->dev, "dsi_te_enable", GPIOD_OUT_HIGH);
	// if (IS_ERR(ctx->dsi_te_enable_gpio)) {
	// 	dev_err(ctx->dev, "%s: cannot get dsi_te_enable-gpios %ld\n",
	// 		__func__, PTR_ERR(ctx->dsi_te_enable_gpio));
	// 	return PTR_ERR(ctx->dsi_te_enable_gpio);
	// }
	// gpiod_set_value(ctx->dsi_te_enable_gpio, 0);
	// devm_gpiod_put(ctx->dev, ctx->dsi_te_enable_gpio);
	// msleep(5);


printk("dsy  %s %d lcm_gc7272_shutdown_status %d st_drm_shutdown_status %d\n", __func__,__LINE__,lcm_gc7272_shutdown_status,st_drm_shutdown_status);
}
else
{
lcm_dcs_write_seq_static(ctx,0xFF,0x55,0xAA,0x66);
lcm_dcs_write_seq_static(ctx,0xFF,0x28);
lcm_dcs_write_seq_static(ctx,0x2F,0x00);
lcm_dcs_write_seq_static(ctx,0xFF,0x10);
lcm_dcs_write_seq_static(ctx,0x28,0x00);
msleep(50);
lcm_dcs_write_seq_static(ctx,0xFF,0xB3);
lcm_dcs_write_seq_static(ctx,0xCE,0x9C);
lcm_dcs_write_seq_static(ctx,0xFF,0x10);
lcm_dcs_write_seq_static(ctx,0x10,0x00);
msleep(120);
lcm_dcs_write_seq_static(ctx,0xFF,0xB3);
lcm_dcs_write_seq_static(ctx,0xCE,0x14);
msleep(120);
lcm_dcs_write_seq_static(ctx,0x4F,0x01);
msleep(10);	

	
	

	ctx->error = 0;
	ctx->prepared = false;
// #if defined(CONFIG_RT5081_PMU_DSV) || defined(CONFIG_MT6370_PMU_DSV)
// 	lcm_panel_bias_disable();
// #else
	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(ctx->dev, "%s: cannot get reset_gpio %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	gpiod_set_value(ctx->reset_gpio, 1);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);
	msleep(5);

	// ctx->lcm_3v3_enable_gpio = devm_gpiod_get(ctx->dev, "lcm_3v3_enable", GPIOD_OUT_HIGH);
	// if (IS_ERR(ctx->lcm_3v3_enable_gpio)) {
	// 	dev_err(ctx->dev, "%s: cannot get lcm_3v3_enable-gpios %ld\n",
	// 		__func__, PTR_ERR(ctx->lcm_3v3_enable_gpio));
	// 	return PTR_ERR(ctx->lcm_3v3_enable_gpio);
	// }
	// gpiod_set_value(ctx->lcm_3v3_enable_gpio, 0);
	// devm_gpiod_put(ctx->dev, ctx->lcm_3v3_enable_gpio);
	// msleep(5);


	// ctx->lcm_1v8_enable_gpio = devm_gpiod_get(ctx->dev, "lcm_1v8_enable", GPIOD_OUT_HIGH);
	// if (IS_ERR(ctx->lcm_1v8_enable_gpio)) {
	// 	dev_err(ctx->dev, "%s: cannot get lcm_1v8_enable-gpios %ld\n",
	// 		__func__, PTR_ERR(ctx->lcm_1v8_enable_gpio));
	// 	return PTR_ERR(ctx->lcm_1v8_enable_gpio);
	// }
	// gpiod_set_value(ctx->lcm_1v8_enable_gpio, 0);
	// devm_gpiod_put(ctx->dev, ctx->lcm_1v8_enable_gpio);
	// msleep(5);


	ctx->avdd_enable_gpio = devm_gpiod_get(ctx->dev, "avdd_enable", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->avdd_enable_gpio)) {
		dev_err(ctx->dev, "%s: cannot get avdd_enable-gpios %ld\n",
			__func__, PTR_ERR(ctx->avdd_enable_gpio));
		return PTR_ERR(ctx->avdd_enable_gpio);
	}
	gpiod_set_value(ctx->avdd_enable_gpio, 0);
	devm_gpiod_put(ctx->dev, ctx->avdd_enable_gpio);
	//msleep(5);


	ctx->avee_enable_gpio = devm_gpiod_get(ctx->dev, "avee_enable", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->avee_enable_gpio)) {
		dev_err(ctx->dev, "%s: cannot get avee_enable-gpios %ld\n",
			__func__, PTR_ERR(ctx->avee_enable_gpio));
		return PTR_ERR(ctx->avee_enable_gpio);
	}
	gpiod_set_value(ctx->avee_enable_gpio, 0);
	devm_gpiod_put(ctx->dev, ctx->avee_enable_gpio);
	msleep(5);

	// ctx->dsi_te_enable_gpio = devm_gpiod_get(ctx->dev, "dsi_te_enable", GPIOD_OUT_HIGH);
	// if (IS_ERR(ctx->dsi_te_enable_gpio)) {
	// 	dev_err(ctx->dev, "%s: cannot get dsi_te_enable-gpios %ld\n",
	// 		__func__, PTR_ERR(ctx->dsi_te_enable_gpio));
	// 	return PTR_ERR(ctx->dsi_te_enable_gpio);
	// }
	// gpiod_set_value(ctx->dsi_te_enable_gpio, 0);
	// devm_gpiod_put(ctx->dev, ctx->dsi_te_enable_gpio);
	// msleep(5);
	lcm_gc7272_shutdown_status = 0;		

printk("dsy  %s %d lcm_gc7272_shutdown_status %d st_drm_shutdown_status %d\n", __func__,__LINE__,lcm_gc7272_shutdown_status,st_drm_shutdown_status);
}



	// ctx->bl_enable_gpio = devm_gpiod_get(ctx->dev, "bl_enable", GPIOD_OUT_HIGH);
	// if (IS_ERR(ctx->bl_enable_gpio)) {
	// 	dev_err(ctx->dev, "%s: cannot get bl_enable-gpios %ld\n",
	// 		__func__, PTR_ERR(ctx->bl_enable_gpio));
	// 	return PTR_ERR(ctx->bl_enable_gpio);
	// }
	// gpiod_set_value(ctx->bl_enable_gpio, 0);
	// devm_gpiod_put(ctx->dev, ctx->bl_enable_gpio);
	// msleep(5);

	// if (ctx->gate_ic == 0) {
	// 	ctx->bias_neg = devm_gpiod_get_index(ctx->dev,
	// 		"bias", 1, GPIOD_OUT_HIGH);
	// 	if (IS_ERR(ctx->bias_neg)) {
	// 		dev_err(ctx->dev, "%s: cannot get bias_neg %ld\n",
	// 			__func__, PTR_ERR(ctx->bias_neg));
	// 		return PTR_ERR(ctx->bias_neg);
	// 	}
	// 	gpiod_set_value(ctx->bias_neg, 0);
	// 	devm_gpiod_put(ctx->dev, ctx->bias_neg);

	// 	udelay(1000);

	// 	ctx->bias_pos = devm_gpiod_get_index(ctx->dev,
	// 		"bias", 0, GPIOD_OUT_HIGH);
	// 	if (IS_ERR(ctx->bias_pos)) {
	// 		dev_err(ctx->dev, "%s: cannot get bias_pos %ld\n",
	// 			__func__, PTR_ERR(ctx->bias_pos));
	// 		return PTR_ERR(ctx->bias_pos);
	// 	}
	// 	gpiod_set_value(ctx->bias_pos, 0);
	// 	devm_gpiod_put(ctx->dev, ctx->bias_pos);
	// }
// #endif

	return 0;
}

static int lcm_prepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	int ret;
printk("dsy  %s %d \n", __func__,__LINE__);
	pr_info("%s\n", __func__);
	if (ctx->prepared)
		return 0;

// #if defined(CONFIG_RT5081_PMU_DSV) || defined(CONFIG_MT6370_PMU_DSV)
// 	lcm_panel_bias_enable();
// #else
	// if (ctx->gate_ic == 0) {

	// 	ctx->bias_pos = devm_gpiod_get_index(ctx->dev,
	// 		"bias", 0, GPIOD_OUT_HIGH);
	// 	if (IS_ERR(ctx->bias_pos)) {
	// 		dev_err(ctx->dev, "%s: cannot get bias_pos %ld\n",
	// 			__func__, PTR_ERR(ctx->bias_pos));
	// 		return PTR_ERR(ctx->bias_pos);
	// 	}
	// 	gpiod_set_value(ctx->bias_pos, 1);
	// 	devm_gpiod_put(ctx->dev, ctx->bias_pos);

	// 	udelay(2000);

	// 	ctx->bias_neg = devm_gpiod_get_index(ctx->dev,
	// 		"bias", 1, GPIOD_OUT_HIGH);
	// 	if (IS_ERR(ctx->bias_neg)) {
	// 		dev_err(ctx->dev, "%s: cannot get bias_neg %ld\n",
	// 			__func__, PTR_ERR(ctx->bias_neg));
	// 		return PTR_ERR(ctx->bias_neg);
	// 	}
	// 	gpiod_set_value(ctx->bias_neg, 1);
	// 	devm_gpiod_put(ctx->dev, ctx->bias_neg);
	// }
// #endif
	// ctx->dsi_te_enable_gpio = devm_gpiod_get(ctx->dev, "dsi_te_enable", GPIOD_OUT_HIGH);
	// if (IS_ERR(ctx->dsi_te_enable_gpio)) {
	// 	dev_err(ctx->dev, "%s: cannot get dsi_te_enable-gpios %ld\n",
	// 		__func__, PTR_ERR(ctx->dsi_te_enable_gpio));
	// 	return PTR_ERR(ctx->dsi_te_enable_gpio);
	// }
	// gpiod_set_value(ctx->dsi_te_enable_gpio, 1);
	// devm_gpiod_put(ctx->dev, ctx->dsi_te_enable_gpio);
	// msleep(5);

	// ctx->lcm_3v3_enable_gpio = devm_gpiod_get(ctx->dev, "lcm_3v3_enable", GPIOD_OUT_HIGH);
	// if (IS_ERR(ctx->lcm_3v3_enable_gpio)) {
	// 	dev_err(ctx->dev, "%s: cannot get lcm_3v3_enable-gpios %ld\n",
	// 		__func__, PTR_ERR(ctx->lcm_3v3_enable_gpio));
	// 	return PTR_ERR(ctx->lcm_3v3_enable_gpio);
	// }
	// gpiod_set_value(ctx->lcm_3v3_enable_gpio, 1);
	// devm_gpiod_put(ctx->dev, ctx->lcm_3v3_enable_gpio);
	// msleep(5);


	// ctx->lcm_1v8_enable_gpio = devm_gpiod_get(ctx->dev, "lcm_1v8_enable", GPIOD_OUT_HIGH);
	// if (IS_ERR(ctx->lcm_1v8_enable_gpio)) {
	// 	dev_err(ctx->dev, "%s: cannot get lcm_1v8_enable-gpios %ld\n",
	// 		__func__, PTR_ERR(ctx->lcm_1v8_enable_gpio));
	// 	return PTR_ERR(ctx->lcm_1v8_enable_gpio);
	// }
	// gpiod_set_value(ctx->lcm_1v8_enable_gpio, 1);
	// devm_gpiod_put(ctx->dev, ctx->lcm_1v8_enable_gpio);
	// msleep(5);


	ctx->avdd_enable_gpio = devm_gpiod_get(ctx->dev, "avdd_enable", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->avdd_enable_gpio)) {
		dev_err(ctx->dev, "%s: cannot get avdd_enable-gpios %ld\n",
			__func__, PTR_ERR(ctx->avdd_enable_gpio));
		return PTR_ERR(ctx->avdd_enable_gpio);
	}
	gpiod_set_value(ctx->avdd_enable_gpio, 1);
	devm_gpiod_put(ctx->dev, ctx->avdd_enable_gpio);
	//msleep(5);


	ctx->avee_enable_gpio = devm_gpiod_get(ctx->dev, "avee_enable", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->avee_enable_gpio)) {
		dev_err(ctx->dev, "%s: cannot get avee_enable-gpios %ld\n",
			__func__, PTR_ERR(ctx->avee_enable_gpio));
		return PTR_ERR(ctx->avee_enable_gpio);
	}
	gpiod_set_value(ctx->avee_enable_gpio, 1);
	devm_gpiod_put(ctx->dev, ctx->avee_enable_gpio);


	ocp2131_set_avdd_voltage(OCP2131_AVDD_6000V);
	ocp2131_set_avee_voltage(OCP2131_AVEE_6000V);
	msleep(5);

	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(ctx->dev, "%s: cannot get reset_gpio %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	
	gpiod_set_value(ctx->reset_gpio, 1);
	//devm_gpiod_put(ctx->dev, ctx->reset_gpio);
	msleep(5);
	gpiod_set_value(ctx->reset_gpio, 0);
	//devm_gpiod_put(ctx->dev, ctx->reset_gpio);
	msleep(5);
	gpiod_set_value(ctx->reset_gpio, 1);
	//devm_gpiod_put(ctx->dev, ctx->reset_gpio);
	msleep(5);
	gpiod_set_value(ctx->reset_gpio, 0);
	//devm_gpiod_put(ctx->dev, ctx->reset_gpio);
	msleep(5);
	gpiod_set_value(ctx->reset_gpio, 1);
	//devm_gpiod_put(ctx->dev, ctx->reset_gpio);
	msleep(5);
	gpiod_set_value(ctx->reset_gpio, 0);
	//devm_gpiod_put(ctx->dev, ctx->reset_gpio);
	msleep(5);
	gpiod_set_value(ctx->reset_gpio, 1);
	//devm_gpiod_put(ctx->dev, ctx->reset_gpio);
	msleep(10);
	gpiod_set_value(ctx->reset_gpio, 0);
	//devm_gpiod_put(ctx->dev, ctx->reset_gpio);
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

//#define HFP (88)
//#define HSA (8)
//#define HBP (44)
//#define VFP (1300)
//#define VSA (6)
//#define VBP (30)
//#define VAC (1640)
//#define HAC (720)

static const struct drm_display_mode default_mode = {
	.clock = 176534,//(HAC + HFP + HSA + HBP) * (VAC + VFP + VSA + VBP)*60/1000, //151110,//htotal*vtotal*vrefresh/1000
	.hdisplay = 720,//HAC,
	.hsync_start = 720 + 6,//HFP
	.hsync_end = 720 + 6 + 4,//HSA
	.htotal = 720 + 6 + 4 + 10,//HBP 740
	.vdisplay = 1640,//VAC,
	.vsync_start = 1640 + 2300,//VFP
	.vsync_end = 1640 + 2300 + 6,//VSA
	.vtotal = 1640 + 2300 + 6 + 30,//VBP 3976
	//.vrefresh = 60,
};
static const struct drm_display_mode performance_mode_1 = {	
	.clock = 176756,//(HAC + HFP + HSA + HBP) * (VAC + VFP + VSA + VBP)*60/1000, //151110,//htotal*vtotal*vrefresh/1000
	.hdisplay = 720,//HAC,
	.hsync_start = 720 + 6,//HFP
	.hsync_end = 720 + 6 + 4,//HSA
	.htotal = 720 + 6 + 4 + 10,//HBP 740
	.vdisplay = 1640,//VAC,
	.vsync_start = 1640 + 978,//VFP
	.vsync_end = 1640 + 978 + 6,//VSA
	.vtotal = 1640 + 978 + 6 + 30,//VBP 2,654
	//.vrefresh = 90,
};
static const struct drm_display_mode performance_mode_2 = {	
	.clock = 176179,//(HAC + HFP + HSA + HBP) * (VAC + VFP + VSA + VBP)*60/1000, //151110,//htotal*vtotal*vrefresh/1000
	.hdisplay = 720,//HAC,
	.hsync_start = 720 + 6,//HFP
	.hsync_end = 720 + 6 + 4,//HSA
	.htotal = 720 + 6 + 4 + 10,//HBP 740
	.vdisplay = 1640,//VAC,
	.vsync_start = 1640 + 308,//VFP
	.vsync_end = 1640 + 308 + 6,//VSA
	.vtotal = 1640 + 308 + 6 + 30,//VBP 1,984
	//.vrefresh = 120,
};

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
	unsigned char data[3] = {0x00, 0x00, 0x00};
	unsigned char id[3] = {0x00, 0x00, 0x00};
	ssize_t ret;

	ret = mipi_dsi_dcs_read(dsi, 0x4, data, 3);
	return 1;
	if (ret < 0) {
		pr_err("%s error\n", __func__);
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

static struct mtk_panel_params ext_params = {
	.pll_clk = 608,
	//.data_rate = 1050,
	//.vfp_low_power = 750,
	.ssc_enable = 0,
	.cust_esd_check = 0,
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x9c,
	},
	.data_rate = 1216, //pll_clk*2/3
	.dyn_fps = {
		.switch_en = 1, 
		.vact_timing_fps = 60,
	},	
};
static struct mtk_panel_params ext_params_mode_1 = {
	.pll_clk = 608,
	//.data_rate = 1050,
	//.vfp_low_power = 750,
	.ssc_enable = 0,
	.cust_esd_check = 0,
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x9c,
	},
	.data_rate = 1216, //pll_clk*2/3
	.dyn_fps = {
		.switch_en = 1, 
		.vact_timing_fps = 90,
	},	
};
static struct mtk_panel_params ext_params_mode_2 = {
	.pll_clk = 608,
	//.data_rate = 1050,
	//.vfp_low_power = 750,
	.ssc_enable = 0,
	.cust_esd_check = 0,
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x9c,
	},
	.data_rate = 1216, //pll_clk*2/3
	.dyn_fps = {
		.switch_en = 1, 
		.vact_timing_fps = 120,
	},	
};
struct drm_display_mode *get_mode_by_id_hfp(struct drm_connector *connector,
        unsigned int mode)
{
        struct drm_display_mode *m;
        unsigned int i = 0;

        list_for_each_entry(m, &connector->modes, head) {
                if (i == mode)
                        return m;
                i++;
        }
        return NULL;
}
static int mtk_panel_ext_param_set(struct drm_panel *panel,
                        struct drm_connector *connector, unsigned int mode)
{
        struct mtk_panel_ext *ext = find_panel_ext(panel);
        int ret = 0;
        struct drm_display_mode *m = get_mode_by_id_hfp(connector, mode);

        if (!m) {
                pr_err("%s:%d invalid display_mode\n", __func__, __LINE__);
                return ret;
        }

        if (drm_mode_vrefresh(m) == 60)
                ext->params = &ext_params;
        else if (drm_mode_vrefresh(m) == 90)
                ext->params = &ext_params_mode_1;
        else if (drm_mode_vrefresh(m) == 120)
                ext->params = &ext_params_mode_2;
        else
                ret = 1;

        if (!ret)
                current_fps = drm_mode_vrefresh(m);

        return ret;
}
static struct mtk_panel_funcs ext_funcs = {
	.reset = panel_ext_reset,
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
	.ata_check = panel_ata_check,
	.ext_param_set = mtk_panel_ext_param_set,
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
	struct drm_display_mode *mode_1;
	struct drm_display_mode *mode_2;
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

	mode_1 = drm_mode_duplicate(connector->dev, &performance_mode_1);
	if (!mode_1) {
		dev_info(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			performance_mode_1.hdisplay,
			performance_mode_1.vdisplay,
			drm_mode_vrefresh(&performance_mode_1));
		return -ENOMEM;
	}
	drm_mode_set_name(mode_1);
	mode_1->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode_1);


	mode_2 = drm_mode_duplicate(connector->dev, &performance_mode_2);
	if (!mode_2) {
		dev_info(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			performance_mode_2.hdisplay,
			performance_mode_2.vdisplay,
			drm_mode_vrefresh(&performance_mode_2));
		return -ENOMEM;
	}
	drm_mode_set_name(mode_2);
	mode_2->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode_2);

	connector->display_info.width_mm = 56;
	connector->display_info.height_mm = 163;

	return 1;
}

static const struct drm_panel_funcs lcm_drm_funcs = {
	.disable = lcm_disable,
	.unprepare = lcm_unprepare,
	.prepare = lcm_prepare,
	.enable = lcm_enable,
	.get_modes = lcm_get_modes,
};
// static void st_bc12_check_switch(struct work_struct *work)
//  {
//  	printk("dsy  %s %d lcm_gc7272_shutdown_status %d st_drm_shutdown_status %d\n", __func__,__LINE__,lcm_gc7272_shutdown_status,st_drm_shutdown_status);
// 	st_ctx->lcm_1v8_enable_gpio = devm_gpiod_get(st_ctx->dev, "lcm_1v8_enable", GPIOD_OUT_HIGH);
// 	if (IS_ERR(st_ctx->lcm_1v8_enable_gpio)) {
// 		dev_err(st_ctx->dev, "%s: cannot get lcm_1v8_enable-gpios %ld\n",
// 			__func__, PTR_ERR(st_ctx->lcm_1v8_enable_gpio));
// 		return;
// 	}
// 	gpiod_set_value(st_ctx->lcm_1v8_enable_gpio, 0);
// 	devm_gpiod_put(st_ctx->dev, st_ctx->lcm_1v8_enable_gpio);

//  }
static int lcm_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct device_node *dsi_node, *remote_node = NULL, *endpoint = NULL;
	struct lcm *ctx;
	struct device_node *backlight;
	unsigned int value;
	int ret;

	pr_info("%s+gc7272\n", __func__);



	dsi_node = of_get_parent(dev->of_node);
	if (dsi_node) {
		endpoint = of_graph_get_next_endpoint(dsi_node, NULL);
		if (endpoint) {
			remote_node = of_graph_get_remote_port_parent(endpoint);
			if (!remote_node) {
				pr_info("No panel connected,skip probe lcm\n");
				return -ENODEV;
			}
			pr_info("gc7272-lcm device node name:%s\n", remote_node->name);
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


	// ctx->lcm_3v3_enable_gpio = devm_gpiod_get(dev, "lcm_3v3_enable", GPIOD_OUT_HIGH);
	// if (IS_ERR(ctx->lcm_3v3_enable_gpio)) {
	// 	dev_err(dev, "%s: cannot get lcm_3v3_enable-gpios %ld\n",
	// 		__func__, PTR_ERR(ctx->lcm_3v3_enable_gpio));
	// 	return PTR_ERR(ctx->lcm_3v3_enable_gpio);
	// }
	// devm_gpiod_put(dev, ctx->lcm_3v3_enable_gpio);


	ctx->avdd_enable_gpio = devm_gpiod_get(dev, "avdd_enable", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->avdd_enable_gpio)) {
		dev_err(dev, "%s: cannot get avdd_enable-gpios %ld\n",
			__func__, PTR_ERR(ctx->avdd_enable_gpio));
		return PTR_ERR(ctx->avdd_enable_gpio);
	}
	devm_gpiod_put(dev, ctx->avdd_enable_gpio);

	ctx->avee_enable_gpio = devm_gpiod_get(dev, "avee_enable", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->avee_enable_gpio)) {
		dev_err(dev, "%s: cannot get avee_enable-gpios %ld\n",
			__func__, PTR_ERR(ctx->avee_enable_gpio));
		return PTR_ERR(ctx->avee_enable_gpio);
	}
	devm_gpiod_put(dev, ctx->avee_enable_gpio);


	// ctx->dsi_te_enable_gpio = devm_gpiod_get(dev, "dsi_te_enable", GPIOD_OUT_HIGH);
	// if (IS_ERR(ctx->dsi_te_enable_gpio)) {
	// 	dev_err(dev, "%s: cannot get dsi_te_enable-gpios %ld\n",
	// 		__func__, PTR_ERR(ctx->dsi_te_enable_gpio));
	// 	return PTR_ERR(ctx->dsi_te_enable_gpio);
	// }
	// devm_gpiod_put(dev, ctx->dsi_te_enable_gpio);

	// ctx->lcm_1v8_enable_gpio = devm_gpiod_get(ctx->dev, "lcm_1v8_enable", GPIOD_OUT_HIGH);
	// if (IS_ERR(ctx->lcm_1v8_enable_gpio)) {
	// 	dev_err(ctx->dev, "%s: cannot get lcm_1v8_enable-gpios %ld\n",
	// 		__func__, PTR_ERR(ctx->lcm_1v8_enable_gpio));
	// 	return PTR_ERR(ctx->lcm_1v8_enable_gpio);
	// }
	// gpiod_set_value(ctx->lcm_1v8_enable_gpio, 1);
	// devm_gpiod_put(ctx->dev, ctx->lcm_1v8_enable_gpio);
	// msleep(5);
	// ctx->lcm_1v8_enable_gpio = devm_gpiod_get(dev, "lcm_1v8_enable", GPIOD_OUT_HIGH);
	// if (IS_ERR(ctx->lcm_1v8_enable_gpio)) {
	// 	dev_err(dev, "%s: cannot get lcm_1v8_enable-gpios %ld\n",
	// 		__func__, PTR_ERR(ctx->lcm_1v8_enable_gpio));
	// 	return PTR_ERR(ctx->lcm_1v8_enable_gpio);
	// }
	// devm_gpiod_put(dev, ctx->lcm_1v8_enable_gpio);

	ctx->prepared = true;
	ctx->enabled = true;

	drm_panel_init(&ctx->panel, dev, &lcm_drm_funcs, DRM_MODE_CONNECTOR_DSI);

	drm_panel_add(&ctx->panel);

	ret = mipi_dsi_attach(dsi);
	if (ret < 0)
		drm_panel_remove(&ctx->panel);
	lcm_gc7272_shutdown_status = 0;	
	st_drm_shutdown_status = 0;
#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_tch_handle_reg(&ctx->panel);
	ret = mtk_panel_ext_create(dev, &ext_params, &ext_funcs, &ctx->panel);
	if (ret < 0)
		return ret;
#endif

     // st_bc12_check_workqueue = create_workqueue("st_bc12_check-workqueue");
     // INIT_DELAYED_WORK(&st_bc12_check_work, st_bc12_check_switch);
     // st_ctx = ctx;
	//check_is_need_fake_resolution(dev);
	pr_info("%s-gc7272\n", __func__);

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
static void lcm_gc7272_shutdown(struct mipi_dsi_device *dsi)
{
//	struct lcm *ctx = mipi_dsi_get_drvdata(dsi);
	
	pr_info("%s-gc7272\n", __func__);
	lcm_gc7272_shutdown_status = 1;	
//	lcm_unprepare(&ctx->panel);
}


static const struct of_device_id lcm_of_match[] = {
	{ .compatible = "gc7272,hdplus1640,incell", },
	{ }
};

MODULE_DEVICE_TABLE(of, lcm_of_match);

static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.shutdown = lcm_gc7272_shutdown,
	.driver = {
		.name = "panel-gc7272-hdplus1640-dsi-boe",
		.owner = THIS_MODULE,
		.of_match_table = lcm_of_match,
	},
};

module_mipi_dsi_driver(lcm_driver);

MODULE_AUTHOR("dongshiyi <shiyi.dong@sunritel.com.cn>");
MODULE_DESCRIPTION("gc7272 hdplus1640 Panel Driver");
MODULE_LICENSE("GPL v2");