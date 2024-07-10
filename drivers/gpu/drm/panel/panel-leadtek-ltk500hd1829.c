// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2019 Theobroma Systems Design und Consulting GmbH
 *
 * base on panel-kingdisplay-kd097d04.c
 * Copyright (c) 2017, Fuzhou Rockchip Electronics Co., Ltd
 */

#include <linux/backlight.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/regulator/consumer.h>

#include <video/mipi_display.h>

#include <drm/drm_crtc.h>
#include <drm/drm_device.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_modes.h>
#include <drm/drm_panel.h>

struct ltk500hd1829_cmd {
	char cmd;
	char data;
};

struct ltk500hd1829_desc {
	const struct drm_display_mode *mode;
	const struct ltk500hd1829_cmd *init;
	unsigned int num_init;
};

struct ltk500hd1829 {
	struct device *dev;
	struct drm_panel panel;
	struct gpio_desc *reset_gpio;
	struct regulator *vcc;
	struct regulator *iovcc;
	const struct ltk500hd1829_desc *panel_desc;
	bool prepared;
};

static const struct ltk500hd1829_cmd ltk101b4029w_init[] = {
	/* Page0 */
	{ 0xE0, 0x00 },
	/* PASSWORD */
	{ 0xE1, 0x93 },
	{ 0xE2, 0x65 },
	{ 0xE3, 0xF8 },
	{ 0x80, 0x03 }, /* 0X03:4-LANE; 0X02:3-LANE; 0X01:2-LANE */
	/* Page1 */
	{ 0xE0, 0x01 },
	/* Set VCOM */
	{ 0x00, 0x00 },
	{ 0x01, 0x6F },
	/* Set Gamma Power, VGMP,VGMN,VGSP,VGSN */
	{ 0x17, 0x00 },
	{ 0x18, 0xAF }, /* 4.3V */
	{ 0x19, 0x01 }, /* 0.3V */
	{ 0x1A, 0x00 },
	{ 0x1B, 0xAF }, /* 4.3V */
	{ 0x1C, 0x01 }, /* 0.3V */
	/* Set Gate Power */
	{ 0x1F, 0x3E }, /* VGH_R  = 15V */
	{ 0x20, 0x28 }, /* VGL_R  = -12V */
	{ 0x21, 0x28 }, /* VGL_R2 = -12V */
	{ 0x22, 0x7E },
	/* SETPANEL */
	{ 0x35, 0x26 },
	{ 0x37, 0x09 },
	/* SET RGBCYC */
	{ 0x38, 0x04 },
	{ 0x39, 0x00 },
	{ 0x3A, 0x01 },
	{ 0x3C, 0x7C },
	{ 0x3D, 0xFF },
	{ 0x3E, 0xFF },
	{ 0x3F, 0x7F },
	/* Set TCON */
	{ 0x40, 0x06 }, /* RSO = 800 RGB */
	{ 0x41, 0xA0 }, /* LN = 640->1280 line */
	{ 0x42, 0x81 },
	{ 0x43, 0x08 }, /* VFP = 8 */
	{ 0x44, 0x0B }, /* VBP = 12 */
	{ 0x45, 0x28 }, /* HBP = 40 */
	/* power voltage */
	{ 0x55, 0x0F }, /* DCDCM = 0001, JD PWR_IC */
	{ 0x57, 0x69 },
	{ 0x59, 0x0A }, /* VCL = -2.9V */
	{ 0x5A, 0x28 }, /* VGH = 15V */
	{ 0x5B, 0x14 }, /* VGL = -11V */
	/* Gamma */
	{ 0x5D, 0x7C },
	{ 0x5E, 0x65 },
	{ 0x5F, 0x55 },
	{ 0x60, 0x47 },
	{ 0x61, 0x43 },
	{ 0x62, 0x32 },
	{ 0x63, 0x34 },
	{ 0x64, 0x1C },
	{ 0x65, 0x33 },
	{ 0x66, 0x31 },
	{ 0x67, 0x30 },
	{ 0x68, 0x4E },
	{ 0x69, 0x3C },
	{ 0x6A, 0x44 },
	{ 0x6B, 0x35 },
	{ 0x6C, 0x31 },
	{ 0x6D, 0x23 },
	{ 0x6E, 0x11 },
	{ 0x6F, 0x00 },
	{ 0x70, 0x7C },
	{ 0x71, 0x65 },
	{ 0x72, 0x55 },
	{ 0x73, 0x47 },
	{ 0x74, 0x43 },
	{ 0x75, 0x32 },
	{ 0x76, 0x34 },
	{ 0x77, 0x1C },
	{ 0x78, 0x33 },
	{ 0x79, 0x31 },
	{ 0x7A, 0x30 },
	{ 0x7B, 0x4E },
	{ 0x7C, 0x3C },
	{ 0x7D, 0x44 },
	{ 0x7E, 0x35 },
	{ 0x7F, 0x31 },
	{ 0x80, 0x23 },
	{ 0x81, 0x11 },
	{ 0x82, 0x00 },
	 /* Page2, for GIP */
	{ 0xE0, 0x02 },
	/* GIP_L Pin mapping */
	{ 0x00, 0x1E },
	{ 0x01, 0x1E },
	{ 0x02, 0x41 },
	{ 0x03, 0x41 },
	{ 0x04, 0x43 },
	{ 0x05, 0x43 },
	{ 0x06, 0x1F },
	{ 0x07, 0x1F },
	{ 0x08, 0x35 },
	{ 0x09, 0x1F },
	{ 0x0A, 0x15 },
	{ 0x0B, 0x15 },
	{ 0x0C, 0x1F },
	{ 0x0D, 0x47 },
	{ 0x0E, 0x47 },
	{ 0x0F, 0x45 },
	{ 0x10, 0x45 },
	{ 0x11, 0x4B },
	{ 0x12, 0x4B },
	{ 0x13, 0x49 },
	{ 0x14, 0x49 },
	{ 0x15, 0x1F },
	/* GIP_R Pin mapping */
	{ 0x16, 0x1E },
	{ 0x17, 0x1E },
	{ 0x18, 0x40 },
	{ 0x19, 0x40 },
	{ 0x1A, 0x42 },
	{ 0x1B, 0x42 },
	{ 0x1C, 0x1F },
	{ 0x1D, 0x1F },
	{ 0x1E, 0x35 },
	{ 0x1F, 0x1F },
	{ 0x20, 0x15 },
	{ 0x21, 0x15 },
	{ 0x22, 0x1f },
	{ 0x23, 0x46 },
	{ 0x24, 0x46 },
	{ 0x25, 0x44 },
	{ 0x26, 0x44 },
	{ 0x27, 0x4A },
	{ 0x28, 0x4A },
	{ 0x29, 0x48 },
	{ 0x2A, 0x48 },
	{ 0x2B, 0x1F },
	/* GIP Timing */
	{ 0x58, 0x40 },
	{ 0x5B, 0x30 },
	{ 0x5C, 0x03 },
	{ 0x5D, 0x30 },
	{ 0x5E, 0x01 },
	{ 0x5F, 0x02 },
	{ 0x63, 0x14 },
	{ 0x64, 0x6A },
	{ 0x67, 0x73 },
	{ 0x68, 0x05 },
	{ 0x69, 0x14 },
	{ 0x6A, 0x6A },
	{ 0x6B, 0x08 },
	{ 0x6C, 0x00 },
	{ 0x6D, 0x00 },
	{ 0x6E, 0x00 },
	{ 0x6F, 0x88 },
	{ 0x77, 0xDD },
	{ 0x79, 0x0E },
	{ 0x7A, 0x03 },
	{ 0x7D, 0x14 },
	{ 0x7E, 0x6A },
	/* Page4 */
	{ 0xE0, 0x04 },
	{ 0x09, 0x11 },
	{ 0x0E, 0x48 },
	{ 0x2B, 0x2B },
	{ 0x2D, 0x03 },
	{ 0x2E, 0x44 },
	/* Page0 */
	{ 0xE0, 0x00 },
	{ 0xE6, 0x02 },
	{ 0xE7, 0x0C },
};

