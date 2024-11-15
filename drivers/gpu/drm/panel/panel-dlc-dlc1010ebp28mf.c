// SPDX-License-Identifier: GPL-2.0
/*
 */

#include <linux/backlight.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/fb.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/media-bus-format.h>

#include <linux/gpio/consumer.h>
#include <linux/regulator/consumer.h>

#include <drm/drm_mipi_dsi.h>
#include <drm/drm_modes.h>
#include <drm/drm_panel.h>
#include <drm/drm_crtc.h>

#include <video/mipi_display.h>

#include <video/of_videomode.h>
#include <video/videomode.h>

#include <linux/of.h>


static const u32 dlc1010ebp28mf_bus_formats[] = {
	MEDIA_BUS_FMT_RGB888_1X24,
	MEDIA_BUS_FMT_RGB666_1X18,
	MEDIA_BUS_FMT_RGB565_1X16,
};

struct dlc1010ebp28mf {
	struct drm_panel	panel;
	struct mipi_dsi_device	*dsi;

	struct gpio_desc	*reset;
	struct backlight_device *backlight;

	struct regulator	*power;

	bool prepared;
	bool enabled;

	struct videomode vm;
	u32 width_mm;
	u32 height_mm;
};

struct dlc1010ebp28mf_instr {
        u8 cmd;
        u8 data;
};

#define CMD_INSTR(CMD, DATA)     {.cmd = CMD, .data = DATA}

