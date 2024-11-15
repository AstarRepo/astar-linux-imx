// // SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2022, SOMLABS
 */

#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/regulator/consumer.h>

#include <video/mipi_display.h>

#include <drm/drm_mipi_dsi.h>
#include <drm/drm_modes.h>
#include <drm/drm_panel.h>

struct wf101ktyapmnb0 {
	struct device *dev;
	struct drm_panel panel;
	struct gpio_desc *reset_gpio;
	bool prepared;
	bool enabled;
};

static const struct drm_display_mode wf101ktyapmnb0_default_mode = {
        .clock       = 70000,
        .hdisplay    = 1280,
        .hsync_start = 1280 + 72,
        .hsync_end   = 1280 + 72 + 10,
        .htotal      = 1280 + 72 + 10 + 88,
        .vdisplay    = 800,
        .vsync_start = 800 + 15,
        .vsync_end   = 800 + 15 + 10,
        .vtotal      = 800 + 15 + 10 + 23,
        .flags       = DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC,
        .width_mm    = 217,
        .height_mm   = 136,
};

static inline struct wf101ktyapmnb0 *panel_to_wf101ktyapmnb0(struct drm_panel *panel)
{
	return container_of(panel, struct wf101ktyapmnb0, panel);
}

static inline void unisystem_dsi_write(struct wf101ktyapmnb0 *ctx, const void *seq,
					size_t len)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	int err;
	uint8_t *cmd;

	cmd = (uint8_t*) seq;

	err = mipi_dsi_dcs_write_buffer (dsi, seq, len);
	if (err < 0)
		dev_err(ctx->dev, "[DEBUG] cmd : %d write failed: %d\n", (uint8_t)cmd[0], err);
}

#define WRITE_DSI(ctx, seq...)				\
	{							\
		const u8 d[] = { seq };				\
		unisystem_dsi_write(ctx, d, ARRAY_SIZE(d));	\
	}

static void wf101ktyapmnb0_init_sequence(struct wf101ktyapmnb0 *ctx)
{
    WRITE_DSI(ctx, MIPI_DCS_EXIT_SLEEP_MODE, 0x00);
	msleep(200);

	WRITE_DSI(ctx, 0xCD, 0xAA);

	msleep(20);
	WRITE_DSI(ctx, 0x4D, 0xAA);

	WRITE_DSI(ctx, 0x3D, 0x32);
	WRITE_DSI(ctx, 0x23, 0x81, 0x00, 0x01, 0x00);
	WRITE_DSI(ctx, 0x2E, 0x03);
	WRITE_DSI(ctx, 0x39, 0x09);

	WRITE_DSI(ctx, 0x52, 0x13, 0x13, 0x13, 0x13, 0x13, 0x13, 0x12, 0x13, 0x10, 0x11, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x03, 0x0C, 0x13, 0x13);
	WRITE_DSI(ctx, 0x59, 0x13, 0x13, 0x13, 0x13, 0x13, 0x13, 0x12, 0x13, 0x10, 0x11, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x03, 0x0C, 0x13, 0x13);

	WRITE_DSI(ctx, 0x32, 0x02);

	WRITE_DSI(ctx, 0x34, 0x7E);
	WRITE_DSI(ctx, 0x5F, 0x38);
	WRITE_DSI(ctx, 0x2B, 0x20);
	WRITE_DSI(ctx, 0x35, 0x05);
	WRITE_DSI(ctx, 0x33, 0x08);
	WRITE_DSI(ctx, 0x51, 0x80);
	WRITE_DSI(ctx, 0x73, 0xF0);
	WRITE_DSI(ctx, 0x74, 0x91);
	WRITE_DSI(ctx, 0x75, 0x03);
	WRITE_DSI(ctx, 0x71, 0xE3);
	WRITE_DSI(ctx, 0x7A, 0x17);
	WRITE_DSI(ctx, 0x3C, 0x40);
	WRITE_DSI(ctx, 0x4A, 0x02);
	WRITE_DSI(ctx, 0x18, 0xFF);
	WRITE_DSI(ctx, 0x19, 0x1F);
	WRITE_DSI(ctx, 0x1A, 0xDC);
	WRITE_DSI(ctx, 0x4E, 0x4A);
	WRITE_DSI(ctx, 0x4F, 0x4C);

	WRITE_DSI(ctx, 0x53, 0x37, 0x2A, 0x29, 0x2A, 0x2E, 0x2F, 0x22, 0x0D, 0x0E, 0x0C, 0x0E, 0x0F, 0x10);
	WRITE_DSI(ctx, 0x54, 0x37, 0x2A, 0x29, 0x2A, 0x2E, 0x2F, 0x22, 0x0D, 0x0E, 0x0C, 0x0E, 0x0F, 0x10);
	WRITE_DSI(ctx, 0x55, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11);
	WRITE_DSI(ctx, 0x56, 0x08);
	WRITE_DSI(ctx, 0x67, 0x22);
	WRITE_DSI(ctx, 0x6F, 0x01, 0x01, 0x01, 0x0A, 0x01, 0x01, 0x90, 0x00);
	WRITE_DSI(ctx, 0x6D, 0xA5);
	WRITE_DSI(ctx, 0x6C, 0x08);
	WRITE_DSI(ctx, 0x0E, 0x0A);

	msleep(200);
}