static const struct drm_display_mode ltk101b4029w_mode = {
	.hdisplay	= 800,
	.hsync_start	= 800 + 18,
	.hsync_end	= 800 + 18 + 18,
	.htotal		= 800 + 18 + 18 + 18,
	.vdisplay	= 1280,
	.vsync_start	= 1280 + 24,
	.vsync_end	= 1280 + 24 + 4,
	.vtotal		= 1280 + 24 + 4 + 8,
	.clock		= 67330,
	.width_mm	= 136,
	.height_mm	= 218,
};

static const struct ltk500hd1829_desc ltk101b4029w_data = {
	.mode = &ltk101b4029w_mode,
	.init = ltk101b4029w_init,
	.num_init = ARRAY_SIZE(ltk101b4029w_init),
};

/*
 * There is no description in the Reference Manual about these commands.
 * We received them from the vendor, so just use them as is.
 */
static const struct ltk500hd1829_cmd ltk500hd1829_init[] = {
	{ 0xE0, 0x00 },
	{ 0xE1, 0x93 },
	{ 0xE2, 0x65 },
	{ 0xE3, 0xF8 },
	{ 0x80, 0x03 },
	{ 0xE0, 0x04 },
	{ 0x2D, 0x03 },
	{ 0xE0, 0x01 },
	{ 0x00, 0x00 },
	{ 0x01, 0xB6 },
	{ 0x03, 0x00 },
	{ 0x04, 0xC5 },
	{ 0x17, 0x00 },
	{ 0x18, 0xBF },
	{ 0x19, 0x01 },
	{ 0x1A, 0x00 },
	{ 0x1B, 0xBF },
	{ 0x1C, 0x01 },
	{ 0x1F, 0x7C },
	{ 0x20, 0x26 },
	{ 0x21, 0x26 },
	{ 0x22, 0x4E },
	{ 0x37, 0x09 },
	{ 0x38, 0x04 },
	{ 0x39, 0x08 },
	{ 0x3A, 0x1F },
	{ 0x3B, 0x1F },
	{ 0x3C, 0x78 },
	{ 0x3D, 0xFF },
	{ 0x3E, 0xFF },
	{ 0x3F, 0x00 },
	{ 0x40, 0x04 },
	{ 0x41, 0xA0 },
	{ 0x43, 0x0F },
	{ 0x44, 0x0A },
	{ 0x45, 0x24 },
	{ 0x55, 0x01 },
	{ 0x56, 0x01 },
	{ 0x57, 0xA5 },
	{ 0x58, 0x0A },
	{ 0x59, 0x4A },
	{ 0x5A, 0x38 },
	{ 0x5B, 0x10 },
	{ 0x5C, 0x19 },
	{ 0x5D, 0x7C },
	{ 0x5E, 0x64 },
	{ 0x5F, 0x54 },
	{ 0x60, 0x48 },
	{ 0x61, 0x44 },
	{ 0x62, 0x35 },
	{ 0x63, 0x3A },
	{ 0x64, 0x24 },
	{ 0x65, 0x3B },
	{ 0x66, 0x39 },
	{ 0x67, 0x37 },
	{ 0x68, 0x56 },
	{ 0x69, 0x41 },
	{ 0x6A, 0x47 },
	{ 0x6B, 0x2F },
	{ 0x6C, 0x23 },
	{ 0x6D, 0x13 },
	{ 0x6E, 0x02 },
	{ 0x6F, 0x08 },
	{ 0x70, 0x7C },
	{ 0x71, 0x64 },
	{ 0x72, 0x54 },
	{ 0x73, 0x48 },
	{ 0x74, 0x44 },
	{ 0x75, 0x35 },
	{ 0x76, 0x3A },
	{ 0x77, 0x22 },
	{ 0x78, 0x3B },
	{ 0x79, 0x39 },
	{ 0x7A, 0x38 },
	{ 0x7B, 0x52 },
	{ 0x7C, 0x41 },
	{ 0x7D, 0x47 },
	{ 0x7E, 0x2F },
	{ 0x7F, 0x23 },
	{ 0x80, 0x13 },
	{ 0x81, 0x02 },
	{ 0x82, 0x08 },
	{ 0xE0, 0x02 },
	{ 0x00, 0x57 },
	{ 0x01, 0x77 },
	{ 0x02, 0x44 },
	{ 0x03, 0x46 },
	{ 0x04, 0x48 },
	{ 0x05, 0x4A },
	{ 0x06, 0x4C },
	{ 0x07, 0x4E },
	{ 0x08, 0x50 },
	{ 0x09, 0x55 },
	{ 0x0A, 0x52 },
	{ 0x0B, 0x55 },
	{ 0x0C, 0x55 },
	{ 0x0D, 0x55 },
	{ 0x0E, 0x55 },
	{ 0x0F, 0x55 },
	{ 0x10, 0x55 },
	{ 0x11, 0x55 },
	{ 0x12, 0x55 },
	{ 0x13, 0x40 },
	{ 0x14, 0x55 },
	{ 0x15, 0x55 },
	{ 0x16, 0x57 },
	{ 0x17, 0x77 },
	{ 0x18, 0x45 },
	{ 0x19, 0x47 },
	{ 0x1A, 0x49 },
	{ 0x1B, 0x4B },
	{ 0x1C, 0x4D },
	{ 0x1D, 0x4F },
	{ 0x1E, 0x51 },
	{ 0x1F, 0x55 },
	{ 0x20, 0x53 },
	{ 0x21, 0x55 },
	{ 0x22, 0x55 },
	{ 0x23, 0x55 },
	{ 0x24, 0x55 },
	{ 0x25, 0x55 },
	{ 0x26, 0x55 },
	{ 0x27, 0x55 },
	{ 0x28, 0x55 },
	{ 0x29, 0x41 },
	{ 0x2A, 0x55 },
	{ 0x2B, 0x55 },
	{ 0x2C, 0x57 },
	{ 0x2D, 0x77 },
	{ 0x2E, 0x4F },
	{ 0x2F, 0x4D },
	{ 0x30, 0x4B },
	{ 0x31, 0x49 },
	{ 0x32, 0x47 },
	{ 0x33, 0x45 },
	{ 0x34, 0x41 },
	{ 0x35, 0x55 },
	{ 0x36, 0x53 },
	{ 0x37, 0x55 },
	{ 0x38, 0x55 },
	{ 0x39, 0x55 },
	{ 0x3A, 0x55 },
	{ 0x3B, 0x55 },
	{ 0x3C, 0x55 },
	{ 0x3D, 0x55 },
	{ 0x3E, 0x55 },
	{ 0x3F, 0x51 },
	{ 0x40, 0x55 },
	{ 0x41, 0x55 },
	{ 0x42, 0x57 },
	{ 0x43, 0x77 },
	{ 0x44, 0x4E },
	{ 0x45, 0x4C },
	{ 0x46, 0x4A },
	{ 0x47, 0x48 },
	{ 0x48, 0x46 },
	{ 0x49, 0x44 },
	{ 0x4A, 0x40 },
	{ 0x4B, 0x55 },
	{ 0x4C, 0x52 },
	{ 0x4D, 0x55 },
	{ 0x4E, 0x55 },
	{ 0x4F, 0x55 },
	{ 0x50, 0x55 },
	{ 0x51, 0x55 },
	{ 0x52, 0x55 },
	{ 0x53, 0x55 },
	{ 0x54, 0x55 },
	{ 0x55, 0x50 },
	{ 0x56, 0x55 },
	{ 0x57, 0x55 },
	{ 0x58, 0x40 },
	{ 0x59, 0x00 },
	{ 0x5A, 0x00 },
	{ 0x5B, 0x10 },
	{ 0x5C, 0x09 },
	{ 0x5D, 0x30 },
	{ 0x5E, 0x01 },
	{ 0x5F, 0x02 },
	{ 0x60, 0x30 },
	{ 0x61, 0x03 },
	{ 0x62, 0x04 },
	{ 0x63, 0x06 },
	{ 0x64, 0x6A },
	{ 0x65, 0x75 },
	{ 0x66, 0x0F },
	{ 0x67, 0xB3 },
	{ 0x68, 0x0B },
	{ 0x69, 0x06 },
	{ 0x6A, 0x6A },
	{ 0x6B, 0x10 },
	{ 0x6C, 0x00 },
	{ 0x6D, 0x04 },
	{ 0x6E, 0x04 },
	{ 0x6F, 0x88 },
	{ 0x70, 0x00 },
	{ 0x71, 0x00 },
	{ 0x72, 0x06 },
	{ 0x73, 0x7B },
	{ 0x74, 0x00 },
	{ 0x75, 0xBC },
	{ 0x76, 0x00 },
	{ 0x77, 0x05 },
	{ 0x78, 0x2E },
	{ 0x79, 0x00 },
	{ 0x7A, 0x00 },
	{ 0x7B, 0x00 },
	{ 0x7C, 0x00 },
	{ 0x7D, 0x03 },
	{ 0x7E, 0x7B },
	{ 0xE0, 0x04 },
	{ 0x09, 0x10 },
	{ 0x2B, 0x2B },
	{ 0x2E, 0x44 },
	{ 0xE0, 0x00 },
	{ 0xE6, 0x02 },
	{ 0xE7, 0x02 },
	{ 0x35, 0x00 },
};

