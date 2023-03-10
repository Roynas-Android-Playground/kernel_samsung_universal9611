/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/lcd.h>
#include <linux/backlight.h>
#include <linux/of_device.h>
#include <linux/gpio.h>
#include <video/mipi_display.h>
#include <linux/i2c.h>
#include <linux/module.h>

#include "../decon.h"
#include "../decon_board.h"
#include "../decon_notify.h"
#include "../dsim.h"
#include "dsim_panel.h"

#include "nt36672a_xcoverpro_param.h"
#include "dd.h"

#define PANEL_STATE_SUSPENED	0
#define PANEL_STATE_RESUMED	1
#define PANEL_STATE_SUSPENDING	2

#define NT36672A_ID_REG			0xDA	/* LCD ID1,ID2,ID3 */
#define NT36672A_ID_LEN			3
#define BRIGHTNESS_REG			0x51

#define get_bit(value, shift, width)	((value >> shift) & (GENMASK(width - 1, 0)))

#define DSI_WRITE(cmd, size)		do {				\
	ret = dsim_write_hl_data(lcd, cmd, size);			\
	if (ret < 0)							\
		dev_info(&lcd->ld->dev, "%s: failed to write %s\n", __func__, #cmd);	\
} while (0)

struct lcd_info {
	unsigned int			connected;
	unsigned int			brightness;
	unsigned int			state;

	struct lcd_device		*ld;
	struct backlight_device		*bd;

	union {
		struct {
			u8		reserved;
			u8		id[NT36672A_ID_LEN];
		};
		u32			value;
	} id_info;

	int				lux;

	struct dsim_device		*dsim;
	struct mutex			lock;

	struct notifier_block		fb_notifier;
	struct i2c_client		*blic_client;
	unsigned int			conn_init_done;
	unsigned int			conn_det_enable;
	unsigned int			conn_det_count;

	struct workqueue_struct		*conn_workqueue;
	struct work_struct		conn_work;
};


static int dsim_write_hl_data(struct lcd_info *lcd, const u8 *cmd, u32 cmdsize)
{
	int ret = 0;
	int retry = 2;

	if (!lcd->connected)
		return ret;

try_write:
	if (cmdsize == 1)
		ret = dsim_write_data(lcd->dsim, MIPI_DSI_DCS_SHORT_WRITE, cmd[0], 0);
	else if (cmdsize == 2)
		ret = dsim_write_data(lcd->dsim, MIPI_DSI_DCS_SHORT_WRITE_PARAM, cmd[0], cmd[1]);
	else
		ret = dsim_write_data(lcd->dsim, MIPI_DSI_DCS_LONG_WRITE, (unsigned long)cmd, cmdsize);

	if (ret < 0) {
		if (--retry)
			goto try_write;
		else
			dev_info(&lcd->ld->dev, "%s: fail. %02x, ret: %d\n", __func__, cmd[0], ret);
	}

	return ret;
}

static int dsi_write_table(struct lcd_info *lcd, const u8 *table, int size)
{
	int i = 0, ret = 0;

	for (i = 0; i < size / 2; i++) {
		ret = dsim_write_hl_data(lcd, table + (i * 2), 2);

		if (ret < 0) {
			dev_info(&lcd->ld->dev, "%s: failed to write CMD : 0x%02x, 0x%02x\n", __func__, table[i * 2], table[i * 2 + 1]);
			goto write_exit;
		}
	}

write_exit:
	return ret;
}

#if defined(CONFIG_SEC_FACTORY)
static int dsim_read_hl_data(struct lcd_info *lcd, u8 addr, u32 size, u8 *buf)
{
	int ret = 0, rx_size = 0;
	int retry = 2;

	if (!lcd->connected)
		return ret;

try_read:
	rx_size = dsim_read_data(lcd->dsim, MIPI_DSI_DCS_READ, (u32)addr, size, buf);
	dev_info(&lcd->ld->dev, "%s: %2d(%2d), %02x, %*ph%s\n", __func__, size, rx_size, addr,
		min_t(u32, min_t(u32, size, rx_size), 5), buf, (rx_size > 5) ? "..." : "");
	if (rx_size != size) {
		if (--retry)
			goto try_read;
		else {
			dev_info(&lcd->ld->dev, "%s: fail. %02x, %d(%d)\n", __func__, addr, size, rx_size);
			ret = -EPERM;
		}
	}

	return ret;
}
#endif

static int lm36274_array_write(struct i2c_client *client, u8 *ptr, u8 len)
{
	unsigned int i = 0;
	int ret = 0;
	u8 command = 0, value = 0;
	struct lcd_info *lcd = NULL;

	if (!client)
		return ret;

	lcd = i2c_get_clientdata(client);
	if (!lcd)
		return ret;

	if (!lcdtype) {
		dev_info(&lcd->ld->dev, "%s: lcdtype: %d\n", __func__, lcdtype);
		return ret;
	}

	if (len % 2) {
		dev_info(&lcd->ld->dev, "%s: length(%d) invalid\n", __func__, len);
		return ret;
	}

	for (i = 0; i < len; i += 2) {
		command = ptr[i + 0];
		value = ptr[i + 1];

		ret = i2c_smbus_write_byte_data(client, command, value);
		if (ret < 0)
			dev_info(&lcd->ld->dev, "%s: fail. %2x, %2x, %d\n", __func__, command, value, ret);
	}

	return ret;
}

static int dsim_panel_set_brightness(struct lcd_info *lcd, int force)
{
	int ret = 0;
	unsigned char bl_reg[3] = {0, };

	mutex_lock(&lcd->lock);

	lcd->brightness = lcd->bd->props.brightness;

	if (!force && lcd->state != PANEL_STATE_RESUMED) {
		dev_info(&lcd->ld->dev, "%s: panel is not active state\n", __func__);
		goto exit;
	}

	bl_reg[0] = BRIGHTNESS_REG;
	bl_reg[1] = (brightness_table[lcd->brightness] >> 8) & 0x0F;
	bl_reg[2] = brightness_table[lcd->brightness] & 0xFF;

	DSI_WRITE(bl_reg, ARRAY_SIZE(bl_reg));

	dev_info(&lcd->ld->dev, "%s: brightness: %3d, %4d(%2x%2x), lx: %d\n", __func__,
		lcd->brightness, brightness_table[lcd->brightness], bl_reg[1], bl_reg[2], lcd->lux);

exit:
	mutex_unlock(&lcd->lock);

	return ret;
}

static int panel_get_brightness(struct backlight_device *bd)
{
	struct lcd_info *lcd = bl_get_data(bd);

	return brightness_table[lcd->brightness];
}

static int panel_set_brightness(struct backlight_device *bd)
{
	int ret = 0;
	struct lcd_info *lcd = bl_get_data(bd);

	if (lcd->state == PANEL_STATE_RESUMED) {
		ret = dsim_panel_set_brightness(lcd, 0);
		if (ret < 0)
			dev_info(&lcd->ld->dev, "%s: failed to set brightness\n", __func__);
	}

	return ret;
}

static int panel_check_fb(struct backlight_device *bd, struct fb_info *fb)
{
	return 0;
}

static const struct backlight_ops panel_backlight_ops = {
	.get_brightness = panel_get_brightness,
	.update_status = panel_set_brightness,
	.check_fb = panel_check_fb,
};

static int nt36672a_read_init_info(struct lcd_info *lcd)
{
	struct panel_private *priv = &lcd->dsim->priv;

	priv->lcdconnected = lcd->connected = lcdtype ? 1 : 0;

	lcd->id_info.id[0] = (lcdtype & 0xFF0000) >> 16;
	lcd->id_info.id[1] = (lcdtype & 0x00FF00) >> 8;
	lcd->id_info.id[2] = (lcdtype & 0x0000FF) >> 0;

	dev_info(&lcd->ld->dev, "%s: %x\n", __func__, cpu_to_be32(lcd->id_info.value));

	return 0;
}