static const struct dlc1010ebp28mf_instr dlc1010ebp28mf_init_data[] = {
	CMD_INSTR(0xFF, 0x03),
	CMD_INSTR(0x01, 0x00),
	CMD_INSTR(0x02, 0x00),
	CMD_INSTR(0x03, 0x53),
	CMD_INSTR(0x04, 0x53),
	CMD_INSTR(0x05, 0x13),
	CMD_INSTR(0x06, 0x04),
	CMD_INSTR(0x07, 0x02),
	CMD_INSTR(0x08, 0x02),
	CMD_INSTR(0x09, 0x00),
	CMD_INSTR(0x0A, 0x00),
	CMD_INSTR(0x0B, 0x00),
	CMD_INSTR(0x0C, 0x00),
	CMD_INSTR(0x0D, 0x00),
	CMD_INSTR(0x0E, 0x00),
	CMD_INSTR(0x0F, 0x00),
	CMD_INSTR(0x10, 0x00),
	CMD_INSTR(0x11, 0x00),
	CMD_INSTR(0x12, 0x00),
	CMD_INSTR(0x13, 0x00),
	CMD_INSTR(0x14, 0x00),
	CMD_INSTR(0x15, 0x00),
	CMD_INSTR(0x16, 0x00),
	CMD_INSTR(0x17, 0x00),
	CMD_INSTR(0x18, 0x00),
	CMD_INSTR(0x19, 0x00),
	CMD_INSTR(0x1A, 0x00),
	CMD_INSTR(0x1B, 0x00),
	CMD_INSTR(0x1C, 0x00),
	CMD_INSTR(0x1D, 0x00),
	CMD_INSTR(0x1E, 0xC0),
	CMD_INSTR(0x1F, 0x00),
	CMD_INSTR(0x20, 0x02),
	CMD_INSTR(0x21, 0x09),
	CMD_INSTR(0x22, 0x00),
	CMD_INSTR(0x23, 0x00),
	CMD_INSTR(0x24, 0x00),
	CMD_INSTR(0x25, 0x00),
	CMD_INSTR(0x26, 0x00),
	CMD_INSTR(0x27, 0x00),
	CMD_INSTR(0x28, 0x55),
	CMD_INSTR(0x29, 0x03),
	CMD_INSTR(0x2A, 0x00),
	CMD_INSTR(0x2B, 0x00),
	CMD_INSTR(0x2C, 0x00),
	CMD_INSTR(0x2D, 0x00),
	CMD_INSTR(0x2E, 0x00),
	CMD_INSTR(0x2F, 0x00),
	CMD_INSTR(0x30, 0x00),
	CMD_INSTR(0x31, 0x00),
	CMD_INSTR(0x32, 0x00),
	CMD_INSTR(0x33, 0x00),
	CMD_INSTR(0x34, 0x00),
	CMD_INSTR(0x35, 0x00),
	CMD_INSTR(0x36, 0x00),
	CMD_INSTR(0x37, 0x00),
	CMD_INSTR(0x38, 0x3C),
	CMD_INSTR(0x39, 0x00),
	CMD_INSTR(0x3A, 0x00),
	CMD_INSTR(0x3B, 0x00),
	CMD_INSTR(0x3C, 0x00),
	CMD_INSTR(0x3D, 0x00),
	CMD_INSTR(0x3E, 0x00),
	CMD_INSTR(0x3F, 0x00),
	CMD_INSTR(0x40, 0x00),
	CMD_INSTR(0x41, 0x00),
	CMD_INSTR(0x42, 0x00),
	CMD_INSTR(0x43, 0x00),
	CMD_INSTR(0x44, 0x00),
	CMD_INSTR(0x45, 0x00),
	CMD_INSTR(0x50, 0x01),
	CMD_INSTR(0x51, 0x23),
	CMD_INSTR(0x52, 0x45),
	CMD_INSTR(0x53, 0x67),
	CMD_INSTR(0x54, 0x89),
	CMD_INSTR(0x55, 0xAB),
	CMD_INSTR(0x56, 0x01),
	CMD_INSTR(0x57, 0x23),
	CMD_INSTR(0x58, 0x45),
	CMD_INSTR(0x59, 0x67),
	CMD_INSTR(0x5A, 0x89),
	CMD_INSTR(0x5B, 0xAB),
	CMD_INSTR(0x5C, 0xCD),
	CMD_INSTR(0x5D, 0xEF),
	CMD_INSTR(0x5E, 0x01),
	CMD_INSTR(0x5F, 0x0A),
	CMD_INSTR(0x60, 0x02),
	CMD_INSTR(0x61, 0x02),
	CMD_INSTR(0x62, 0x08),
	CMD_INSTR(0x63, 0x15),
	CMD_INSTR(0x64, 0x14),
	CMD_INSTR(0x65, 0x02),
	CMD_INSTR(0x66, 0x11),
	CMD_INSTR(0x67, 0x10),
	CMD_INSTR(0x68, 0x02),
	CMD_INSTR(0x69, 0x0F),
	CMD_INSTR(0x6A, 0x0E),
	CMD_INSTR(0x6B, 0x02),
	CMD_INSTR(0x6C, 0x0D),
	CMD_INSTR(0x6D, 0x0C),
	CMD_INSTR(0x6E, 0x06),
	CMD_INSTR(0x6F, 0x02),
	CMD_INSTR(0x70, 0x02),
	CMD_INSTR(0x71, 0x02),
	CMD_INSTR(0x72, 0x02),
	CMD_INSTR(0x73, 0x02),
	CMD_INSTR(0x74, 0x02),
	CMD_INSTR(0x75, 0x0A),
	CMD_INSTR(0x76, 0x02),
	CMD_INSTR(0x77, 0x02),
	CMD_INSTR(0x78, 0x06),
	CMD_INSTR(0x79, 0x15),
	CMD_INSTR(0x7A, 0x14),
	CMD_INSTR(0x7B, 0x02),
	CMD_INSTR(0x7C, 0x10),
	CMD_INSTR(0x7D, 0x11),
	CMD_INSTR(0x7E, 0x02),
	CMD_INSTR(0x7F, 0x0C),
	CMD_INSTR(0x80, 0x0D),
	CMD_INSTR(0x81, 0x02),
	CMD_INSTR(0x82, 0x0E),
	CMD_INSTR(0x83, 0x0F),
	CMD_INSTR(0x84, 0x08),
	CMD_INSTR(0x85, 0x02),
	CMD_INSTR(0x86, 0x02),
	CMD_INSTR(0x87, 0x02),
	CMD_INSTR(0x88, 0x02),
	CMD_INSTR(0x89, 0x02),
	CMD_INSTR(0x8A, 0x02),
	CMD_INSTR(0xFF, 0x04),
	CMD_INSTR(0x6C, 0x15),
	CMD_INSTR(0x6E, 0x30),
	CMD_INSTR(0x6F, 0x55),
	CMD_INSTR(0x3A, 0x24),
	CMD_INSTR(0x8D, 0x1F),
	CMD_INSTR(0x87, 0xBA),
	CMD_INSTR(0x26, 0x76),
	CMD_INSTR(0xB2, 0xD1),
	CMD_INSTR(0xB5, 0x07),
	CMD_INSTR(0x35, 0x1F),
	CMD_INSTR(0x88, 0x0B),
	CMD_INSTR(0x21, 0x30),
	CMD_INSTR(0xFF, 0x01),
	CMD_INSTR(0x22, 0x0A),
	CMD_INSTR(0x31, 0x09),
	CMD_INSTR(0x40, 0x33),
	CMD_INSTR(0x53, 0x37),
	CMD_INSTR(0x55, 0x88),
	CMD_INSTR(0x50, 0x95),
	CMD_INSTR(0x51, 0x95),
	CMD_INSTR(0x60, 0x30),
	CMD_INSTR(0xA0, 0x0F),
	CMD_INSTR(0xA1, 0x17),
	CMD_INSTR(0xA2, 0x22),
	CMD_INSTR(0xA3, 0x19),
	CMD_INSTR(0xA4, 0x15),
	CMD_INSTR(0xA5, 0x28),
	CMD_INSTR(0xA6, 0x1C),
	CMD_INSTR(0xA7, 0x1C),
	CMD_INSTR(0xA8, 0x78),
	CMD_INSTR(0xA9, 0x1C),
	CMD_INSTR(0xAA, 0x28),
	CMD_INSTR(0xAB, 0x69),
	CMD_INSTR(0xAC, 0x1A),
	CMD_INSTR(0xAD, 0x19),
	CMD_INSTR(0xAE, 0x4B),
	CMD_INSTR(0xAF, 0x22),
	CMD_INSTR(0xB0, 0x2A),
	CMD_INSTR(0xB1, 0x4B),
	CMD_INSTR(0xB2, 0x6B),
	CMD_INSTR(0xB3, 0x3F),
	CMD_INSTR(0xC0, 0x01),
	CMD_INSTR(0xC1, 0x17),
	CMD_INSTR(0xC2, 0x22),
	CMD_INSTR(0xC3, 0x19),
	CMD_INSTR(0xC4, 0x15),
	CMD_INSTR(0xC5, 0x28),
	CMD_INSTR(0xC6, 0x1C),
	CMD_INSTR(0xC7, 0x1D),
	CMD_INSTR(0xC8, 0x78),
	CMD_INSTR(0xC9, 0x1C),
	CMD_INSTR(0xCA, 0x28),
	CMD_INSTR(0xCB, 0x69),
	CMD_INSTR(0xCC, 0x1A),
	CMD_INSTR(0xCD, 0x19),
	CMD_INSTR(0xCE, 0x4B),
	CMD_INSTR(0xCF, 0x22),
	CMD_INSTR(0xD0, 0x2A),
	CMD_INSTR(0xD1, 0x4B),
	CMD_INSTR(0xD2, 0x6B),
	CMD_INSTR(0xD3, 0x3F),
	CMD_INSTR(0xFF, 0x00),
	CMD_INSTR(0x35, 0x00),
	CMD_INSTR(0x11, 0x00),
};