static const struct drm_display_mode ltk500hd1829_mode = {
	.hdisplay	= 720,
	.hsync_start	= 720 + 50,
	.hsync_end	= 720 + 50 + 50,
	.htotal		= 720 + 50 + 50 + 50,
	.vdisplay	= 1280,
	.vsync_start	= 1280 + 30,
	.vsync_end	= 1280 + 30 + 4,
	.vtotal		= 1280 + 30 + 4 + 12,
	.clock		= 69217,
	.width_mm	= 62,
	.height_mm	= 110,
};

static const struct ltk500hd1829_desc ltk500hd1829_data = {
	.mode = &ltk500hd1829_mode,
	.init = ltk500hd1829_init,
	.num_init = ARRAY_SIZE(ltk500hd1829_init),
};

static inline
struct ltk500hd1829 *panel_to_ltk500hd1829(struct drm_panel *panel)
{
	return container_of(panel, struct ltk500hd1829, panel);
}

static int ltk500hd1829_unprepare(struct drm_panel *panel)
{
	struct ltk500hd1829 *ctx = panel_to_ltk500hd1829(panel);
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	int ret;

	if (!ctx->prepared)
		return 0;

	ret = mipi_dsi_dcs_set_display_off(dsi);
	if (ret < 0)
		dev_err(panel->dev, "failed to set display off: %d\n", ret);

	ret = mipi_dsi_dcs_enter_sleep_mode(dsi);
	if (ret < 0) {
		dev_err(panel->dev, "failed to enter sleep mode: %d\n", ret);
	}

	/* 120ms to enter sleep mode */
	msleep(120);

	regulator_disable(ctx->iovcc);
	regulator_disable(ctx->vcc);

	ctx->prepared = false;

	return 0;
}