#if defined(CONFIG_SEC_FACTORY)
static int dsim_read_info(struct lcd_info *lcd, u8 reg, u32 len, u8 *buf)
{
	int ret = 0, i;

	ret = dsim_read_hl_data(lcd, reg, len, buf);
	if (ret < 0) {
		dev_info(&lcd->ld->dev, "%s: fail. %02x, ret: %d\n", __func__, reg, ret);
		goto exit;
	}

	dev_dbg(&lcd->ld->dev, "%s: %02xh\n", __func__, reg);
	for (i = 0; i < len; i++)
		dev_dbg(&lcd->ld->dev, "%02dth value is %02x, %3d\n", i + 1, buf[i], buf[i]);

exit:
	return ret;
}

static int nt36672a_read_id(struct lcd_info *lcd)
{
	struct panel_private *priv = &lcd->dsim->priv;
	int i, ret = 0;
	struct decon_device *decon = get_decon_drvdata(0);
	static char *LDI_BIT_DESC_ID[BITS_PER_BYTE * NT36672A_ID_LEN] = {
		[0 ... 23] = "ID Read Fail",
	};

	lcd->id_info.value = 0;
	priv->lcdconnected = lcd->connected = lcdtype ? 1 : 0;

	for (i = 0; i < NT36672A_ID_LEN; i++) {
		ret = dsim_read_info(lcd, NT36672A_ID_REG + i, 1, &lcd->id_info.id[i]);
		if (ret < 0)
			break;
	}

	if (ret < 0 || !lcd->id_info.value) {
		priv->lcdconnected = lcd->connected = 0;
		dev_info(&lcd->ld->dev, "%s: connected lcd is invalid\n", __func__);

		if (lcdtype && decon)
			decon_abd_save_bit(&decon->abd, BITS_PER_BYTE * NT36672A_ID_LEN, cpu_to_be32(lcd->id_info.value), LDI_BIT_DESC_ID);
	}

	dev_info(&lcd->ld->dev, "%s: %x\n", __func__, cpu_to_be32(lcd->id_info.value));

	return ret;

}
#endif

static int nt36672a_displayon_late(struct lcd_info *lcd)
{
	int ret = 0;

	dev_info(&lcd->ld->dev, "%s\n", __func__);

	DSI_WRITE(SEQ_DISPLAY_ON, ARRAY_SIZE(SEQ_DISPLAY_ON));

	return ret;
}

static int nt36672a_exit(struct lcd_info *lcd)
{
	int ret = 0;

	dev_info(&lcd->ld->dev, "%s\n", __func__);

	DSI_WRITE(SEQ_DISPLAY_OFF, ARRAY_SIZE(SEQ_DISPLAY_OFF));
	msleep(20);
	DSI_WRITE(SEQ_SLEEP_IN, ARRAY_SIZE(SEQ_SLEEP_IN));

	return ret;
}

static int nt36672a_init(struct lcd_info *lcd)
{
	int ret = 0;

	dev_info(&lcd->ld->dev, "%s: ++\n", __func__);

	dsi_write_table(lcd, SEQ_INIT_1_TABLE, ARRAY_SIZE(SEQ_INIT_1_TABLE));

	DSI_WRITE(SEQ_R_N_1, ARRAY_SIZE(SEQ_R_N_1));
	DSI_WRITE(SEQ_R_N_2, ARRAY_SIZE(SEQ_R_N_2));
	DSI_WRITE(SEQ_R_N_3, ARRAY_SIZE(SEQ_R_N_3));
	DSI_WRITE(SEQ_R_N_4, ARRAY_SIZE(SEQ_R_N_4));
	DSI_WRITE(SEQ_G_N_1, ARRAY_SIZE(SEQ_G_N_1));
	DSI_WRITE(SEQ_G_N_2, ARRAY_SIZE(SEQ_G_N_2));
	DSI_WRITE(SEQ_G_N_3, ARRAY_SIZE(SEQ_G_N_3));
	DSI_WRITE(SEQ_G_N_4, ARRAY_SIZE(SEQ_G_N_4));
	DSI_WRITE(SEQ_B_N_1, ARRAY_SIZE(SEQ_B_N_1));
	DSI_WRITE(SEQ_B_N_2, ARRAY_SIZE(SEQ_B_N_2));
	DSI_WRITE(SEQ_B_N_3, ARRAY_SIZE(SEQ_B_N_3));
	DSI_WRITE(SEQ_B_N_4, ARRAY_SIZE(SEQ_B_N_4));

	dsi_write_table(lcd, SEQ_INIT_2_TABLE, ARRAY_SIZE(SEQ_INIT_2_TABLE));

	DSI_WRITE(SEQ_R_P_1, ARRAY_SIZE(SEQ_R_P_1));
	DSI_WRITE(SEQ_R_P_2, ARRAY_SIZE(SEQ_R_P_2));
	DSI_WRITE(SEQ_R_P_3, ARRAY_SIZE(SEQ_R_P_3));
	DSI_WRITE(SEQ_R_P_4, ARRAY_SIZE(SEQ_R_P_4));
	DSI_WRITE(SEQ_G_P_1, ARRAY_SIZE(SEQ_G_P_1));
	DSI_WRITE(SEQ_G_P_2, ARRAY_SIZE(SEQ_G_P_2));
	DSI_WRITE(SEQ_G_P_3, ARRAY_SIZE(SEQ_G_P_3));
	DSI_WRITE(SEQ_G_P_4, ARRAY_SIZE(SEQ_G_P_4));
	DSI_WRITE(SEQ_B_P_1, ARRAY_SIZE(SEQ_B_P_1));
	DSI_WRITE(SEQ_B_P_2, ARRAY_SIZE(SEQ_B_P_2));
	DSI_WRITE(SEQ_B_P_3, ARRAY_SIZE(SEQ_B_P_3));
	DSI_WRITE(SEQ_B_P_4, ARRAY_SIZE(SEQ_B_P_4));

	dsi_write_table(lcd, SEQ_INIT_3_TABLE, ARRAY_SIZE(SEQ_INIT_3_TABLE));

	DSI_WRITE(SEQ_DEFAULT_BRIGHTNESS, ARRAY_SIZE(SEQ_DEFAULT_BRIGHTNESS));
	DSI_WRITE(SEQ_DEFAULT_BRIGHTNESS_DIMMING, ARRAY_SIZE(SEQ_DEFAULT_BRIGHTNESS_DIMMING));

	dsi_write_table(lcd, SEQ_INIT_4_TABLE, ARRAY_SIZE(SEQ_INIT_4_TABLE));

	DSI_WRITE(SEQ_SLEEP_OUT, ARRAY_SIZE(SEQ_SLEEP_OUT));

	msleep(120);
	dev_info(&lcd->ld->dev, "%s: --\n", __func__);

	return ret;
}