static int wf101ktyapmnb0_enable(struct drm_panel *panel)
{
        struct wf101ktyapmnb0 *ctx = panel_to_wf101ktyapmnb0(panel);
        struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
        int ret;

        if (ctx->enabled)
                return 0;

	if (ctx->reset_gpio != NULL) {
		gpiod_set_value(ctx->reset_gpio, 1);
		usleep_range(20000, 25000);

		gpiod_set_value(ctx->reset_gpio, 0);
		usleep_range(20000, 25000);
	}
	
        dsi->mode_flags |= MIPI_DSI_MODE_LPM;
		wf101ktyapmnb0_init_sequence(ctx);

        ret = mipi_dsi_dcs_exit_sleep_mode(dsi);
        if (ret)
                return ret;

        msleep(100);

        ret = mipi_dsi_dcs_set_display_on(dsi);
        if (ret)
                return ret;

        msleep(100);

        ctx->enabled = true;

        return 0;
}

static int wf101ktyapmnb0_disable(struct drm_panel *panel)
{
	struct wf101ktyapmnb0 *ctx = panel_to_wf101ktyapmnb0(panel);
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	int ret;

	if (!ctx->enabled)
		return 0;

	ret = mipi_dsi_dcs_set_display_off(dsi);
	if (ret)
		dev_warn(panel->dev, "failed to set display off: %d\n", ret);

	ret = mipi_dsi_dcs_enter_sleep_mode(dsi);
	if (ret)
		dev_warn(panel->dev, "failed to enter sleep mode: %d\n", ret);

	msleep(120);

	ctx->enabled = false;

	return 0;
}

static int wf101ktyapmnb0_unprepare(struct drm_panel *panel)
{
	struct wf101ktyapmnb0 *ctx = panel_to_wf101ktyapmnb0(panel);

	if (!ctx->prepared)
		return 0;

	if (ctx->reset_gpio != NULL) {
		gpiod_set_value(ctx->reset_gpio, 0);
		usleep_range(15000, 17000);
		gpiod_set_value(ctx->reset_gpio, 1);
	}

	ctx->prepared = false;

	return 0;
}

static int wf101ktyapmnb0_prepare(struct drm_panel *panel)
{
	struct wf101ktyapmnb0 *ctx = panel_to_wf101ktyapmnb0(panel);

	if (ctx->prepared)
		return 0;

	ctx->prepared = true;

	return 0;
}

static int wf101ktyapmnb0_get_modes(struct drm_panel *panel,
			     struct drm_connector *connector)
{
	struct drm_display_mode *mode;

	mode = drm_mode_duplicate(connector->dev, &wf101ktyapmnb0_default_mode);
	if (!mode) {
		dev_err(panel->dev, "failed to add mode %ux%u@%u\n",
			wf101ktyapmnb0_default_mode.hdisplay,
			wf101ktyapmnb0_default_mode.vdisplay,
			drm_mode_vrefresh(&wf101ktyapmnb0_default_mode));
		return -ENOMEM;
	}

	drm_mode_set_name(mode);

	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(connector, mode);

	connector->display_info.width_mm = mode->width_mm;
	connector->display_info.height_mm = mode->height_mm;

	return 1;
}

static const struct drm_panel_funcs wf101ktyapmnb0_drm_funcs = {
	.disable = wf101ktyapmnb0_disable,
	.unprepare = wf101ktyapmnb0_unprepare,
	.prepare = wf101ktyapmnb0_prepare,
	.enable = wf101ktyapmnb0_enable,
	.get_modes = wf101ktyapmnb0_get_modes,
};

static int wf101ktyapmnb0_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct wf101ktyapmnb0 *ctx;
	int ret;

	ctx = devm_kzalloc(dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);

	if (IS_ERR(ctx->reset_gpio))
		ctx->reset_gpio = NULL;
	else
		gpiod_set_value(ctx->reset_gpio, 0);

	mipi_dsi_set_drvdata(dsi, ctx);

	ctx->dev = dev;

	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_SYNC_PULSE | MIPI_DSI_CLOCK_NON_CONTINUOUS | MIPI_DSI_MODE_LPM;

	drm_panel_init(&ctx->panel, dev, &wf101ktyapmnb0_drm_funcs,
		       DRM_MODE_CONNECTOR_DSI);

	ret = drm_panel_of_backlight(&ctx->panel);
	if (ret)
		return ret;

	drm_panel_add(&ctx->panel);

	ret = mipi_dsi_attach(dsi);
	if (ret < 0) {
		dev_err(dev, "mipi_dsi_attach() failed: %d\n", ret);
		drm_panel_remove(&ctx->panel);
		return ret;
	}

	return 0;
}

static void wf101ktyapmnb0_remove(struct mipi_dsi_device *dsi)
{
	struct wf101ktyapmnb0 *ctx = mipi_dsi_get_drvdata(dsi);

	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);
}

static const struct of_device_id winstar_wf101ktyapmnb0_of_match[] = {
	{ .compatible = "winstar,wf101ktyapmnb0" },
	{ }
};
MODULE_DEVICE_TABLE(of, winstar_wf101ktyapmnb0_of_match);

static struct mipi_dsi_driver winstar_wf101ktyapmnb0_driver = {
	.probe = wf101ktyapmnb0_probe,
	.remove = wf101ktyapmnb0_remove,
	.driver = {
		.name = "panel-winstar-wf101ktyapmnb0",
		.of_match_table = winstar_wf101ktyapmnb0_of_match,
	},
};
module_mipi_dsi_driver(winstar_wf101ktyapmnb0_driver);

MODULE_AUTHOR("Grzegorz Mydlarz <gmydlarz@astar.eu>");
MODULE_DESCRIPTION("DRM Driver for winstar wf101ktyapmnb0 MIPI DSI panel");
MODULE_LICENSE("GPL v2");