static inline struct dlc1010ebp28mf *panel_to_dlc1010ebp28mf(struct drm_panel *panel)
{
	return container_of(panel, struct dlc1010ebp28mf, panel);
}

static int dlc1010ebp28mf_switch_page(struct mipi_dsi_device *dsi, u8 page)
{
	u8 buf[4] = { 0xff, 0x98, 0x81, page };

	return mipi_dsi_dcs_write_buffer(dsi, buf, sizeof(buf));
}

static int dlc1010ebp28mf_send_cmd_data(struct mipi_dsi_device *dsi, u8 cmd, u8 data)
{
	u8 buf[2] = { cmd, data };
	return mipi_dsi_dcs_write_buffer(dsi, buf, sizeof(buf));
}

static int dlc1010ebp28mf_init(struct mipi_dsi_device *dsi)
{
	size_t i;
	int ret = 0;
	for (i = 0; i < ARRAY_SIZE(dlc1010ebp28mf_init_data); i++) {
		const struct dlc1010ebp28mf_instr *instr = &dlc1010ebp28mf_init_data[i];

		if (instr->cmd == 0xFF) {
			ret = dlc1010ebp28mf_switch_page(dsi, instr->data);
		} else {
			ret = dlc1010ebp28mf_send_cmd_data(dsi, instr->cmd,
						      instr->data);
		}
		if (ret < 0) {
			dev_err(&dsi->dev, "Error when setting device @ %lu (cmd: %08X)\n", i, instr->cmd);
			return ret;
		}
	}
	if(ret > 0) {
		ret = 0;
	}
	return ret;
}

static int dlc1010ebp28mf_prepare(struct drm_panel *panel)
{
	struct dlc1010ebp28mf *ctx = panel_to_dlc1010ebp28mf(panel);

	if (ctx->reset) {

		gpiod_set_value(ctx->reset, 1);
		usleep_range(20000, 25000);

		gpiod_set_value(ctx->reset, 0);
		usleep_range(20000, 25000);
	}

	ctx->prepared = true;

	return 0;
}