static int nt36672c_init(struct lcd_info *lcd)
{
	int ret = 0;

	dev_info(&lcd->ld->dev, "%s: ++\n", __func__);

	DSI_WRITE(SEQ_NT36672C_001, ARRAY_SIZE(SEQ_NT36672C_001));
	DSI_WRITE(SEQ_NT36672C_002, ARRAY_SIZE(SEQ_NT36672C_002));
	DSI_WRITE(SEQ_NT36672C_003, ARRAY_SIZE(SEQ_NT36672C_003));
	DSI_WRITE(SEQ_NT36672C_004, ARRAY_SIZE(SEQ_NT36672C_004));
	DSI_WRITE(SEQ_NT36672C_005, ARRAY_SIZE(SEQ_NT36672C_005));
	DSI_WRITE(SEQ_NT36672C_006, ARRAY_SIZE(SEQ_NT36672C_006));
	DSI_WRITE(SEQ_NT36672C_007, ARRAY_SIZE(SEQ_NT36672C_007));
	DSI_WRITE(SEQ_NT36672C_008, ARRAY_SIZE(SEQ_NT36672C_008));
	DSI_WRITE(SEQ_NT36672C_009, ARRAY_SIZE(SEQ_NT36672C_009));
	DSI_WRITE(SEQ_NT36672C_010, ARRAY_SIZE(SEQ_NT36672C_010));
	DSI_WRITE(SEQ_NT36672C_011, ARRAY_SIZE(SEQ_NT36672C_011));
	DSI_WRITE(SEQ_NT36672C_012, ARRAY_SIZE(SEQ_NT36672C_012));
	DSI_WRITE(SEQ_NT36672C_013, ARRAY_SIZE(SEQ_NT36672C_013));
	DSI_WRITE(SEQ_NT36672C_014, ARRAY_SIZE(SEQ_NT36672C_014));
	DSI_WRITE(SEQ_NT36672C_015, ARRAY_SIZE(SEQ_NT36672C_015));
	DSI_WRITE(SEQ_NT36672C_016, ARRAY_SIZE(SEQ_NT36672C_016));
	DSI_WRITE(SEQ_NT36672C_017, ARRAY_SIZE(SEQ_NT36672C_017));
	DSI_WRITE(SEQ_NT36672C_018, ARRAY_SIZE(SEQ_NT36672C_018));
	DSI_WRITE(SEQ_NT36672C_019, ARRAY_SIZE(SEQ_NT36672C_019));
	DSI_WRITE(SEQ_NT36672C_020, ARRAY_SIZE(SEQ_NT36672C_020));
	DSI_WRITE(SEQ_NT36672C_021, ARRAY_SIZE(SEQ_NT36672C_021));
	DSI_WRITE(SEQ_NT36672C_022, ARRAY_SIZE(SEQ_NT36672C_022));
	DSI_WRITE(SEQ_NT36672C_023, ARRAY_SIZE(SEQ_NT36672C_023));
	DSI_WRITE(SEQ_NT36672C_024, ARRAY_SIZE(SEQ_NT36672C_024));
	DSI_WRITE(SEQ_NT36672C_025, ARRAY_SIZE(SEQ_NT36672C_025));
	DSI_WRITE(SEQ_NT36672C_026, ARRAY_SIZE(SEQ_NT36672C_026));
	DSI_WRITE(SEQ_NT36672C_027, ARRAY_SIZE(SEQ_NT36672C_027));
	DSI_WRITE(SEQ_NT36672C_028, ARRAY_SIZE(SEQ_NT36672C_028));
	DSI_WRITE(SEQ_NT36672C_029, ARRAY_SIZE(SEQ_NT36672C_029));
	DSI_WRITE(SEQ_NT36672C_030, ARRAY_SIZE(SEQ_NT36672C_030));
	DSI_WRITE(SEQ_NT36672C_031, ARRAY_SIZE(SEQ_NT36672C_031));
	DSI_WRITE(SEQ_NT36672C_032, ARRAY_SIZE(SEQ_NT36672C_032));
	DSI_WRITE(SEQ_NT36672C_033, ARRAY_SIZE(SEQ_NT36672C_033));
	DSI_WRITE(SEQ_NT36672C_034, ARRAY_SIZE(SEQ_NT36672C_034));
	DSI_WRITE(SEQ_NT36672C_035, ARRAY_SIZE(SEQ_NT36672C_035));
	DSI_WRITE(SEQ_NT36672C_036, ARRAY_SIZE(SEQ_NT36672C_036));
	DSI_WRITE(SEQ_NT36672C_037, ARRAY_SIZE(SEQ_NT36672C_037));
	DSI_WRITE(SEQ_NT36672C_038, ARRAY_SIZE(SEQ_NT36672C_038));
	DSI_WRITE(SEQ_NT36672C_039, ARRAY_SIZE(SEQ_NT36672C_039));
	DSI_WRITE(SEQ_NT36672C_040, ARRAY_SIZE(SEQ_NT36672C_040));
	DSI_WRITE(SEQ_NT36672C_041, ARRAY_SIZE(SEQ_NT36672C_041));
	DSI_WRITE(SEQ_NT36672C_042, ARRAY_SIZE(SEQ_NT36672C_042));
	DSI_WRITE(SEQ_NT36672C_043, ARRAY_SIZE(SEQ_NT36672C_043));
	DSI_WRITE(SEQ_NT36672C_044, ARRAY_SIZE(SEQ_NT36672C_044));
	DSI_WRITE(SEQ_NT36672C_045, ARRAY_SIZE(SEQ_NT36672C_045));
	DSI_WRITE(SEQ_NT36672C_046, ARRAY_SIZE(SEQ_NT36672C_046));
	DSI_WRITE(SEQ_NT36672C_047, ARRAY_SIZE(SEQ_NT36672C_047));
	DSI_WRITE(SEQ_NT36672C_048, ARRAY_SIZE(SEQ_NT36672C_048));
	DSI_WRITE(SEQ_NT36672C_049, ARRAY_SIZE(SEQ_NT36672C_049));
	DSI_WRITE(SEQ_NT36672C_050, ARRAY_SIZE(SEQ_NT36672C_050));
	DSI_WRITE(SEQ_NT36672C_051, ARRAY_SIZE(SEQ_NT36672C_051));
	DSI_WRITE(SEQ_NT36672C_052, ARRAY_SIZE(SEQ_NT36672C_052));
	DSI_WRITE(SEQ_NT36672C_053, ARRAY_SIZE(SEQ_NT36672C_053));
	DSI_WRITE(SEQ_NT36672C_054, ARRAY_SIZE(SEQ_NT36672C_054));
	DSI_WRITE(SEQ_NT36672C_055, ARRAY_SIZE(SEQ_NT36672C_055));
	DSI_WRITE(SEQ_NT36672C_056, ARRAY_SIZE(SEQ_NT36672C_056));
	DSI_WRITE(SEQ_NT36672C_057, ARRAY_SIZE(SEQ_NT36672C_057));
	DSI_WRITE(SEQ_NT36672C_058, ARRAY_SIZE(SEQ_NT36672C_058));
	DSI_WRITE(SEQ_NT36672C_059, ARRAY_SIZE(SEQ_NT36672C_059));
	DSI_WRITE(SEQ_NT36672C_060, ARRAY_SIZE(SEQ_NT36672C_060));
	DSI_WRITE(SEQ_NT36672C_061, ARRAY_SIZE(SEQ_NT36672C_061));
	DSI_WRITE(SEQ_NT36672C_062, ARRAY_SIZE(SEQ_NT36672C_062));
	DSI_WRITE(SEQ_NT36672C_063, ARRAY_SIZE(SEQ_NT36672C_063));
	DSI_WRITE(SEQ_NT36672C_064, ARRAY_SIZE(SEQ_NT36672C_064));
	DSI_WRITE(SEQ_NT36672C_065, ARRAY_SIZE(SEQ_NT36672C_065));
	DSI_WRITE(SEQ_NT36672C_066, ARRAY_SIZE(SEQ_NT36672C_066));
	DSI_WRITE(SEQ_NT36672C_067, ARRAY_SIZE(SEQ_NT36672C_067));
	DSI_WRITE(SEQ_NT36672C_068, ARRAY_SIZE(SEQ_NT36672C_068));
	DSI_WRITE(SEQ_NT36672C_069, ARRAY_SIZE(SEQ_NT36672C_069));
	DSI_WRITE(SEQ_NT36672C_070, ARRAY_SIZE(SEQ_NT36672C_070));
	DSI_WRITE(SEQ_NT36672C_071, ARRAY_SIZE(SEQ_NT36672C_071));
	DSI_WRITE(SEQ_NT36672C_072, ARRAY_SIZE(SEQ_NT36672C_072));
	DSI_WRITE(SEQ_NT36672C_073, ARRAY_SIZE(SEQ_NT36672C_073));
	DSI_WRITE(SEQ_NT36672C_074, ARRAY_SIZE(SEQ_NT36672C_074));
	DSI_WRITE(SEQ_NT36672C_075, ARRAY_SIZE(SEQ_NT36672C_075));
	DSI_WRITE(SEQ_NT36672C_076, ARRAY_SIZE(SEQ_NT36672C_076));
	DSI_WRITE(SEQ_NT36672C_077, ARRAY_SIZE(SEQ_NT36672C_077));
	DSI_WRITE(SEQ_NT36672C_078, ARRAY_SIZE(SEQ_NT36672C_078));
	DSI_WRITE(SEQ_NT36672C_079, ARRAY_SIZE(SEQ_NT36672C_079));
	DSI_WRITE(SEQ_NT36672C_080, ARRAY_SIZE(SEQ_NT36672C_080));
	DSI_WRITE(SEQ_NT36672C_081, ARRAY_SIZE(SEQ_NT36672C_081));
	DSI_WRITE(SEQ_NT36672C_082, ARRAY_SIZE(SEQ_NT36672C_082));
	DSI_WRITE(SEQ_NT36672C_083, ARRAY_SIZE(SEQ_NT36672C_083));
	DSI_WRITE(SEQ_NT36672C_084, ARRAY_SIZE(SEQ_NT36672C_084));
	DSI_WRITE(SEQ_NT36672C_085, ARRAY_SIZE(SEQ_NT36672C_085));
	DSI_WRITE(SEQ_NT36672C_086, ARRAY_SIZE(SEQ_NT36672C_086));
	DSI_WRITE(SEQ_NT36672C_087, ARRAY_SIZE(SEQ_NT36672C_087));
	DSI_WRITE(SEQ_NT36672C_088, ARRAY_SIZE(SEQ_NT36672C_088));
	DSI_WRITE(SEQ_NT36672C_089, ARRAY_SIZE(SEQ_NT36672C_089));
	DSI_WRITE(SEQ_NT36672C_090, ARRAY_SIZE(SEQ_NT36672C_090));
	DSI_WRITE(SEQ_NT36672C_091, ARRAY_SIZE(SEQ_NT36672C_091));
	DSI_WRITE(SEQ_NT36672C_092, ARRAY_SIZE(SEQ_NT36672C_092));
	DSI_WRITE(SEQ_NT36672C_093, ARRAY_SIZE(SEQ_NT36672C_093));
	DSI_WRITE(SEQ_NT36672C_094, ARRAY_SIZE(SEQ_NT36672C_094));
	DSI_WRITE(SEQ_NT36672C_095, ARRAY_SIZE(SEQ_NT36672C_095));
	DSI_WRITE(SEQ_NT36672C_096, ARRAY_SIZE(SEQ_NT36672C_096));
	DSI_WRITE(SEQ_NT36672C_097, ARRAY_SIZE(SEQ_NT36672C_097));
	DSI_WRITE(SEQ_NT36672C_098, ARRAY_SIZE(SEQ_NT36672C_098));
	DSI_WRITE(SEQ_NT36672C_099, ARRAY_SIZE(SEQ_NT36672C_099));
	DSI_WRITE(SEQ_NT36672C_100, ARRAY_SIZE(SEQ_NT36672C_100));
	DSI_WRITE(SEQ_NT36672C_101, ARRAY_SIZE(SEQ_NT36672C_101));
	DSI_WRITE(SEQ_NT36672C_102, ARRAY_SIZE(SEQ_NT36672C_102));
	DSI_WRITE(SEQ_NT36672C_103, ARRAY_SIZE(SEQ_NT36672C_103));
	DSI_WRITE(SEQ_NT36672C_104, ARRAY_SIZE(SEQ_NT36672C_104));
	DSI_WRITE(SEQ_NT36672C_105, ARRAY_SIZE(SEQ_NT36672C_105));
	DSI_WRITE(SEQ_NT36672C_106, ARRAY_SIZE(SEQ_NT36672C_106));
	DSI_WRITE(SEQ_NT36672C_107, ARRAY_SIZE(SEQ_NT36672C_107));
	DSI_WRITE(SEQ_NT36672C_108, ARRAY_SIZE(SEQ_NT36672C_108));
	DSI_WRITE(SEQ_NT36672C_109, ARRAY_SIZE(SEQ_NT36672C_109));
	DSI_WRITE(SEQ_NT36672C_110, ARRAY_SIZE(SEQ_NT36672C_110));
	DSI_WRITE(SEQ_NT36672C_111, ARRAY_SIZE(SEQ_NT36672C_111));
	DSI_WRITE(SEQ_NT36672C_112, ARRAY_SIZE(SEQ_NT36672C_112));
	DSI_WRITE(SEQ_NT36672C_113, ARRAY_SIZE(SEQ_NT36672C_113));
	DSI_WRITE(SEQ_NT36672C_114, ARRAY_SIZE(SEQ_NT36672C_114));
	DSI_WRITE(SEQ_NT36672C_115, ARRAY_SIZE(SEQ_NT36672C_115));
	DSI_WRITE(SEQ_NT36672C_116, ARRAY_SIZE(SEQ_NT36672C_116));
	DSI_WRITE(SEQ_NT36672C_117, ARRAY_SIZE(SEQ_NT36672C_117));
	DSI_WRITE(SEQ_NT36672C_118, ARRAY_SIZE(SEQ_NT36672C_118));
	DSI_WRITE(SEQ_NT36672C_119, ARRAY_SIZE(SEQ_NT36672C_119));
	DSI_WRITE(SEQ_NT36672C_120, ARRAY_SIZE(SEQ_NT36672C_120));
	DSI_WRITE(SEQ_NT36672C_121, ARRAY_SIZE(SEQ_NT36672C_121));
	DSI_WRITE(SEQ_NT36672C_122, ARRAY_SIZE(SEQ_NT36672C_122));
	DSI_WRITE(SEQ_NT36672C_123, ARRAY_SIZE(SEQ_NT36672C_123));
	DSI_WRITE(SEQ_NT36672C_124, ARRAY_SIZE(SEQ_NT36672C_124));
	DSI_WRITE(SEQ_NT36672C_125, ARRAY_SIZE(SEQ_NT36672C_125));
	DSI_WRITE(SEQ_NT36672C_126, ARRAY_SIZE(SEQ_NT36672C_126));
	DSI_WRITE(SEQ_NT36672C_127, ARRAY_SIZE(SEQ_NT36672C_127));
	DSI_WRITE(SEQ_NT36672C_128, ARRAY_SIZE(SEQ_NT36672C_128));
	DSI_WRITE(SEQ_NT36672C_129, ARRAY_SIZE(SEQ_NT36672C_129));
	DSI_WRITE(SEQ_NT36672C_130, ARRAY_SIZE(SEQ_NT36672C_130));
	DSI_WRITE(SEQ_NT36672C_131, ARRAY_SIZE(SEQ_NT36672C_131));
	DSI_WRITE(SEQ_NT36672C_132, ARRAY_SIZE(SEQ_NT36672C_132));
	DSI_WRITE(SEQ_NT36672C_133, ARRAY_SIZE(SEQ_NT36672C_133));
	DSI_WRITE(SEQ_NT36672C_134, ARRAY_SIZE(SEQ_NT36672C_134));
	DSI_WRITE(SEQ_NT36672C_135, ARRAY_SIZE(SEQ_NT36672C_135));
	DSI_WRITE(SEQ_NT36672C_136, ARRAY_SIZE(SEQ_NT36672C_136));
	DSI_WRITE(SEQ_NT36672C_137, ARRAY_SIZE(SEQ_NT36672C_137));
	DSI_WRITE(SEQ_NT36672C_138, ARRAY_SIZE(SEQ_NT36672C_138));
	DSI_WRITE(SEQ_NT36672C_139, ARRAY_SIZE(SEQ_NT36672C_139));
	DSI_WRITE(SEQ_NT36672C_140, ARRAY_SIZE(SEQ_NT36672C_140));
	DSI_WRITE(SEQ_NT36672C_141, ARRAY_SIZE(SEQ_NT36672C_141));
	DSI_WRITE(SEQ_NT36672C_142, ARRAY_SIZE(SEQ_NT36672C_142));
	DSI_WRITE(SEQ_NT36672C_143, ARRAY_SIZE(SEQ_NT36672C_143));
	DSI_WRITE(SEQ_NT36672C_144, ARRAY_SIZE(SEQ_NT36672C_144));
	DSI_WRITE(SEQ_NT36672C_145, ARRAY_SIZE(SEQ_NT36672C_145));
	DSI_WRITE(SEQ_NT36672C_146, ARRAY_SIZE(SEQ_NT36672C_146));
	DSI_WRITE(SEQ_NT36672C_147, ARRAY_SIZE(SEQ_NT36672C_147));
	DSI_WRITE(SEQ_NT36672C_148, ARRAY_SIZE(SEQ_NT36672C_148));
	DSI_WRITE(SEQ_NT36672C_149, ARRAY_SIZE(SEQ_NT36672C_149));
	DSI_WRITE(SEQ_NT36672C_150, ARRAY_SIZE(SEQ_NT36672C_150));
	DSI_WRITE(SEQ_NT36672C_151, ARRAY_SIZE(SEQ_NT36672C_151));
	DSI_WRITE(SEQ_NT36672C_152, ARRAY_SIZE(SEQ_NT36672C_152));
	DSI_WRITE(SEQ_NT36672C_153, ARRAY_SIZE(SEQ_NT36672C_153));
	DSI_WRITE(SEQ_NT36672C_154, ARRAY_SIZE(SEQ_NT36672C_154));
	DSI_WRITE(SEQ_NT36672C_155, ARRAY_SIZE(SEQ_NT36672C_155));
	DSI_WRITE(SEQ_NT36672C_156, ARRAY_SIZE(SEQ_NT36672C_156));
	DSI_WRITE(SEQ_NT36672C_157, ARRAY_SIZE(SEQ_NT36672C_157));
	DSI_WRITE(SEQ_NT36672C_158, ARRAY_SIZE(SEQ_NT36672C_158));
	DSI_WRITE(SEQ_NT36672C_159, ARRAY_SIZE(SEQ_NT36672C_159));
	DSI_WRITE(SEQ_NT36672C_160, ARRAY_SIZE(SEQ_NT36672C_160));
	DSI_WRITE(SEQ_NT36672C_161, ARRAY_SIZE(SEQ_NT36672C_161));
	DSI_WRITE(SEQ_NT36672C_162, ARRAY_SIZE(SEQ_NT36672C_162));
	DSI_WRITE(SEQ_NT36672C_163, ARRAY_SIZE(SEQ_NT36672C_163));
	DSI_WRITE(SEQ_NT36672C_164, ARRAY_SIZE(SEQ_NT36672C_164));
	DSI_WRITE(SEQ_NT36672C_165, ARRAY_SIZE(SEQ_NT36672C_165));
	DSI_WRITE(SEQ_NT36672C_166, ARRAY_SIZE(SEQ_NT36672C_166));
	DSI_WRITE(SEQ_NT36672C_167, ARRAY_SIZE(SEQ_NT36672C_167));
	DSI_WRITE(SEQ_NT36672C_168, ARRAY_SIZE(SEQ_NT36672C_168));
	DSI_WRITE(SEQ_NT36672C_169, ARRAY_SIZE(SEQ_NT36672C_169));
	DSI_WRITE(SEQ_NT36672C_170, ARRAY_SIZE(SEQ_NT36672C_170));
	DSI_WRITE(SEQ_NT36672C_171, ARRAY_SIZE(SEQ_NT36672C_171));
	DSI_WRITE(SEQ_NT36672C_172, ARRAY_SIZE(SEQ_NT36672C_172));
	DSI_WRITE(SEQ_NT36672C_173, ARRAY_SIZE(SEQ_NT36672C_173));
	DSI_WRITE(SEQ_NT36672C_174, ARRAY_SIZE(SEQ_NT36672C_174));
	DSI_WRITE(SEQ_NT36672C_175, ARRAY_SIZE(SEQ_NT36672C_175));
	DSI_WRITE(SEQ_NT36672C_176, ARRAY_SIZE(SEQ_NT36672C_176));
	DSI_WRITE(SEQ_NT36672C_177, ARRAY_SIZE(SEQ_NT36672C_177));
	DSI_WRITE(SEQ_NT36672C_178, ARRAY_SIZE(SEQ_NT36672C_178));
	DSI_WRITE(SEQ_NT36672C_179, ARRAY_SIZE(SEQ_NT36672C_179));
	DSI_WRITE(SEQ_NT36672C_180, ARRAY_SIZE(SEQ_NT36672C_180));
	DSI_WRITE(SEQ_NT36672C_181, ARRAY_SIZE(SEQ_NT36672C_181));
	DSI_WRITE(SEQ_NT36672C_182, ARRAY_SIZE(SEQ_NT36672C_182));
	DSI_WRITE(SEQ_NT36672C_183, ARRAY_SIZE(SEQ_NT36672C_183));
	DSI_WRITE(SEQ_NT36672C_184, ARRAY_SIZE(SEQ_NT36672C_184));
	DSI_WRITE(SEQ_NT36672C_185, ARRAY_SIZE(SEQ_NT36672C_185));
	DSI_WRITE(SEQ_NT36672C_186, ARRAY_SIZE(SEQ_NT36672C_186));
	DSI_WRITE(SEQ_NT36672C_187, ARRAY_SIZE(SEQ_NT36672C_187));
	DSI_WRITE(SEQ_NT36672C_188, ARRAY_SIZE(SEQ_NT36672C_188));
	DSI_WRITE(SEQ_NT36672C_189, ARRAY_SIZE(SEQ_NT36672C_189));
	DSI_WRITE(SEQ_NT36672C_190, ARRAY_SIZE(SEQ_NT36672C_190));
	DSI_WRITE(SEQ_NT36672C_191, ARRAY_SIZE(SEQ_NT36672C_191));
	DSI_WRITE(SEQ_NT36672C_192, ARRAY_SIZE(SEQ_NT36672C_192));
	DSI_WRITE(SEQ_NT36672C_193, ARRAY_SIZE(SEQ_NT36672C_193));
	DSI_WRITE(SEQ_NT36672C_194, ARRAY_SIZE(SEQ_NT36672C_194));
	DSI_WRITE(SEQ_NT36672C_195, ARRAY_SIZE(SEQ_NT36672C_195));
	DSI_WRITE(SEQ_NT36672C_196, ARRAY_SIZE(SEQ_NT36672C_196));
	DSI_WRITE(SEQ_NT36672C_197, ARRAY_SIZE(SEQ_NT36672C_197));
	DSI_WRITE(SEQ_NT36672C_198, ARRAY_SIZE(SEQ_NT36672C_198));
	DSI_WRITE(SEQ_NT36672C_199, ARRAY_SIZE(SEQ_NT36672C_199));
	DSI_WRITE(SEQ_NT36672C_200, ARRAY_SIZE(SEQ_NT36672C_200));
	DSI_WRITE(SEQ_NT36672C_201, ARRAY_SIZE(SEQ_NT36672C_201));
	DSI_WRITE(SEQ_NT36672C_202, ARRAY_SIZE(SEQ_NT36672C_202));
	DSI_WRITE(SEQ_NT36672C_203, ARRAY_SIZE(SEQ_NT36672C_203));
	DSI_WRITE(SEQ_NT36672C_204, ARRAY_SIZE(SEQ_NT36672C_204));
	DSI_WRITE(SEQ_NT36672C_205, ARRAY_SIZE(SEQ_NT36672C_205));
	DSI_WRITE(SEQ_NT36672C_206, ARRAY_SIZE(SEQ_NT36672C_206));
	DSI_WRITE(SEQ_NT36672C_207, ARRAY_SIZE(SEQ_NT36672C_207));
	DSI_WRITE(SEQ_NT36672C_208, ARRAY_SIZE(SEQ_NT36672C_208));
	DSI_WRITE(SEQ_NT36672C_209, ARRAY_SIZE(SEQ_NT36672C_209));
	DSI_WRITE(SEQ_NT36672C_210, ARRAY_SIZE(SEQ_NT36672C_210));
	DSI_WRITE(SEQ_NT36672C_211, ARRAY_SIZE(SEQ_NT36672C_211));
	DSI_WRITE(SEQ_NT36672C_212, ARRAY_SIZE(SEQ_NT36672C_212));
	DSI_WRITE(SEQ_NT36672C_213, ARRAY_SIZE(SEQ_NT36672C_213));
	DSI_WRITE(SEQ_NT36672C_214, ARRAY_SIZE(SEQ_NT36672C_214));
	DSI_WRITE(SEQ_NT36672C_215, ARRAY_SIZE(SEQ_NT36672C_215));
	DSI_WRITE(SEQ_NT36672C_216, ARRAY_SIZE(SEQ_NT36672C_216));
	DSI_WRITE(SEQ_NT36672C_217, ARRAY_SIZE(SEQ_NT36672C_217));
	DSI_WRITE(SEQ_NT36672C_218, ARRAY_SIZE(SEQ_NT36672C_218));
	DSI_WRITE(SEQ_NT36672C_219, ARRAY_SIZE(SEQ_NT36672C_219));
	DSI_WRITE(SEQ_NT36672C_220, ARRAY_SIZE(SEQ_NT36672C_220));
	DSI_WRITE(SEQ_NT36672C_221, ARRAY_SIZE(SEQ_NT36672C_221));
	DSI_WRITE(SEQ_NT36672C_222, ARRAY_SIZE(SEQ_NT36672C_222));
	DSI_WRITE(SEQ_NT36672C_223, ARRAY_SIZE(SEQ_NT36672C_223));
	DSI_WRITE(SEQ_NT36672C_224, ARRAY_SIZE(SEQ_NT36672C_224));
	DSI_WRITE(SEQ_NT36672C_225, ARRAY_SIZE(SEQ_NT36672C_225));
	DSI_WRITE(SEQ_NT36672C_226, ARRAY_SIZE(SEQ_NT36672C_226));
	DSI_WRITE(SEQ_NT36672C_227, ARRAY_SIZE(SEQ_NT36672C_227));
	DSI_WRITE(SEQ_NT36672C_228, ARRAY_SIZE(SEQ_NT36672C_228));
	DSI_WRITE(SEQ_NT36672C_229, ARRAY_SIZE(SEQ_NT36672C_229));
	DSI_WRITE(SEQ_NT36672C_230, ARRAY_SIZE(SEQ_NT36672C_230));
	DSI_WRITE(SEQ_NT36672C_231, ARRAY_SIZE(SEQ_NT36672C_231));
	DSI_WRITE(SEQ_NT36672C_232, ARRAY_SIZE(SEQ_NT36672C_232));
	DSI_WRITE(SEQ_NT36672C_233, ARRAY_SIZE(SEQ_NT36672C_233));
	DSI_WRITE(SEQ_NT36672C_234, ARRAY_SIZE(SEQ_NT36672C_234));
	DSI_WRITE(SEQ_NT36672C_235, ARRAY_SIZE(SEQ_NT36672C_235));
	DSI_WRITE(SEQ_NT36672C_236, ARRAY_SIZE(SEQ_NT36672C_236));
	DSI_WRITE(SEQ_NT36672C_237, ARRAY_SIZE(SEQ_NT36672C_237));
	DSI_WRITE(SEQ_NT36672C_238, ARRAY_SIZE(SEQ_NT36672C_238));
	DSI_WRITE(SEQ_NT36672C_239, ARRAY_SIZE(SEQ_NT36672C_239));
	DSI_WRITE(SEQ_NT36672C_240, ARRAY_SIZE(SEQ_NT36672C_240));
	DSI_WRITE(SEQ_NT36672C_241, ARRAY_SIZE(SEQ_NT36672C_241));
	DSI_WRITE(SEQ_NT36672C_242, ARRAY_SIZE(SEQ_NT36672C_242));
	DSI_WRITE(SEQ_NT36672C_243, ARRAY_SIZE(SEQ_NT36672C_243));
	DSI_WRITE(SEQ_NT36672C_244, ARRAY_SIZE(SEQ_NT36672C_244));
	DSI_WRITE(SEQ_NT36672C_245, ARRAY_SIZE(SEQ_NT36672C_245));
	DSI_WRITE(SEQ_NT36672C_246, ARRAY_SIZE(SEQ_NT36672C_246));
	DSI_WRITE(SEQ_NT36672C_247, ARRAY_SIZE(SEQ_NT36672C_247));
	DSI_WRITE(SEQ_NT36672C_248, ARRAY_SIZE(SEQ_NT36672C_248));
	DSI_WRITE(SEQ_NT36672C_249, ARRAY_SIZE(SEQ_NT36672C_249));
	DSI_WRITE(SEQ_NT36672C_250, ARRAY_SIZE(SEQ_NT36672C_250));
	DSI_WRITE(SEQ_NT36672C_251, ARRAY_SIZE(SEQ_NT36672C_251));
	DSI_WRITE(SEQ_NT36672C_252, ARRAY_SIZE(SEQ_NT36672C_252));
	DSI_WRITE(SEQ_NT36672C_253, ARRAY_SIZE(SEQ_NT36672C_253));
	DSI_WRITE(SEQ_NT36672C_254, ARRAY_SIZE(SEQ_NT36672C_254));
	DSI_WRITE(SEQ_NT36672C_255, ARRAY_SIZE(SEQ_NT36672C_255));
	DSI_WRITE(SEQ_NT36672C_256, ARRAY_SIZE(SEQ_NT36672C_256));
	DSI_WRITE(SEQ_NT36672C_257, ARRAY_SIZE(SEQ_NT36672C_257));
	DSI_WRITE(SEQ_NT36672C_258, ARRAY_SIZE(SEQ_NT36672C_258));
	DSI_WRITE(SEQ_NT36672C_259, ARRAY_SIZE(SEQ_NT36672C_259));
	DSI_WRITE(SEQ_NT36672C_260, ARRAY_SIZE(SEQ_NT36672C_260));
	DSI_WRITE(SEQ_NT36672C_261, ARRAY_SIZE(SEQ_NT36672C_261));
	DSI_WRITE(SEQ_NT36672C_262, ARRAY_SIZE(SEQ_NT36672C_262));
	DSI_WRITE(SEQ_NT36672C_263, ARRAY_SIZE(SEQ_NT36672C_263));
	DSI_WRITE(SEQ_NT36672C_264, ARRAY_SIZE(SEQ_NT36672C_264));
	DSI_WRITE(SEQ_NT36672C_265, ARRAY_SIZE(SEQ_NT36672C_265));
	DSI_WRITE(SEQ_NT36672C_266, ARRAY_SIZE(SEQ_NT36672C_266));
	DSI_WRITE(SEQ_NT36672C_267, ARRAY_SIZE(SEQ_NT36672C_267));
	DSI_WRITE(SEQ_NT36672C_268, ARRAY_SIZE(SEQ_NT36672C_268));
	DSI_WRITE(SEQ_NT36672C_269, ARRAY_SIZE(SEQ_NT36672C_269));
	DSI_WRITE(SEQ_NT36672C_270, ARRAY_SIZE(SEQ_NT36672C_270));
	DSI_WRITE(SEQ_NT36672C_271, ARRAY_SIZE(SEQ_NT36672C_271));
	DSI_WRITE(SEQ_NT36672C_272, ARRAY_SIZE(SEQ_NT36672C_272));
	DSI_WRITE(SEQ_NT36672C_273, ARRAY_SIZE(SEQ_NT36672C_273));
	DSI_WRITE(SEQ_NT36672C_274, ARRAY_SIZE(SEQ_NT36672C_274));
	DSI_WRITE(SEQ_NT36672C_275, ARRAY_SIZE(SEQ_NT36672C_275));
	DSI_WRITE(SEQ_NT36672C_276, ARRAY_SIZE(SEQ_NT36672C_276));
	DSI_WRITE(SEQ_NT36672C_277, ARRAY_SIZE(SEQ_NT36672C_277));
	DSI_WRITE(SEQ_NT36672C_278, ARRAY_SIZE(SEQ_NT36672C_278));
	DSI_WRITE(SEQ_NT36672C_279, ARRAY_SIZE(SEQ_NT36672C_279));
	DSI_WRITE(SEQ_NT36672C_280, ARRAY_SIZE(SEQ_NT36672C_280));
	DSI_WRITE(SEQ_NT36672C_281, ARRAY_SIZE(SEQ_NT36672C_281));
	DSI_WRITE(SEQ_NT36672C_282, ARRAY_SIZE(SEQ_NT36672C_282));
	DSI_WRITE(SEQ_NT36672C_283, ARRAY_SIZE(SEQ_NT36672C_283));
	DSI_WRITE(SEQ_NT36672C_284, ARRAY_SIZE(SEQ_NT36672C_284));
	DSI_WRITE(SEQ_NT36672C_285, ARRAY_SIZE(SEQ_NT36672C_285));
	DSI_WRITE(SEQ_NT36672C_286, ARRAY_SIZE(SEQ_NT36672C_286));
	DSI_WRITE(SEQ_NT36672C_287, ARRAY_SIZE(SEQ_NT36672C_287));
	DSI_WRITE(SEQ_NT36672C_288, ARRAY_SIZE(SEQ_NT36672C_288));
	DSI_WRITE(SEQ_NT36672C_289, ARRAY_SIZE(SEQ_NT36672C_289));
	DSI_WRITE(SEQ_NT36672C_290, ARRAY_SIZE(SEQ_NT36672C_290));
	DSI_WRITE(SEQ_NT36672C_291, ARRAY_SIZE(SEQ_NT36672C_291));
	DSI_WRITE(SEQ_NT36672C_292, ARRAY_SIZE(SEQ_NT36672C_292));
	DSI_WRITE(SEQ_NT36672C_293, ARRAY_SIZE(SEQ_NT36672C_293));
	DSI_WRITE(SEQ_NT36672C_294, ARRAY_SIZE(SEQ_NT36672C_294));
	DSI_WRITE(SEQ_NT36672C_295, ARRAY_SIZE(SEQ_NT36672C_295));
	DSI_WRITE(SEQ_NT36672C_296, ARRAY_SIZE(SEQ_NT36672C_296));
	DSI_WRITE(SEQ_NT36672C_297, ARRAY_SIZE(SEQ_NT36672C_297));
	DSI_WRITE(SEQ_NT36672C_298, ARRAY_SIZE(SEQ_NT36672C_298));
	DSI_WRITE(SEQ_NT36672C_299, ARRAY_SIZE(SEQ_NT36672C_299));
	DSI_WRITE(SEQ_NT36672C_300, ARRAY_SIZE(SEQ_NT36672C_300));
	DSI_WRITE(SEQ_NT36672C_301, ARRAY_SIZE(SEQ_NT36672C_301));
	DSI_WRITE(SEQ_NT36672C_302, ARRAY_SIZE(SEQ_NT36672C_302));
	DSI_WRITE(SEQ_NT36672C_303, ARRAY_SIZE(SEQ_NT36672C_303));
	DSI_WRITE(SEQ_NT36672C_304, ARRAY_SIZE(SEQ_NT36672C_304));
	DSI_WRITE(SEQ_NT36672C_305, ARRAY_SIZE(SEQ_NT36672C_305));

	DSI_WRITE(SEQ_DEFAULT_BRIGHTNESS, ARRAY_SIZE(SEQ_DEFAULT_BRIGHTNESS));

	DSI_WRITE(SEQ_SLEEP_OUT, ARRAY_SIZE(SEQ_SLEEP_OUT));

	msleep(120);
	dev_info(&lcd->ld->dev, "%s: --\n", __func__);

	return ret;
}