static int ltk500hd1829_prepare(struct drm_panel *panel)
{
	struct ltk500hd1829 *ctx = panel_to_ltk500hd1829(panel);
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	unsigned int i;
	int ret;

	if (ctx->prepared)
		return 0;

	ret = regulator_enable(ctx->vcc);
	if (ret < 0) {
		dev_err(ctx->dev, "Failed to enable vci supply: %d\n", ret);
		return ret;
	}
	ret = regulator_enable(ctx->iovcc);
	if (ret < 0) {
		dev_err(ctx->dev, "Failed to enable iovcc supply: %d\n", ret);
		goto disable_vcc;
	}

	gpiod_set_value_cansleep(ctx->reset_gpio, 1);
	/* tRW: 10us */
	usleep_range(10, 20);
	gpiod_set_value_cansleep(ctx->reset_gpio, 0);

	/* tRT: >= 5ms */
	usleep_range(5000, 6000);

	for (i = 0; i < ctx->panel_desc->num_init; i++) {
		ret = mipi_dsi_generic_write(dsi, &ctx->panel_desc->init[i],
					     sizeof(struct ltk500hd1829_cmd));
		if (ret < 0) {
			dev_err(panel->dev, "failed to write init cmds: %d\n", ret);
			goto disable_iovcc;
		}
	}

	ret = mipi_dsi_dcs_exit_sleep_mode(dsi);
	if (ret < 0) {
		dev_err(panel->dev, "failed to exit sleep mode: %d\n", ret);
		goto disable_iovcc;
	}

	/* 120ms to exit sleep mode */
	msleep(120);

	ret = mipi_dsi_dcs_set_display_on(dsi);
	if (ret < 0) {
		dev_err(panel->dev, "failed to set display on: %d\n", ret);
		goto disable_iovcc;
	}

	ctx->prepared = true;

	return 0;

disable_iovcc:
	regulator_disable(ctx->iovcc);
disable_vcc:
	regulator_disable(ctx->vcc);
	return ret;
}