static int dlc1010ebp28mf_unprepare(struct drm_panel *panel)
{
	struct dlc1010ebp28mf *ctx = panel_to_dlc1010ebp28mf(panel);
		struct device *dev = &ctx->dsi->dev;

	if (!ctx->prepared)
		return 0;

	if (ctx->enabled) {
		dev_err(dev, "Panel still enabled!\n");
		return -EPERM;
	}

	if (ctx->reset != NULL) {
		gpiod_set_value(ctx->reset, 0);
		usleep_range(15000, 17000);
		gpiod_set_value(ctx->reset, 1);
	}

	ctx->prepared = false;
	return 0;
}

static int dlc1010ebp28mf_enable(struct drm_panel *panel)
{
	struct dlc1010ebp28mf *ctx = panel_to_dlc1010ebp28mf(panel);
	struct mipi_dsi_device *dsi = ctx->dsi;
	struct device *dev = &dsi->dev;
	u8 buf[2] = {0};

	if (ctx->enabled)
		return 0;

	if (!ctx->prepared) {
		dev_err(dev, "Panel not prepared!\n");
		return -EPERM;
	}

	dsi->mode_flags |= MIPI_DSI_MODE_LPM;

	dlc1010ebp28mf_init(dsi);

	usleep_range(15000, 17000);


	dlc1010ebp28mf_switch_page(dsi, 0);
	buf[0] = MIPI_DCS_EXIT_SLEEP_MODE;
	buf[1] = 0;
	mipi_dsi_dcs_write_buffer(dsi, buf, 2);
	mdelay(120);
	buf[0] = MIPI_DCS_SET_DISPLAY_ON;
	mipi_dsi_dcs_write_buffer(dsi, buf, 2);

	backlight_enable(ctx->backlight);

	ctx->enabled = true;
	dsi->mode_flags &= ~MIPI_DSI_MODE_LPM;

	return 0;
}

static int dlc1010ebp28mf_disable(struct drm_panel *panel)
{
	struct dlc1010ebp28mf *ctx = panel_to_dlc1010ebp28mf(panel);
	struct mipi_dsi_device *dsi = ctx->dsi;
	struct device *dev = &dsi->dev;
	int ret;

	if (!ctx->enabled)
		return 0;

	dsi->mode_flags |= MIPI_DSI_MODE_LPM;

	backlight_disable(ctx->backlight);

	usleep_range(10000, 15000);

	ret = mipi_dsi_dcs_set_display_off(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to set display OFF (%d)\n", ret);
	}

	usleep_range(5000, 10000);

	ret = mipi_dsi_dcs_enter_sleep_mode(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to enter sleep mode (%d)\n", ret);
	}

	ctx->enabled = false;

	return 0;
}

static int dlc1010ebp28mf_get_modes(struct drm_panel *panel, struct drm_connector *connector)
{
	struct dlc1010ebp28mf *ctx = panel_to_dlc1010ebp28mf(panel);
	struct device *dev = &ctx->dsi->dev;
	struct drm_display_mode *mode;
	u32 *bus_flags = &connector->display_info.bus_flags;
	int ret;

	dev_info(dev, "get modes\n");

	mode = drm_mode_create(connector->dev);
	if (!mode) {
		dev_err(dev, "Failed to create display mode!\n");
		return 0;
	}

	drm_display_mode_from_videomode(&ctx->vm, mode);
	mode->width_mm = ctx->width_mm;
	mode->height_mm = ctx->height_mm;
	connector->display_info.width_mm = ctx->width_mm;
	connector->display_info.height_mm = ctx->height_mm;
	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;

	if (ctx->vm.flags & DISPLAY_FLAGS_DE_HIGH)
		*bus_flags |= DRM_BUS_FLAG_DE_HIGH;
	if (ctx->vm.flags & DISPLAY_FLAGS_DE_LOW)
		*bus_flags |= DRM_BUS_FLAG_DE_LOW;
	if (ctx->vm.flags & DISPLAY_FLAGS_PIXDATA_NEGEDGE)
		*bus_flags |= DRM_BUS_FLAG_PIXDATA_DRIVE_NEGEDGE;
	if (ctx->vm.flags & DISPLAY_FLAGS_PIXDATA_POSEDGE)
		*bus_flags |= DRM_BUS_FLAG_PIXDATA_DRIVE_POSEDGE;

	ret = drm_display_info_set_bus_formats(&connector->display_info,
			dlc1010ebp28mf_bus_formats, ARRAY_SIZE(dlc1010ebp28mf_bus_formats));
	if (ret) {
		dev_err(dev, "failed to set display bus format (error:%d)!\n", ret);
		return ret;
	}

	drm_mode_probed_add(connector, mode);

	return 1;
}