static int fb_notifier_callback(struct notifier_block *self,
				unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	struct lcd_info *lcd = NULL;
	int fb_blank;

	switch (event) {
	case FB_EVENT_BLANK:
		break;
	default:
		return NOTIFY_DONE;
	}

	lcd = container_of(self, struct lcd_info, fb_notifier);

	fb_blank = *(int *)evdata->data;

	dev_info(&lcd->ld->dev, "%s: %d\n", __func__, fb_blank);

	if (evdata->info->node)
		return NOTIFY_DONE;

	if (fb_blank == FB_BLANK_UNBLANK) {
		mutex_lock(&lcd->lock);
		nt36672a_displayon_late(lcd);
		mutex_unlock(&lcd->lock);

		dsim_panel_set_brightness(lcd, 1);
	}

	return NOTIFY_DONE;
}

static int lm36274_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	struct lcd_info *lcd = NULL;
	int ret = 0;

	if (id && id->driver_data)
		lcd = (struct lcd_info *)id->driver_data;

	if (!lcd) {
		dsim_err("%s: failed to find driver_data for lcd\n", __func__);
		ret = -EINVAL;
		goto exit;
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_info(&lcd->ld->dev, "%s: need I2C_FUNC_I2C\n", __func__);
		ret = -ENODEV;
		goto exit;
	}

	i2c_set_clientdata(client, lcd);

	lcd->blic_client = client;

	dev_info(&lcd->ld->dev, "%s: %s %s\n", __func__, dev_name(&client->adapter->dev), of_node_full_name(client->dev.of_node));