static int ltk500hd1829_get_modes(struct drm_panel *panel,
				  struct drm_connector *connector)
{
	struct ltk500hd1829 *ctx = panel_to_ltk500hd1829(panel);
	struct drm_display_mode *mode;

	mode = drm_mode_duplicate(connector->dev, ctx->panel_desc->mode);
	if (!mode) {
		dev_err(ctx->dev, "failed to add mode %ux%u@%u\n",
			ctx->panel_desc->mode->hdisplay, ctx->panel_desc->mode->vdisplay,
			drm_mode_vrefresh(ctx->panel_desc->mode));
		return -ENOMEM;
	}

	drm_mode_set_name(mode);

	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	connector->display_info.width_mm = mode->width_mm;
	connector->display_info.height_mm = mode->height_mm;
	drm_mode_probed_add(connector, mode);

	return 1;
}

static const struct drm_panel_funcs ltk500hd1829_funcs = {
	.unprepare = ltk500hd1829_unprepare,
	.prepare = ltk500hd1829_prepare,
	.get_modes = ltk500hd1829_get_modes,
};

static int ltk500hd1829_probe(struct mipi_dsi_device *dsi)
{
	struct ltk500hd1829 *ctx;
	struct device *dev = &dsi->dev;
	int ret;

	ctx = devm_kzalloc(&dsi->dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	ctx->panel_desc = of_device_get_match_data(dev);
	if (!ctx->panel_desc)
		return -EINVAL;

	ctx->reset_gpio = devm_gpiod_get_optional(dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(dev, "cannot get reset gpio\n");
		return PTR_ERR(ctx->reset_gpio);
	}

	ctx->vcc = devm_regulator_get(dev, "vcc");
	if (IS_ERR(ctx->vcc)) {
		ret = PTR_ERR(ctx->vcc);
		if (ret != -EPROBE_DEFER)
			dev_err(dev, "Failed to request vcc regulator: %d\n", ret);
		return ret;
	}

	ctx->iovcc = devm_regulator_get(dev, "iovcc");
	if (IS_ERR(ctx->iovcc)) {
		ret = PTR_ERR(ctx->iovcc);
		if (ret != -EPROBE_DEFER)
			dev_err(dev, "Failed to request iovcc regulator: %d\n", ret);
		return ret;
	}

	mipi_dsi_set_drvdata(dsi, ctx);

	ctx->dev = dev;

	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_BURST |
			  MIPI_DSI_MODE_LPM | MIPI_DSI_MODE_NO_EOT_PACKET;

	drm_panel_init(&ctx->panel, &dsi->dev, &ltk500hd1829_funcs,
		       DRM_MODE_CONNECTOR_DSI);

	ret = drm_panel_of_backlight(&ctx->panel);
	if (ret)
		return ret;

	drm_panel_add(&ctx->panel);

	ret = mipi_dsi_attach(dsi);
	if (ret < 0) {
		dev_err(dev, "mipi_dsi_attach failed: %d\n", ret);
		drm_panel_remove(&ctx->panel);
		return ret;
	}

	return 0;
}

static void ltk500hd1829_shutdown(struct mipi_dsi_device *dsi)
{
	struct ltk500hd1829 *ctx = mipi_dsi_get_drvdata(dsi);
	int ret;

	ret = drm_panel_unprepare(&ctx->panel);
	if (ret < 0)
		dev_err(&dsi->dev, "Failed to unprepare panel: %d\n", ret);

	ret = drm_panel_disable(&ctx->panel);
	if (ret < 0)
		dev_err(&dsi->dev, "Failed to disable panel: %d\n", ret);
}

static void ltk500hd1829_remove(struct mipi_dsi_device *dsi)
{
	struct ltk500hd1829 *ctx = mipi_dsi_get_drvdata(dsi);
	int ret;

	ltk500hd1829_shutdown(dsi);

	ret = mipi_dsi_detach(dsi);
	if (ret < 0)
		dev_err(&dsi->dev, "failed to detach from DSI host: %d\n", ret);

	drm_panel_remove(&ctx->panel);
}

static const struct of_device_id ltk500hd1829_of_match[] = {
	{
		.compatible = "leadtek,ltk101b4029w",
		.data = &ltk101b4029w_data,
	},
	{
		.compatible = "leadtek,ltk500hd1829",
		.data = &ltk500hd1829_data,
	},
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, ltk500hd1829_of_match);

static struct mipi_dsi_driver ltk500hd1829_driver = {
	.driver = {
		.name = "panel-leadtek-ltk500hd1829",
		.of_match_table = ltk500hd1829_of_match,
	},
	.probe = ltk500hd1829_probe,
	.remove = ltk500hd1829_remove,
	.shutdown = ltk500hd1829_shutdown,
};
module_mipi_dsi_driver(ltk500hd1829_driver);

MODULE_AUTHOR("Heiko Stuebner <heiko.stuebner@theobroma-systems.com>");
MODULE_DESCRIPTION("Leadtek LTK500HD1829 panel driver");
MODULE_LICENSE("GPL v2");