static const struct drm_panel_funcs dlc1010ebp28mf_funcs = {
	.prepare	= dlc1010ebp28mf_prepare,
	.unprepare	= dlc1010ebp28mf_unprepare,
	.enable		= dlc1010ebp28mf_enable,
	.disable	= dlc1010ebp28mf_disable,
	.get_modes	= dlc1010ebp28mf_get_modes,
};

static const struct display_timing dlc1010ebp28mf_default_timing = {
	.pixelclock.typ   = 58000000,
	.hactive.typ      = 800,
	.hfront_porch.typ = 10,
	.hsync_len.typ    = 30,
	.hback_porch.typ  = 10,
	.vactive.typ      = 1280,
	.vfront_porch.typ = 8,
	.vsync_len.typ    = 2,
	.vback_porch.typ  = 18,
	.flags = DISPLAY_FLAGS_HSYNC_LOW |
		 DISPLAY_FLAGS_VSYNC_LOW |
		 DISPLAY_FLAGS_DE_LOW |
		 DISPLAY_FLAGS_PIXDATA_NEGEDGE,
};





	// 	.pixelclock.typ   = 75900000,
	// .hactive.typ      = 800,
	// .hfront_porch.typ = 72,
	// .hsync_len.typ    = 20,
	// .hback_porch.typ  = 68,
	// .vactive.typ      = 1280,
	// .vfront_porch.typ = 15,
	// .vsync_len.typ    = 10,
	// .vback_porch.typ  = 13,
	// .flags = DISPLAY_FLAGS_HSYNC_LOW |
	// 	 DISPLAY_FLAGS_VSYNC_LOW |
	// 	 DISPLAY_FLAGS_DE_LOW |
	// 	 DISPLAY_FLAGS_PIXDATA_NEGEDGE,

static int dlc1010ebp28mf_dsi_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct device_node *np;
	struct dlc1010ebp28mf *ctx;
	int ret;

	ctx = devm_kzalloc(&dsi->dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	mipi_dsi_set_drvdata(dsi, ctx);
	ctx->dsi = dsi;

	dsi->format = MIPI_DSI_FMT_RGB888;
        dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_LPM | MIPI_DSI_MODE_VIDEO_BURST;
	dsi->lanes = 4;

	videomode_from_timing(&dlc1010ebp28mf_default_timing, &ctx->vm);

	ctx->width_mm = 90;
	ctx->height_mm = 152;

	ctx->reset = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);

	if (IS_ERR(ctx->reset))
		ctx->reset = NULL;
	else
		gpiod_set_value(ctx->reset, 0);

	np = of_parse_phandle(dsi->dev.of_node, "backlight", 0);
	if (np) {
		ctx->backlight = of_find_backlight_by_node(np);
		of_node_put(np);

		if (!ctx->backlight)
			return -EPROBE_DEFER;
	}

	drm_panel_init(&ctx->panel, dev, &dlc1010ebp28mf_funcs, DRM_MODE_CONNECTOR_DSI);
	ctx->panel.dev = &dsi->dev;
	ctx->panel.funcs = &dlc1010ebp28mf_funcs;
	dev_set_drvdata(dev, ctx);

	drm_panel_add(&ctx->panel);

	ret = mipi_dsi_attach(dsi);
	if (ret < 0)
		drm_panel_remove(&ctx->panel);

	return ret;
}

static void dlc1010ebp28mf_dsi_remove(struct mipi_dsi_device *dsi)
{
	struct dlc1010ebp28mf *ctx = mipi_dsi_get_drvdata(dsi);

	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);

	if (ctx->backlight)
		put_device(&ctx->backlight->dev);
}

static const struct of_device_id dlc1010ebp28mf_of_match[] = {
	{ .compatible = "dlc,dlc1010ebp28mf" },
	{ }
};
MODULE_DEVICE_TABLE(of, dlc1010ebp28mf_of_match);

static struct mipi_dsi_driver dlc1010ebp28mf_dsi_driver = {
	.probe		= dlc1010ebp28mf_dsi_probe,
	.remove		= dlc1010ebp28mf_dsi_remove,
	.driver = {
		.name		= "dlc1010ebp28mf-dsi",
		.of_match_table	= dlc1010ebp28mf_of_match,
	},
};
module_mipi_dsi_driver(dlc1010ebp28mf_dsi_driver);

MODULE_LICENSE("GPL v2");