exit:
	return ret;
}

static struct i2c_device_id blic_i2c_id[] = {
	{"lm36274", 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, blic_i2c_id);

static const struct of_device_id blic_i2c_dt_ids[] = {
	{ .compatible = "i2c,lm36274" },
	{ }
};

MODULE_DEVICE_TABLE(of, blic_i2c_dt_ids);

static struct i2c_driver blic_i2c_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "lm36274",
		.of_match_table	= of_match_ptr(blic_i2c_dt_ids),
	},
	.id_table = blic_i2c_id,
	.probe = lm36274_probe,
};

static int nt36672a_probe(struct lcd_info *lcd)
{
	int ret = 0;

	dev_info(&lcd->ld->dev, "+ %s\n", __func__);

	lcd->bd->props.max_brightness = EXTEND_BRIGHTNESS;
	lcd->bd->props.brightness = UI_DEFAULT_BRIGHTNESS;

	lcd->state = PANEL_STATE_RESUMED;
	lcd->lux = -1;

	ret = nt36672a_read_init_info(lcd);
	if (ret < 0)
		dev_info(&lcd->ld->dev, "%s: failed to init information\n", __func__);

	lcd->fb_notifier.notifier_call = fb_notifier_callback;
	decon_register_notifier(&lcd->fb_notifier);

	blic_i2c_id->driver_data = (kernel_ulong_t)lcd;
	i2c_add_driver(&blic_i2c_driver);

	dev_info(&lcd->ld->dev, "- %s\n", __func__);

	return 0;
}

static ssize_t lcd_type_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);

	sprintf(buf, "TIA_%02X%02X%02X\n", lcd->id_info.id[0], lcd->id_info.id[1], lcd->id_info.id[2]);

	return strlen(buf);
}

static ssize_t window_type_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);

	sprintf(buf, "%02x %02x %02x\n", lcd->id_info.id[0], lcd->id_info.id[1], lcd->id_info.id[2]);

	return strlen(buf);
}

static ssize_t lux_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);

	sprintf(buf, "%d\n", lcd->lux);

	return strlen(buf);
}

static ssize_t lux_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	int value;
	int rc;

	rc = kstrtoint(buf, 0, &value);
	if (rc < 0)
		return rc;

	if (lcd->lux != value) {
		mutex_lock(&lcd->lock);
		lcd->lux = value;
		mutex_unlock(&lcd->lock);
	}

	return size;
}

static DEVICE_ATTR(lcd_type, 0444, lcd_type_show, NULL);
static DEVICE_ATTR(window_type, 0444, window_type_show, NULL);
static DEVICE_ATTR(lux, 0644, lux_show, lux_store);

static struct attribute *lcd_sysfs_attributes[] = {
	&dev_attr_lcd_type.attr,
	&dev_attr_window_type.attr,
	&dev_attr_lux.attr,
	NULL,
};

static const struct attribute_group lcd_sysfs_attr_group = {
	.attrs = lcd_sysfs_attributes,
};

static void lcd_init_sysfs(struct lcd_info *lcd)
{
	int ret = 0;
	struct i2c_client *clients[] = {lcd->blic_client, NULL};

	ret = sysfs_create_group(&lcd->ld->dev.kobj, &lcd_sysfs_attr_group);
	if (ret < 0)
		dev_info(&lcd->ld->dev, "failed to add lcd sysfs\n");

	init_debugfs_backlight(lcd->bd, brightness_table, clients);

	init_debugfs_param("blic_init", &LM36274_INIT, U8_MAX, ARRAY_SIZE(LM36274_INIT), 2);
}

static int dsim_panel_probe(struct dsim_device *dsim)
{
	int ret = 0;
	struct lcd_info *lcd;

	dsim->priv.par = lcd = kzalloc(sizeof(struct lcd_info), GFP_KERNEL);
	if (!lcd) {
		pr_err("%s: failed to allocate for lcd\n", __func__);
		ret = -ENOMEM;
		goto probe_err;
	}

	lcd->ld = exynos_lcd_device_register("panel", dsim->dev, lcd, NULL);
	if (IS_ERR(lcd->ld)) {
		pr_err("%s: failed to register lcd device\n", __func__);
		ret = PTR_ERR(lcd->ld);
		goto probe_err;
	}

	lcd->bd = exynos_backlight_device_register("panel", dsim->dev, lcd, &panel_backlight_ops, NULL);
	if (IS_ERR(lcd->bd)) {
		pr_err("%s: failed to register backlight device\n", __func__);
		ret = PTR_ERR(lcd->bd);
		goto probe_err;
	}

	mutex_init(&lcd->lock);

	lcd->dsim = dsim;
	ret = nt36672a_probe(lcd);
	if (ret < 0)
		dev_info(&lcd->ld->dev, "%s: failed to probe panel\n", __func__);

	lcd_init_sysfs(lcd);

	dev_info(&lcd->ld->dev, "%s: %s: done\n", kbasename(__FILE__), __func__);
probe_err:
	return ret;
}

static int dsim_panel_resume_early(struct dsim_device *dsim)
{
	struct lcd_info *lcd = dsim->priv.par;

	dev_info(&lcd->ld->dev, "+ %s\n", __func__);

	lm36274_array_write(lcd->blic_client, LM36274_INIT, ARRAY_SIZE(LM36274_INIT));

	dev_info(&lcd->ld->dev, "- %s: %d, %d\n", __func__, lcd->state, lcd->connected);

	return 0;
}

static int dsim_panel_displayon(struct dsim_device *dsim)
{
	struct lcd_info *lcd = dsim->priv.par;

	dev_info(&lcd->ld->dev, "+ %s: %d\n", __func__, lcd->state);

#if defined(CONFIG_SEC_FACTORY)
	nt36672a_read_id(lcd);
#endif

	if (lcd->state == PANEL_STATE_SUSPENED) {
		if (lcdtype == LDI_2ND_ID)
			nt36672c_init(lcd);
		else
			nt36672a_init(lcd);
	}

	mutex_lock(&lcd->lock);
	lcd->state = PANEL_STATE_RESUMED;
	mutex_unlock(&lcd->lock);

	dev_info(&lcd->ld->dev, "- %s: %d, %d\n", __func__, lcd->state, lcd->connected);

	return 0;
}

static int dsim_panel_suspend(struct dsim_device *dsim)
{
	struct lcd_info *lcd = dsim->priv.par;

	dev_info(&lcd->ld->dev, "+ %s: %d\n", __func__, lcd->state);

	if (lcd->state == PANEL_STATE_SUSPENED)
		goto exit;

	mutex_lock(&lcd->lock);
	lcd->state = PANEL_STATE_SUSPENDING;
	mutex_unlock(&lcd->lock);

	nt36672a_exit(lcd);

	mutex_lock(&lcd->lock);
	lcd->state = PANEL_STATE_SUSPENED;
	mutex_unlock(&lcd->lock);

	dev_info(&lcd->ld->dev, "- %s: %d, %d\n", __func__, lcd->state, lcd->connected);

exit:
	return 0;
}

struct dsim_lcd_driver nt36672a_mipi_lcd_driver = {
	.name		= "nt36672a",
	.probe		= dsim_panel_probe,
	.resume_early	= dsim_panel_resume_early,
	.displayon	= dsim_panel_displayon,
	.suspend	= dsim_panel_suspend,
};
__XX_ADD_LCD_DRIVER(nt36672a_mipi_lcd_driver);

static void panel_conn_uevent(struct lcd_info *lcd)
{
	char *uevent_conn_str[3] = {"CONNECTOR_NAME=UB_CONNECT", "CONNECTOR_TYPE=HIGH_LEVEL", NULL};

	if (!IS_ENABLED(CONFIG_SEC_FACTORY))
		return;

	if (!lcd->conn_det_enable)
		return;

	kobject_uevent_env(&lcd->ld->dev.kobj, KOBJ_CHANGE, uevent_conn_str);

	dev_info(&lcd->ld->dev, "%s: %s, %s\n", __func__, uevent_conn_str[0], uevent_conn_str[1]);
}

static void panel_conn_work(struct work_struct *work)
{
	struct lcd_info *lcd = container_of(work, struct lcd_info, conn_work);

	dev_info(&lcd->ld->dev, "%s\n", __func__);

	panel_conn_uevent(lcd);
}

static irqreturn_t panel_conn_det_handler(int irq, void *dev_id)
{
	struct lcd_info *lcd = (struct lcd_info *)dev_id;

	dev_info(&lcd->ld->dev, "%s\n", __func__);

	queue_work(lcd->conn_workqueue, &lcd->conn_work);

	lcd->conn_det_count++;

	return IRQ_HANDLED;
}

static ssize_t conn_det_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	int gpio_active = of_gpio_get_active("gpio_con");

	if (gpio_active < 0)
		sprintf(buf, "%d\n", -1);
	else
		sprintf(buf, "%s\n", gpio_active ? "disconnected" : "connected");

	dev_info(&lcd->ld->dev, "%s: %s\n", __func__, buf);

	return strlen(buf);
}

static ssize_t conn_det_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	unsigned int value;
	int rc;
	int gpio_active = of_gpio_get_active("gpio_con");

	rc = kstrtouint(buf, 0, &value);
	if (rc < 0)
		return rc;

	if (gpio_active < 0)
		return -EINVAL;

	if (lcd->conn_det_enable != value) {
		dev_info(&lcd->ld->dev, "%s: %u, %u\n", __func__, lcd->conn_det_enable, value);

		mutex_lock(&lcd->lock);
		lcd->conn_det_enable = value;
		mutex_unlock(&lcd->lock);

		dev_info(&lcd->ld->dev, "%s: %s\n", __func__, gpio_active ? "disconnected" : "connected");
		if (lcd->conn_det_enable && gpio_active)
			panel_conn_uevent(lcd);
	}

	return size;
}

static DEVICE_ATTR(conn_det, 0644, conn_det_show, conn_det_store);

static void panel_conn_register(struct lcd_info *lcd)
{
	struct decon_device *decon = get_decon_drvdata(0);
	struct abd_protect *abd = &decon->abd;
	int gpio = 0, gpio_active = 0;

	if (!decon) {
		dev_info(&lcd->ld->dev, "%s: decon is invalid\n", __func__);
		return;
	}

	if (!lcd->connected) {
		dev_info(&lcd->ld->dev, "%s: lcd connected: %d\n", __func__, lcd->connected);
		return;
	}

	gpio = of_get_gpio_with_name("gpio_con");
	if (gpio < 0) {
		dev_info(&lcd->ld->dev, "%s: gpio_con is %d\n", __func__, gpio);
		return;
	}

	gpio_active = of_gpio_get_active("gpio_con");
	if (gpio_active) {
		dev_info(&lcd->ld->dev, "%s: gpio_con_active is %d\n", __func__, gpio_active);
		return;
	}

	INIT_WORK(&lcd->conn_work, panel_conn_work);

	lcd->conn_workqueue = create_singlethread_workqueue("lcd_conn_workqueue");
	if (!lcd->conn_workqueue) {
		dev_info(&lcd->ld->dev, "%s: create_singlethread_workqueue fail\n", __func__);
		return;
	}

	decon_abd_pin_register_handler(abd, gpio_to_irq(gpio), panel_conn_det_handler, lcd);

	if (!IS_ENABLED(CONFIG_SEC_FACTORY))
		return;

	decon_abd_con_register(abd);
	device_create_file(&lcd->ld->dev, &dev_attr_conn_det);
}

static int __init panel_conn_init(void)
{
	struct lcd_info *lcd = NULL;
	struct dsim_device *pdata = NULL;
	struct platform_device *pdev = NULL;

	pdev = of_find_dsim_platform_device();
	if (!pdev) {
		dsim_info("%s: of_find_dsim_platform_device fail\n", __func__);
		return 0;
	}

	pdata = platform_get_drvdata(pdev);
	if (!pdata) {
		dsim_info("%s: platform_get_drvdata fail\n", __func__);
		return 0;
	}

	if (!pdata->panel_ops) {
		dsim_info("%s: panel_ops invalid\n", __func__);
		return 0;
	}

	if (pdata->panel_ops != this_driver)
		return 0;

	lcd = pdata->priv.par;
	if (!lcd) {
		dsim_info("lcd_info invalid\n");
		return 0;
	}

	if (unlikely(!lcd->conn_init_done)) {
		lcd->conn_init_done = 1;
		panel_conn_register(lcd);
	}

	dev_info(&lcd->ld->dev, "%s: %s: done\n", kbasename(__FILE__), __func__);

	return 0;
}
late_initcall_sync(panel_conn_init);

